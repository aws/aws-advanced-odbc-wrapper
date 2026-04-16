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
Unified Aurora and RDS resource management for CI integration tests.

Consolidates cluster/instance creation, endpoint setup, secrets, security group,
and IAM user provisioning into two commands: `create` and `destroy`.

Usage:
    python db_resources.py create  --cluster-id ID --engine ENGINE ...
    python db_resources.py create  --cluster-id ID --engine ENGINE --rds-single-az ...
    python db_resources.py create  --cluster-id ID --engine ENGINE --rds-multi-az ...
    python db_resources.py destroy --cluster-id ID --region REGION ...
"""

import argparse
import json
import os
import sys
import time
import urllib.request

import boto3
import mysql.connector
import psycopg
from aws_advanced_python_wrapper import AwsWrapperConnection
from botocore.exceptions import ClientError, WaiterError

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def get_public_ip():
    return urllib.request.urlopen("http://checkip.amazonaws.com").read().decode().strip()


def get_boto3_clients(region):
    """Return a dict of boto3 clients keyed by service name."""
    return {
        "rds": boto3.client("rds", region_name=region),
        "ec2": boto3.client("ec2", region_name=region),
        "secretsmanager": boto3.client("secretsmanager", region_name=region),
    }


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

def resolve_engine_version(rds_client, engine, version, limitless=False):
    if version != "latest":
        return version
    response = rds_client.describe_db_engine_versions(Engine=engine)
    versions = [
        v["EngineVersion"] for v in response["DBEngineVersions"]
        if (limitless and "-limitless" in v["EngineVersion"])
        or (not limitless and "-limitless" not in v["EngineVersion"])
    ]
    versions.sort()
    resolved = versions[-1] if versions else version
    print(f"  Resolved engine version: {resolved}")
    return resolved


# ---------------------------------------------------------------------------
# Security group helpers
# ---------------------------------------------------------------------------

def get_cluster_sg(rds_client, cluster_id):
    response = rds_client.describe_db_clusters(DBClusterIdentifier=cluster_id)
    return response["DBClusters"][0]["VpcSecurityGroups"][0]["VpcSecurityGroupId"]


def get_instance_sg(rds_client, instance_id):
    """Get the security group ID for a standalone RDS instance."""
    response = rds_client.describe_db_instances(DBInstanceIdentifier=instance_id)
    return response["DBInstances"][0]["VpcSecurityGroups"][0]["VpcSecurityGroupId"]


def add_ip_to_sg(ec2_client, rds_client, ip, cluster_id, is_rds_instance=False):
    sg = get_instance_sg(rds_client, cluster_id) if is_rds_instance else get_cluster_sg(rds_client, cluster_id)
    cidr = f"{ip}/32"
    try:
        ec2_client.authorize_security_group_ingress(
            GroupId=sg,
            IpProtocol="tcp",
            CidrIp=cidr,
            FromPort=0,
            ToPort=65535,
        )
    except ClientError as e:
        # Ignore duplicate rule errors
        if e.response["Error"]["Code"] != "InvalidPermission.Duplicate":
            print(f"  Warning: authorize-security-group-ingress failed: {e}")
    print(f"  Allowed {cidr} on SG {sg}, sleeping 60s for propagation...")
    time.sleep(60)


def remove_ip_from_sg(ec2_client, rds_client, ip, cluster_id, is_rds_instance=False):
    try:
        sg = get_instance_sg(rds_client, cluster_id) if is_rds_instance else get_cluster_sg(rds_client, cluster_id)
    except Exception:
        print("  Could not resolve SG — cluster may already be deleted")
        return
    cidr = f"{ip}/32"
    try:
        ec2_client.revoke_security_group_ingress(
            GroupId=sg,
            IpProtocol="tcp",
            CidrIp=cidr,
            FromPort=0,
            ToPort=65535,
        )
    except ClientError:
        pass
    print(f"  Removed {cidr} from SG {sg}")


# ---------------------------------------------------------------------------
# Cluster creation
# ---------------------------------------------------------------------------

def create_aurora_cluster(rds_client, args):
    version = resolve_engine_version(rds_client, args.engine, args.engine_version)

    create_kwargs = dict(
        DBClusterIdentifier=args.cluster_id,
        DatabaseName=args.database,
        MasterUsername=args.username,
        MasterUserPassword=args.password,
        SourceRegion=args.region,
        EnableIAMDatabaseAuthentication=True,
        Engine=args.engine,
        EngineVersion=version,
        StorageEncrypted=True,
        Tags=[{"Key": "env", "Value": "test-runner"}],
    )

    pg = getattr(args, "parameter_group", None)
    if pg:
        create_kwargs["DBClusterParameterGroupName"] = pg

    print("Creating Aurora RDS cluster...")
    rds_client.create_db_cluster(**create_kwargs)

    for i in range(1, args.num_instances + 1):
        rds_client.create_db_instance(
            DBClusterIdentifier=args.cluster_id,
            DBInstanceIdentifier=f"{args.cluster_id}-{i}",
            DBInstanceClass="db.r5.large",
            Engine=args.engine,
            EngineVersion=version,
            PubliclyAccessible=True,
            Tags=[{"Key": "env", "Value": "test-runner"}],
        )

    print("Waiting for instances to become available...")
    waiter = rds_client.get_waiter("db_instance_available")
    waiter.wait(
        Filters=[{"Name": "db-cluster-id", "Values": [args.cluster_id]}],
    )


def create_limitless_cluster(rds_client, args):
    version = resolve_engine_version(rds_client, args.engine, args.engine_version, limitless=True)

    print("Creating Limitless RDS cluster...")
    create_kwargs = dict(
        ClusterScalabilityType="limitless",
        DBClusterIdentifier=args.cluster_id,
        MasterUsername=args.username,
        MasterUserPassword=args.password,
        Engine=args.engine,
        EngineVersion=version,
        EnableCloudwatchLogsExports=["postgresql"],
        EnableIAMDatabaseAuthentication=True,
        EnablePerformanceInsights=True,
        PerformanceInsightsRetentionPeriod=31,
        StorageType="aurora-iopt1",
        Tags=[{"Key": "env", "Value": "test-runner"}],
    )
    if args.monitoring_role_arn:
        create_kwargs["MonitoringInterval"] = 5
        create_kwargs["MonitoringRoleArn"] = args.monitoring_role_arn
    else:
        print("  Warning: --monitoring-role-arn not provided, skipping enhanced monitoring.")

    rds_client.create_db_cluster(**create_kwargs)

    print("Creating Limitless shard group...")
    rds_client.create_db_shard_group(
        DBClusterIdentifier=args.cluster_id,
        DBShardGroupIdentifier=args.shard_id,
        MinACU=28.0,
        MaxACU=601.0,
        PubliclyAccessible=True,
        Tags=[{"Key": "env", "Value": "test-runner"}],
    )

    # Wait with retries
    waiter = rds_client.get_waiter("db_cluster_available")
    for attempt in range(3):
        print(f"  Waiting for cluster availability (attempt {attempt + 1}/3)...")
        try:
            waiter.wait(DBClusterIdentifier=args.cluster_id)
            print("  Cluster available.")
            break
        except WaiterError:
            if attempt < 2:
                time.sleep(30)
    else:
        raise RuntimeError("Cluster did not become available after retries")


def create_custom_endpoint(rds_client, cluster_id, endpoint_id, num_instances):
    half = num_instances // 2
    members = [f"{cluster_id}-{i}".lower() for i in range(1, half + 1)]

    waiter = rds_client.get_waiter("db_cluster_available")
    waiter.wait(DBClusterIdentifier=cluster_id)

    print(f"Creating custom endpoint {endpoint_id} with static members {members}...")
    rds_client.create_db_cluster_endpoint(
        DBClusterEndpointIdentifier=endpoint_id,
        DBClusterIdentifier=cluster_id,
        EndpointType="ANY",
        StaticMembers=members,
    )

    time.sleep(30)
    waiter.wait(DBClusterIdentifier=cluster_id)


def get_cluster_endpoint(rds_client, cluster_id):
    response = rds_client.describe_db_clusters(DBClusterIdentifier=cluster_id)
    return response["DBClusters"][0]["Endpoint"]


# ---------------------------------------------------------------------------
# RDS instance creation (non-Aurora, single-AZ / multi-AZ)
# ---------------------------------------------------------------------------

def resolve_rds_engine_version(rds_client, engine, version):
    """Resolve 'latest' to the newest major engine version for non-Aurora RDS engines."""
    if version != "latest":
        return version
    rds_engine = engine.replace("aurora-", "")
    response = rds_client.describe_db_engine_versions(Engine=rds_engine)
    versions = sorted([v["EngineVersion"] for v in response["DBEngineVersions"]])
    resolved = versions[-1] if versions else version
    print(f"  Resolved RDS engine version: {resolved}")
    return resolved


def create_rds_instance(rds_client, args, multi_az=False):
    """Create a standalone RDS DB instance (single-AZ or multi-AZ)."""
    rds_engine = args.engine.replace("aurora-", "")
    version = resolve_rds_engine_version(rds_client, rds_engine, args.engine_version)
    az_label = "Multi-AZ" if multi_az else "Single-AZ"

    print(f"Creating {az_label} RDS instance ({rds_engine} {version})...")
    rds_client.create_db_instance(
        DBInstanceIdentifier=args.cluster_id,
        DBInstanceClass=args.instance_class,
        Engine=rds_engine,
        EngineVersion=version,
        MasterUsername=args.username,
        MasterUserPassword=args.password,
        DBName=args.database,
        AllocatedStorage=args.allocated_storage,
        StorageEncrypted=True,
        PubliclyAccessible=True,
        EnableIAMDatabaseAuthentication=True,
        MultiAZ=multi_az,
        Tags=[{"Key": "env", "Value": "test-runner"}],
    )

    print("Waiting for instance to become available...")
    waiter = rds_client.get_waiter("db_instance_available")
    waiter.wait(DBInstanceIdentifier=args.cluster_id)


def get_rds_instance_endpoint(rds_client, instance_id):
    """Get the endpoint address for a standalone RDS instance."""
    response = rds_client.describe_db_instances(DBInstanceIdentifier=instance_id)
    return response["DBInstances"][0]["Endpoint"]["Address"]


def delete_rds_instance(rds_client, instance_id):
    """Delete a standalone RDS DB instance."""
    print(f"Deleting RDS instance {instance_id}...")
    try:
        rds_client.delete_db_instance(
            DBInstanceIdentifier=instance_id,
            SkipFinalSnapshot=True,
            DeleteAutomatedBackups=True,
        )
    except ClientError as e:
        print(f"  Warning: delete-db-instance failed: {e}")


def create_secrets(sm_client, username, password, engine, cluster_endpoint):
    secret_name = f"AWS-ODBC-Tests-{cluster_endpoint}"
    secret_value = json.dumps({
        "username": username,
        "password": password,
        "engine": engine,
        "host": cluster_endpoint,
    })
    response = sm_client.create_secret(
        Name=secret_name,
        Description="Secrets created by GH actions for DB auth",
        SecretString=secret_value,
    )
    return response["ARN"]


def setup_pg_iam_user(endpoint, port, database, username, password, iam_user):
    connstr = f"host={endpoint} port={port} dbname={database} user={username} password={password}"
    print(f"Connecting to PostgreSQL via AWS Advanced Python Wrapper: {endpoint}:{port}/{database}")
    try:
        with AwsWrapperConnection.connect(
            psycopg.Connection.connect,
            connstr,
            wrapper_dialect="aurora-pg",
            plugins="",
            autocommit=True
        ) as conn:
            cur = conn.cursor()
            print(f"  CREATE USER {iam_user}")
            cur.execute(f"CREATE USER {iam_user};")
            print(f"  GRANT rds_iam TO {iam_user}")
            cur.execute(f"GRANT rds_iam TO {iam_user};")
            cur.execute("SELECT usename FROM pg_user;")
            users = cur.fetchall()
            print("  Users:", ", ".join(u[0] for u in users))
            cur.close()
    except Exception as e:
        print(f"Warning: IAM user setup failed: {e}")


def setup_mysql_iam_user(endpoint, database, username, password, iam_user,
                         extra_user=None, extra_password=None):
    print(f"Connecting to MySQL via AWS Advanced Python Wrapper: {endpoint}:3306/{database}")
    try:
        with AwsWrapperConnection.connect(
            mysql.connector.Connect,
            host=endpoint,
            database=database,
            user=username,
            password=password,
            wrapper_dialect="aurora-mysql",
            plugins="",
            autocommit=True
        ) as conn:
            cur = conn.cursor()
            print(f"  CREATE USER {iam_user}")
            cur.execute(f"CREATE USER '{iam_user}' IDENTIFIED WITH AWSAuthenticationPlugin AS 'RDS'")
            cur.execute(f"GRANT ALL ON {database}.* TO '{iam_user}'@'%'")
            cur.execute("FLUSH PRIVILEGES")

            if extra_user and extra_password:
                print(f"  CREATE USER {extra_user}")
                cur.execute(f"CREATE USER '{extra_user}' IDENTIFIED WITH caching_sha2_password BY '{extra_password}'")
                cur.execute(f"GRANT ALL ON {database}.* TO '{extra_user}'@'%'")
                cur.execute("FLUSH PRIVILEGES")

            cur.close()
    except Exception as e:
        print(f"Warning: MySQL IAM user setup failed: {e}")


# ---------------------------------------------------------------------------
# Parameter group helpers
# ---------------------------------------------------------------------------

def create_parameter_group(rds_client, name, engine, engine_version):
    """Create or reuse a DB cluster parameter group configured for blue/green."""
    print(f"Checking for existing parameter group: {name}")

    try:
        rds_client.describe_db_cluster_parameter_groups(
            DBClusterParameterGroupName=name,
        )
        print(f"  Parameter group {name} already exists, reusing.")
        return name
    except ClientError as e:
        if e.response["Error"]["Code"] != "DBParameterGroupNotFound":
            raise

    version = resolve_engine_version(rds_client, engine, engine_version)

    if "mysql" in engine:
        param_family = f"aurora-mysql-{version}"
    elif "postgresql" in engine:
        major = version.split(".")[0]
        param_family = f"aurora-postgresql{major}"
    else:
        raise RuntimeError(f"Unsupported engine: {engine}")

    print(f"Creating parameter group {name} with family {param_family}...")
    rds_client.create_db_cluster_parameter_group(
        DBClusterParameterGroupName=name,
        DBParameterGroupFamily=param_family,
        Description="Custom parameter group for Blue/Green Deployment testing",
    )

    if "mysql" in engine:
        print("  Setting MySQL parameter: binlog_format=ROW")
        rds_client.modify_db_cluster_parameter_group(
            DBClusterParameterGroupName=name,
            Parameters=[{
                "ParameterName": "binlog_format",
                "ParameterValue": "ROW",
                "ApplyMethod": "pending-reboot",
            }],
        )
    elif "postgresql" in engine:
        print("  Setting PostgreSQL parameters: rds.logical_replication=1, max_wal_senders=20, rds.allowed_extensions=rds_tools")
        rds_client.modify_db_cluster_parameter_group(
            DBClusterParameterGroupName=name,
            Parameters=[
                {"ParameterName": "rds.logical_replication", "ParameterValue": "1", "ApplyMethod": "pending-reboot"},
                {"ParameterName": "max_wal_senders", "ParameterValue": "20", "ApplyMethod": "pending-reboot"},
                {"ParameterName": "rds.allowed_extensions", "ParameterValue": "rds_tools", "ApplyMethod": "pending-reboot"},
            ],
        )

    print(f"  Parameter group {name} created and configured.")
    return name


def delete_parameter_group(rds_client, name):
    """Delete a DB cluster parameter group."""
    print(f"Deleting parameter group: {name}")
    try:
        rds_client.delete_db_cluster_parameter_group(
            DBClusterParameterGroupName=name,
        )
    except ClientError as e:
        print(f"  Warning: delete parameter group failed: {e}")


# ---------------------------------------------------------------------------
# Blue/Green deployment helpers
# ---------------------------------------------------------------------------

def create_blue_green_deployment(rds_client, bg_name, cluster_id, engine, engine_version):
    """Create a blue/green deployment from an existing cluster. Returns deployment ID."""
    print(f"Creating Blue/Green Deployment: {bg_name}")

    response = rds_client.describe_db_clusters(DBClusterIdentifier=cluster_id)
    cluster_arn = response["DBClusters"][0]["DBClusterArn"]
    if not cluster_arn:
        raise RuntimeError(f"Failed to get cluster ARN for {cluster_id}")

    version = resolve_engine_version(rds_client, engine, engine_version)

    response = rds_client.create_blue_green_deployment(
        BlueGreenDeploymentName=bg_name,
        Source=cluster_arn,
        TargetEngineVersion=version,
        Tags=[{"Key": "env", "Value": "test-runner"}],
    )

    deployment_id = response["BlueGreenDeployment"]["BlueGreenDeploymentIdentifier"]
    print(f"  Blue/Green Deployment created: {deployment_id}")
    return deployment_id


def wait_blue_green_deployment_ready(rds_client, deployment_id, max_retries=30, interval=60):
    """Poll until the blue/green deployment reaches AVAILABLE status."""
    print(f"Waiting for Blue/Green Deployment {deployment_id} to be ready...")
    time.sleep(60)

    for attempt in range(1, max_retries + 1):
        print(f"  Attempt {attempt}/{max_retries}: checking status...")
        response = rds_client.describe_blue_green_deployments(
            BlueGreenDeploymentIdentifier=deployment_id,
        )
        deployment = response["BlueGreenDeployments"][0]
        status = deployment["Status"]
        print(f"  Status: {status}")

        if status == "AVAILABLE":
            all_ready = all(
                s.get("Status") == "AVAILABLE"
                for s in deployment.get("SwitchoverDetails", [])
            )
            if all_ready:
                print("  Blue/Green Deployment is ready!")
                return

        if status in ("SWITCHOVER_COMPLETED", "SWITCHOVER_FAILED", "INVALID_CONFIGURATION"):
            raise RuntimeError(f"Deployment reached terminal state: {status}")

        if attempt < max_retries:
            print(f"  Retrying in {interval}s...")
            time.sleep(interval)

    raise RuntimeError(f"Timeout waiting for deployment after {max_retries} attempts")


def delete_blue_green_deployment(rds_client, ec2_client, deployment_id, region):
    """Delete a blue/green deployment and clean up source/target clusters."""
    print(f"Deleting Blue/Green Deployment {deployment_id}...")

    try:
        response = rds_client.describe_blue_green_deployments(
            BlueGreenDeploymentIdentifier=deployment_id,
        )
    except ClientError:
        print("  Deployment not found, skipping.")
        return

    deployment = response["BlueGreenDeployments"][0]
    source_arn = deployment.get("Source", "")
    target_arn = deployment.get("Target", "")

    source_cluster_id = source_arn.split(":cluster:")[-1] if ":cluster:" in source_arn else ""
    target_cluster_id = target_arn.split(":cluster:")[-1] if ":cluster:" in target_arn else ""

    ip = get_public_ip()
    if source_cluster_id:
        remove_ip_from_sg(ec2_client, rds_client, ip, source_cluster_id)

    try:
        rds_client.delete_blue_green_deployment(
            BlueGreenDeploymentIdentifier=deployment_id,
        )
    except ClientError as e:
        print(f"  Warning: delete blue/green deployment failed: {e}")

    print("  Waiting for BG deletion to propagate (30m)...")
    time.sleep(1800)

    if target_cluster_id:
        delete_aurora_cluster_by_id(rds_client, target_cluster_id)
    if source_cluster_id:
        delete_aurora_cluster_by_id(rds_client, source_cluster_id)

    print("  Waiting for cluster deletion to complete (30m)...")
    time.sleep(1800)

    print(f"  Blue/Green Deployment {deployment_id} cleanup complete.")


def delete_aurora_cluster_by_id(rds_client, cluster_id):
    """Delete an Aurora cluster by discovering its instances first."""
    print(f"Deleting Aurora cluster {cluster_id}...")
    try:
        response = rds_client.describe_db_clusters(DBClusterIdentifier=cluster_id)
        instances = [m["DBInstanceIdentifier"] for m in response["DBClusters"][0].get("DBClusterMembers", [])]
        for inst in instances:
            print(f"  Deleting instance: {inst}")
            try:
                rds_client.delete_db_instance(
                    DBInstanceIdentifier=inst,
                    SkipFinalSnapshot=True,
                )
            except ClientError as e:
                print(f"  Warning: delete instance {inst} failed: {e}")
    except ClientError as e:
        print(f"  Warning: describe cluster {cluster_id} failed: {e}")

    try:
        rds_client.delete_db_cluster(
            DBClusterIdentifier=cluster_id,
            SkipFinalSnapshot=True,
        )
    except ClientError as e:
        print(f"  Warning: delete cluster {cluster_id} failed: {e}")


# ---------------------------------------------------------------------------
# Cluster destruction
# ---------------------------------------------------------------------------

def delete_custom_endpoint(rds_client, endpoint_id, cluster_id):
    print(f"Deleting custom endpoint {endpoint_id}...")
    try:
        rds_client.delete_db_cluster_endpoint(
            DBClusterEndpointIdentifier=endpoint_id,
        )
    except ClientError as e:
        print(f"  Warning: delete custom endpoint failed: {e}")

    try:
        waiter = rds_client.get_waiter("db_cluster_available")
        waiter.wait(DBClusterIdentifier=cluster_id)
    except (ClientError, WaiterError) as e:
        print(f"  Warning: wait after endpoint deletion failed: {e}")


def delete_aurora_cluster(rds_client, cluster_id, num_instances):
    print(f"Deleting Aurora cluster {cluster_id} ({num_instances} instances)...")
    for i in range(1, num_instances + 1):
        try:
            rds_client.delete_db_instance(
                DBInstanceIdentifier=f"{cluster_id}-{i}",
                SkipFinalSnapshot=True,
            )
        except ClientError as e:
            print(f"  Warning: delete instance {cluster_id}-{i} failed: {e}")

    try:
        rds_client.delete_db_cluster(
            DBClusterIdentifier=cluster_id,
            SkipFinalSnapshot=True,
        )
    except ClientError as e:
        print(f"  Warning: delete cluster {cluster_id} failed: {e}")


def delete_limitless_cluster(rds_client, cluster_id, shard_id):
    max_retries = 5
    for attempt in range(max_retries):
        try:
            rds_client.delete_db_shard_group(DBShardGroupIdentifier=shard_id)
            print(f"  Shard {shard_id} deletion initiated.")
            break
        except ClientError as e:
            if "already being deleted" in str(e):
                print(f"  Shard {shard_id} deletion initiated.")
                break
            if attempt < max_retries - 1:
                print(f"  Retrying shard deletion ({max_retries - attempt - 1} left)...")
                time.sleep(30)
    else:
        print(f"  WARNING: Failed to delete shard {shard_id} after {max_retries} attempts")

    for attempt in range(max_retries):
        try:
            rds_client.delete_db_cluster(
                DBClusterIdentifier=cluster_id,
                SkipFinalSnapshot=True,
            )
            print(f"  Cluster {cluster_id} deletion initiated.")
            break
        except ClientError:
            if attempt < max_retries - 1:
                print(f"  Retrying cluster deletion ({max_retries - attempt - 1} left)...")
                time.sleep(30)
    else:
        print(f"  WARNING: Failed to delete cluster {cluster_id} after {max_retries} attempts")


def delete_secrets(sm_client, secrets_arn):
    if secrets_arn:
        print(f"Deleting secrets {secrets_arn}...")
        try:
            sm_client.delete_secret(SecretId=secrets_arn)
        except ClientError as e:
            print(f"  Warning: delete secret failed: {e}")


# ---------------------------------------------------------------------------
# CLI: create
# ---------------------------------------------------------------------------

def cmd_create(args):
    """Create all Aurora/RDS resources and export env vars for subsequent steps."""
    clients = get_boto3_clients(args.region)
    rds_client = clients["rds"]
    ec2_client = clients["ec2"]
    sm_client = clients["secretsmanager"]

    is_limitless = args.limitless
    is_blue_green = args.blue_green
    is_rds_instance = args.rds_single_az or args.rds_multi_az

    # 0. Parameter group (required for blue/green, optional otherwise)
    if is_blue_green and args.parameter_group:
        pg_name = create_parameter_group(rds_client, args.parameter_group, args.engine, args.engine_version)
        set_github_env("PARAMETER_GROUP", pg_name)

    # 1. Create cluster or instance
    if is_rds_instance:
        create_rds_instance(rds_client, args, multi_az=args.rds_multi_az)
    elif is_limitless:
        create_limitless_cluster(rds_client, args)
    else:
        create_aurora_cluster(rds_client, args)

    # 2. Get public IP and add to SG
    ip = get_public_ip()
    set_github_output("public_ip", ip)
    add_ip_to_sg(ec2_client, rds_client, ip, args.cluster_id, is_rds_instance=is_rds_instance)

    # 3. Custom endpoint (Aurora non-limitless only)
    if not is_limitless and not is_rds_instance and args.custom_endpoint_id:
        create_custom_endpoint(rds_client, args.cluster_id, args.custom_endpoint_id, args.num_instances)

    # 4. Get endpoint
    if is_rds_instance:
        endpoint = get_rds_instance_endpoint(rds_client, args.cluster_id)
    else:
        endpoint = get_cluster_endpoint(rds_client, args.cluster_id)
    set_github_env("AURORA_CLUSTER_ENDPOINT", endpoint)

    # 5. Create secrets
    secrets_arn = create_secrets(sm_client, args.username, args.password, args.engine, endpoint)
    set_github_env("AURORA_CLUSTER_SECRETS_ARN", secrets_arn, mask=True)

    # 6. Setup IAM DB user
    port = "5432" if "postgresql" in args.engine else "3306"
    if "postgresql" in args.engine:
        setup_pg_iam_user(endpoint, port, args.database, args.username, args.password, args.iam_user)
    elif "mysql" in args.engine:
        setup_mysql_iam_user(endpoint, args.database, args.username, args.password, args.iam_user,
                             extra_user=args.extra_mysql_user, extra_password=args.extra_mysql_password)

    # 7. Blue/Green deployment (Aurora only)
    if is_blue_green and not is_rds_instance:
        bg_name = f"BG-{args.cluster_id}"
        bg_id = create_blue_green_deployment(rds_client, bg_name, args.cluster_id, args.engine, args.engine_version)
        set_github_env("BG_RESOURCE_ID", bg_id)

        wait_blue_green_deployment_ready(rds_client, bg_id)

        bg_endpoint = get_cluster_endpoint(rds_client, args.cluster_id)
        set_github_env("BG_CLUSTER_ENDPOINT", bg_endpoint)

    # 8. Describe resource for logs
    if is_rds_instance:
        try:
            response = rds_client.describe_db_instances(DBInstanceIdentifier=args.cluster_id)
            print(json.dumps(response["DBInstances"][0], indent=2, default=str))
        except ClientError:
            pass
    else:
        try:
            response = rds_client.describe_db_clusters(DBClusterIdentifier=args.cluster_id)
            print(json.dumps(response["DBClusters"][0], indent=2, default=str))
        except ClientError:
            pass

    print("\nResources created successfully.")


# ---------------------------------------------------------------------------
# CLI: destroy
# ---------------------------------------------------------------------------

def cmd_destroy(args):
    """Tear down all Aurora/RDS resources. Best-effort — never raises."""
    clients = get_boto3_clients(args.region)
    rds_client = clients["rds"]
    ec2_client = clients["ec2"]
    sm_client = clients["secretsmanager"]

    ip = args.public_ip or get_public_ip()
    is_rds_instance = args.rds_single_az or args.rds_multi_az

    # Blue/Green deployment
    if args.blue_green and args.bg_deployment_id:
        try:
            delete_blue_green_deployment(rds_client, ec2_client, args.bg_deployment_id, args.region)
        except Exception as e:
            print(f"  WARNING: blue/green deployment cleanup failed: {e}")

    # Custom endpoint
    if args.custom_endpoint_id:
        try:
            delete_custom_endpoint(rds_client, args.custom_endpoint_id, args.cluster_id)
        except Exception as e:
            print(f"  WARNING: custom endpoint cleanup failed: {e}")

    # Cluster or instance (skip if blue/green already handled cluster deletion)
    if not (args.blue_green and args.bg_deployment_id):
        try:
            if is_rds_instance:
                delete_rds_instance(rds_client, args.cluster_id)
            elif args.limitless:
                delete_limitless_cluster(rds_client, args.cluster_id, args.shard_id)
            else:
                delete_aurora_cluster(rds_client, args.cluster_id, args.num_instances)
        except Exception as e:
            print(f"  WARNING: cluster/instance cleanup failed: {e}")

    # Parameter group
    if args.parameter_group:
        try:
            delete_parameter_group(rds_client, args.parameter_group)
        except Exception as e:
            print(f"  WARNING: parameter group cleanup failed: {e}")

    # Secrets
    try:
        delete_secrets(sm_client, args.secrets_arn)
    except Exception as e:
        print(f"  WARNING: secrets cleanup failed: {e}")

    # Security group
    try:
        remove_ip_from_sg(ec2_client, rds_client, ip, args.cluster_id, is_rds_instance=is_rds_instance)
    except Exception as e:
        print(f"  WARNING: SG cleanup failed: {e}")

    print("\nResource cleanup complete.")


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def build_parser():
    parser = argparse.ArgumentParser(description="Aurora resource management for CI")
    sub = parser.add_subparsers(dest="command", required=True)

    # -- create --
    c = sub.add_parser("create", help="Provision DB cluster/instance and all supporting resources")
    c.add_argument("--cluster-id", required=True, help="Cluster or instance identifier")
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
    c.add_argument("--monitoring-role-arn", nargs="?", default=None, help="ARN for enhanced monitoring (required for limitless clusters)")
    c.add_argument("--extra-mysql-user", default=None, help="Additional MySQL user to create (macOS)")
    c.add_argument("--extra-mysql-password", default=None, help="Password for additional MySQL user")
    c.add_argument("--blue-green", action="store_true", help="Create a blue/green deployment after cluster setup")
    c.add_argument("--parameter-group", default=None, help="Name for the custom parameter group (created/reused)")
    c.add_argument("--rds-single-az", action="store_true", help="Create a standalone RDS single-AZ instance instead of Aurora")
    c.add_argument("--rds-multi-az", action="store_true", help="Create a standalone RDS multi-AZ instance instead of Aurora")
    c.add_argument("--instance-class", default="db.m5.large", help="DB instance class for RDS instances (default: db.m5.large)")
    c.add_argument("--allocated-storage", type=int, default=20, help="Allocated storage in GB for RDS instances (default: 20)")

    # -- destroy --
    d = sub.add_parser("destroy", help="Tear down DB cluster/instance and all relevant resources")
    d.add_argument("--cluster-id", required=True, help="Cluster or instance identifier")
    d.add_argument("--region", required=True)
    d.add_argument("--num-instances", type=int, default=5)
    d.add_argument("--custom-endpoint-id", default=None)
    d.add_argument("--secrets-arn", default=None)
    d.add_argument("--public-ip", default=None, help="IP to remove from SG (auto-detected if omitted)")
    d.add_argument("--limitless", action="store_true")
    d.add_argument("--shard-id", default=None, help="Required for limitless clusters")
    d.add_argument("--blue-green", action="store_true", help="Tear down blue/green deployment resources")
    d.add_argument("--bg-deployment-id", default=None, help="Blue/green deployment identifier to delete")
    d.add_argument("--parameter-group", default=None, help="Parameter group name to delete")
    d.add_argument("--rds-single-az", action="store_true", help="Destroy a standalone RDS single-AZ instance")
    d.add_argument("--rds-multi-az", action="store_true", help="Destroy a standalone RDS multi-AZ instance")

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "create":
        if args.limitless and not args.shard_id:
            parser.error("--shard-id is required for limitless clusters")
        if args.limitless and not args.monitoring_role_arn:
            parser.error("--monitoring-role-arn is required for limitless clusters")
        if args.rds_single_az and args.rds_multi_az:
            parser.error("--rds-single-az and --rds-multi-az are mutually exclusive")
        if (args.rds_single_az or args.rds_multi_az) and args.limitless:
            parser.error("--rds-single-az/--rds-multi-az cannot be combined with --limitless")
        cmd_create(args)
    elif args.command == "destroy":
        if args.limitless and not args.shard_id:
            parser.error("--shard-id is required for limitless clusters")
        if args.rds_single_az and args.rds_multi_az:
            parser.error("--rds-single-az and --rds-multi-az are mutually exclusive")
        cmd_destroy(args)


if __name__ == "__main__":
    main()
