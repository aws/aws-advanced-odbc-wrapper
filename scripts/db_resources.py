#!/usr/bin/env python3
#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Unified Aurora resource management for CI integration tests.

Consolidates cluster creation, endpoint setup, secrets, security group,
and IAM user provisioning into two commands: `create` and `destroy`.

Usage:
    python aurora_resources.py create  --cluster-id ID --engine ENGINE ...
    python aurora_resources.py destroy --cluster-id ID --region REGION ...
"""

import argparse
import json
import os
import subprocess
import sys
import time
import urllib.request


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def run(cmd, check=True, capture=True):
    """Run a shell command, return stdout."""
    print(f"+ {' '.join(cmd)}", flush=True)
    result = subprocess.run(cmd, capture_output=capture, text=True)
    if check and result.returncode != 0:
        stderr = result.stderr if capture else ""
        raise RuntimeError(f"Command failed ({result.returncode}): {stderr}")
    return result.stdout.strip() if capture else ""


def aws(*args, check=True):
    return run(["aws", *args], check=check)


def get_public_ip():
    return urllib.request.urlopen("http://checkip.amazonaws.com").read().decode().strip()


def set_github_env(key, value, mask=False):
    """Write a key=value pair to $GITHUB_ENV so subsequent steps can use it."""
    if mask:
        print(f"::add-mask::{value}")
    env_file = os.environ.get("GITHUB_ENV")
    if env_file:
        with open(env_file, "a") as f:
            f.write(f"{key}={value}\n")
    print(f"  {key}={value}")


def set_github_output(key, value):
    """Write a key=value pair to $GITHUB_OUTPUT."""
    output_file = os.environ.get("GITHUB_OUTPUT")
    if output_file:
        with open(output_file, "a") as f:
            f.write(f"{key}={value}\n")
    print(f"  output {key}={value}")


# ---------------------------------------------------------------------------
# Engine version resolution
# ---------------------------------------------------------------------------

def resolve_engine_version(engine, version, limitless=False):
    if version != "latest":
        return version
    query_filter = "contains" if limitless else "!contains"
    raw = aws(
        "rds", "describe-db-engine-versions",
        "--engine", engine,
        "--query", f"DBEngineVersions[?{query_filter}(EngineVersion, '-limitless')].EngineVersion",
        "--output", "json",
    )
    versions = sorted(json.loads(raw), reverse=False)
    resolved = versions[-1] if versions else version
    print(f"  Resolved engine version: {resolved}")
    return resolved


# ---------------------------------------------------------------------------
# Security group helpers
# ---------------------------------------------------------------------------

def get_cluster_sg(cluster_id, region):
    raw = aws("rds", "describe-db-clusters",
              "--db-cluster-identifier", cluster_id,
              "--region", region)
    info = json.loads(raw)
    return info["DBClusters"][0]["VpcSecurityGroups"][0]["VpcSecurityGroupId"]


def add_ip_to_sg(ip, cluster_id, region):
    sg = get_cluster_sg(cluster_id, region)
    cidr = f"{ip}/32"
    aws("ec2", "authorize-security-group-ingress",
        "--group-id", sg, "--protocol", "tcp",
        "--cidr", cidr, "--port", "0-65535",
        "--region", region, check=False)
    print(f"  Allowed {cidr} on SG {sg}, sleeping 60s for propagation...")
    time.sleep(60)


def remove_ip_from_sg(ip, cluster_id, region):
    try:
        sg = get_cluster_sg(cluster_id, region)
    except Exception:
        print("  Could not resolve SG — cluster may already be deleted")
        return
    cidr = f"{ip}/32"
    aws("ec2", "revoke-security-group-ingress",
        "--group-id", sg, "--protocol", "tcp",
        "--cidr", cidr, "--port", "0-65535",
        "--region", region, check=False)
    print(f"  Removed {cidr} from SG {sg}")


# ---------------------------------------------------------------------------
# Cluster creation
# ---------------------------------------------------------------------------

def create_aurora_cluster(args):
    version = resolve_engine_version(args.engine, args.engine_version)

    print("Creating Aurora RDS cluster...")
    aws("rds", "create-db-cluster",
        "--db-cluster-identifier", args.cluster_id,
        "--database-name", args.database,
        "--master-username", args.username,
        "--master-user-password", args.password,
        "--source-region", args.region,
        "--enable-iam-database-authentication",
        "--engine", args.engine,
        "--engine-version", version,
        "--storage-encrypted",
        "--tags", "Key=env,Value=test-runner")

    for i in range(1, args.num_instances + 1):
        aws("rds", "create-db-instance",
            "--db-cluster-identifier", args.cluster_id,
            "--db-instance-identifier", f"{args.cluster_id}-{i}",
            "--db-instance-class", "db.r5.large",
            "--engine", args.engine,
            "--engine-version", version,
            "--publicly-accessible",
            "--tags", "Key=env,Value=test-runner")

    print("Waiting for instances to become available...")
    aws("rds", "wait", "db-instance-available",
        "--filters", f"Name=db-cluster-id,Values={args.cluster_id}")


def create_limitless_cluster(args):
    version = resolve_engine_version(args.engine, args.engine_version, limitless=True)

    print("Creating Limitless RDS cluster...")
    aws("rds", "create-db-cluster",
        "--cluster-scalability-type", "limitless",
        "--db-cluster-identifier", args.cluster_id,
        "--master-username", args.username,
        "--master-user-password", args.password,
        "--region", args.region,
        "--engine", args.engine,
        "--engine-version", version,
        "--enable-cloudwatch-logs-export", "postgresql",
        "--enable-iam-database-authentication",
        "--enable-performance-insights",
        "--monitoring-interval", "5",
        "--performance-insights-retention-period", "31",
        "--monitoring-role-arn", args.monitoring_role_arn,
        "--storage-type", "aurora-iopt1",
        "--tags", "Key=env,Value=test-runner")

    print("Creating Limitless shard group...")
    aws("rds", "create-db-shard-group",
        "--db-cluster-identifier", args.cluster_id,
        "--db-shard-group-identifier", args.shard_id,
        "--min-acu", "28.0",
        "--max-acu", "601.0",
        "--publicly-accessible",
        "--tags", "Key=env,Value=test-runner")

    # Wait with retries
    for attempt in range(3):
        print(f"  Waiting for cluster availability (attempt {attempt + 1}/3)...")
        result = subprocess.run(
            ["aws", "rds", "wait", "db-cluster-available",
             "--db-cluster-identifier", args.cluster_id],
            capture_output=True, text=True,
        )
        if result.returncode == 0:
            print("  Cluster available.")
            break
        if attempt < 2:
            time.sleep(30)
    else:
        raise RuntimeError("Cluster did not become available after retries")


def create_custom_endpoint(cluster_id, endpoint_id, num_instances):
    half = num_instances // 2
    members = [f"{cluster_id}-{i}".lower() for i in range(1, half + 1)]

    aws("rds", "wait", "db-cluster-available",
        "--db-cluster-identifier", cluster_id)

    print(f"Creating custom endpoint {endpoint_id} with static members {members}...")
    aws("rds", "create-db-cluster-endpoint",
        "--db-cluster-endpoint-identifier", endpoint_id,
        "--db-cluster-identifier", cluster_id,
        "--endpoint-type", "any",
        "--static-members", *members)

    time.sleep(30)
    aws("rds", "wait", "db-cluster-available",
        "--db-cluster-identifier", cluster_id)


def get_cluster_endpoint(cluster_id):
    return aws("rds", "describe-db-clusters",
               "--db-cluster-identifier", cluster_id,
               "--query", "DBClusters[0].Endpoint",
               "--output", "text")


def create_secrets(username, password, engine, cluster_endpoint):
    secret_name = f"AWS-ODBC-Tests-{cluster_endpoint}"
    secret_value = json.dumps({
        "username": username,
        "password": password,
        "engine": engine,
        "host": cluster_endpoint,
    })
    raw = aws("secretsmanager", "create-secret",
              "--name", secret_name,
              "--description", "Secrets created by GH actions for DB auth",
              "--secret-string", secret_value)
    return json.loads(raw)["ARN"]


def setup_pg_iam_user(endpoint, port, database, username, password, iam_user):
    connstr = f"postgresql://{username}:{password}@{endpoint}:{port}/{database}"
    run(["psql", connstr,
         "--command", f"CREATE USER {iam_user};",
         "--command", f"GRANT rds_iam TO {iam_user};",
         "--command", "\\du"], check=False)


def setup_mysql_iam_user(endpoint, database, username, password, iam_user,
                         extra_user=None, extra_password=None):
    sql = (
        f"CREATE USER '{iam_user}' IDENTIFIED WITH AWSAuthenticationPlugin AS 'RDS'; "
        f"GRANT ALL ON {database}.* TO '{iam_user}'@'%'; "
        f"FLUSH PRIVILEGES;"
    )
    run(["mysql", "-h", endpoint, "-P", "3306", "-D", database,
         "-u", username, f"-p{password}", f"--execute={sql}"], check=False)

    if extra_user and extra_password:
        sql2 = (
            f"CREATE USER '{extra_user}' IDENTIFIED WITH caching_sha2_password BY '{extra_password}'; "
            f"GRANT ALL ON {database}.* TO '{extra_user}'@'%'; "
            f"FLUSH PRIVILEGES;"
        )
        run(["mysql", "-h", endpoint, "-P", "3306", "-D", database,
             "-u", username, f"-p{password}", f"--execute={sql2}"], check=False)


# ---------------------------------------------------------------------------
# Cluster destruction
# ---------------------------------------------------------------------------

def delete_custom_endpoint(endpoint_id, cluster_id):
    print(f"Deleting custom endpoint {endpoint_id}...")
    aws("rds", "delete-db-cluster-endpoint",
        "--db-cluster-endpoint-identifier", endpoint_id, check=False)
    aws("rds", "wait", "db-cluster-available",
        "--db-cluster-identifier", cluster_id, check=False)


def delete_aurora_cluster(cluster_id, num_instances):
    print(f"Deleting Aurora cluster {cluster_id} ({num_instances} instances)...")
    for i in range(1, num_instances + 1):
        aws("rds", "delete-db-instance", "--skip-final-snapshot",
            "--db-instance-identifier", f"{cluster_id}-{i}", check=False)
    aws("rds", "delete-db-cluster",
        "--db-cluster-identifier", cluster_id,
        "--skip-final-snapshot", check=False)


def delete_limitless_cluster(cluster_id, shard_id):
    max_retries = 5
    for attempt in range(max_retries):
        result = subprocess.run(
            ["aws", "rds", "delete-db-shard-group",
             "--db-shard-group-identifier", shard_id],
            capture_output=True, text=True,
        )
        if result.returncode == 0 or "already being deleted" in result.stderr:
            print(f"  Shard {shard_id} deletion initiated.")
            break
        if attempt < max_retries - 1:
            print(f"  Retrying shard deletion ({max_retries - attempt - 1} left)...")
            time.sleep(30)
    else:
        print(f"  WARNING: Failed to delete shard {shard_id} after {max_retries} attempts")

    for attempt in range(max_retries):
        result = subprocess.run(
            ["aws", "rds", "delete-db-cluster",
             "--db-cluster-identifier", cluster_id, "--skip-final-snapshot"],
            capture_output=True, text=True,
        )
        if result.returncode == 0:
            print(f"  Cluster {cluster_id} deletion initiated.")
            break
        if attempt < max_retries - 1:
            print(f"  Retrying cluster deletion ({max_retries - attempt - 1} left)...")
            time.sleep(30)
    else:
        print(f"  WARNING: Failed to delete cluster {cluster_id} after {max_retries} attempts")


def delete_secrets(secrets_arn):
    if secrets_arn:
        print(f"Deleting secrets {secrets_arn}...")
        aws("secretsmanager", "delete-secret", "--secret-id", secrets_arn, check=False)


# ---------------------------------------------------------------------------
# CLI: create
# ---------------------------------------------------------------------------

def cmd_create(args):
    """Create all Aurora resources and export env vars for subsequent steps."""
    is_limitless = args.limitless

    # 1. Create cluster
    if is_limitless:
        create_limitless_cluster(args)
    else:
        create_aurora_cluster(args)

    # 2. Get public IP and add to SG (unix-style; Windows PS1 does this internally)
    ip = get_public_ip()
    set_github_output("public_ip", ip)
    add_ip_to_sg(ip, args.cluster_id, args.region)

    # 3. Custom endpoint (non-limitless only)
    if not is_limitless and args.custom_endpoint_id:
        create_custom_endpoint(args.cluster_id, args.custom_endpoint_id, args.num_instances)

    # 4. Get cluster endpoint
    endpoint = get_cluster_endpoint(args.cluster_id)
    set_github_env("AURORA_CLUSTER_ENDPOINT", endpoint)

    # 5. Create secrets
    secrets_arn = create_secrets(args.username, args.password, args.engine, endpoint)
    set_github_env("AURORA_CLUSTER_SECRETS_ARN", secrets_arn, mask=True)

    # 6. Setup IAM DB user
    port = "5432" if "postgresql" in args.engine else "3306"
    if "postgresql" in args.engine:
        setup_pg_iam_user(endpoint, port, args.database, args.username, args.password, args.iam_user)
    elif "mysql" in args.engine:
        setup_mysql_iam_user(endpoint, args.database, args.username, args.password, args.iam_user,
                             extra_user=args.extra_mysql_user, extra_password=args.extra_mysql_password)

    # 7. Describe cluster for logs
    aws("rds", "describe-db-clusters", "--db-cluster-identifier", args.cluster_id, check=False)

    print("\nAurora resources created successfully.")


# ---------------------------------------------------------------------------
# CLI: destroy
# ---------------------------------------------------------------------------

def cmd_destroy(args):
    """Tear down all Aurora resources. Best-effort — never raises."""
    ip = args.public_ip or get_public_ip()

    # Custom endpoint
    if args.custom_endpoint_id:
        try:
            delete_custom_endpoint(args.custom_endpoint_id, args.cluster_id)
        except Exception as e:
            print(f"  WARNING: custom endpoint cleanup failed: {e}")

    # Cluster
    try:
        if args.limitless:
            delete_limitless_cluster(args.cluster_id, args.shard_id)
        else:
            delete_aurora_cluster(args.cluster_id, args.num_instances)
    except Exception as e:
        print(f"  WARNING: cluster cleanup failed: {e}")

    # Secrets
    try:
        delete_secrets(args.secrets_arn)
    except Exception as e:
        print(f"  WARNING: secrets cleanup failed: {e}")

    # Security group
    try:
        remove_ip_from_sg(ip, args.cluster_id, args.region)
    except Exception as e:
        print(f"  WARNING: SG cleanup failed: {e}")

    print("\nAurora resource cleanup complete.")


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def build_parser():
    parser = argparse.ArgumentParser(description="Aurora resource management for CI")
    sub = parser.add_subparsers(dest="command", required=True)

    # -- create --
    c = sub.add_parser("create", help="Provision DB cluster and all supporting resources")
    c.add_argument("--cluster-id", required=True)
    c.add_argument("--engine", required=True, help="e.g. aurora-postgresql, aurora-mysql")
    c.add_argument("--engine-version", default="latest")
    c.add_argument("--database", required=True)
    c.add_argument("--username", required=True)
    c.add_argument("--password", required=True)
    c.add_argument("--region", required=True)
    c.add_argument("--num-instances", type=int, default=5)
    c.add_argument("--iam-user", required=True)
    c.add_argument("--custom-endpoint-id", default=None)
    c.add_argument("--limitless", action="store_true")
    c.add_argument("--shard-id", default=None, help="Required for limitless clusters")
    c.add_argument("--monitoring-role-arn", default=None, help="Required for limitless clusters")
    c.add_argument("--extra-mysql-user", default=None, help="Additional MySQL user to create (macOS)")
    c.add_argument("--extra-mysql-password", default=None, help="Password for additional MySQL user")

    # -- destroy --
    d = sub.add_parser("destroy", help="Tear down DB cluster and all relevant resources")
    d.add_argument("--cluster-id", required=True)
    d.add_argument("--region", required=True)
    d.add_argument("--num-instances", type=int, default=5)
    d.add_argument("--custom-endpoint-id", default=None)
    d.add_argument("--secrets-arn", default=None)
    d.add_argument("--public-ip", default=None, help="IP to remove from SG (auto-detected if omitted)")
    d.add_argument("--limitless", action="store_true")
    d.add_argument("--shard-id", default=None, help="Required for limitless clusters")

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "create":
        if args.limitless and not args.shard_id:
            parser.error("--shard-id is required for limitless clusters")
        if args.limitless and not args.monitoring_role_arn:
            parser.error("--monitoring-role-arn is required for limitless clusters")
        cmd_create(args)
    elif args.command == "destroy":
        if args.limitless and not args.shard_id:
            parser.error("--shard-id is required for limitless clusters")
        cmd_destroy(args)


if __name__ == "__main__":
    main()
