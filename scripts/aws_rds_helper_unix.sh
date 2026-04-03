#!/bin/bash
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

true=1
false=0

# ---------------- Security Group Operations ----------------------
function add_ip_to_db_sg {
    LocalIp=$1
    ClusterId=$2
    Region=$3

    # Get the security group associated with the DB cluster using AWS CLI
    dbClusterInfo=$(aws rds describe-db-clusters --db-cluster-identifier $ClusterId --region $Region)

    if [ $? -ne 0 ]; then
        echo "Failed to get DB cluster information."
        exit 1
    fi

    # Check if we got the DB Cluster information
    dbClustersCount=$(echo "$dbClusterInfo" | jq -r '.DBClusters | length')
    if [ $dbClustersCount -eq 0 ]; then
        echo "Error: DB Cluster with ID '$ClusterId' not found in region '$Region'."
        exit 1
    fi

    securityGroupId=$(echo "$dbClusterInfo" | jq -r '.DBClusters[0].VpcSecurityGroups[0].VpcSecurityGroupId')
    cidrBlock="$LocalIp/32"

    # Allow inbound traffic from the local IP address to the DB security group using AWS CLI
    aws ec2 authorize-security-group-ingress --group-id $securityGroupId --protocol tcp --cidr $cidrBlock --port 0-65535 --region $Region || true

    # Sleep to ensure IP is added and propagated to DBs before moving onto next step
    sleep 60
} # function add_ip_to_db_sg
export -f add_ip_to_db_sg

function remove_ip_from_db_sg {
    LocalIp=$1
    ClusterId=$2
    Region=$3

    # Get the security group associated with the DB cluster using AWS CLI
    dbClusterInfo=$(aws rds describe-db-clusters --db-cluster-identifier $ClusterId --region $Region)

    if [ $? -ne 0 ]; then
        echo "Failed to get DB cluster information."
        exit 1
    fi

    # Check if we got the DB Cluster information
    dbClustersCount=$(echo "$dbClusterInfo" | jq -r '.DBClusters | length')
    if [ $dbClustersCount -eq 0 ]; then
        echo "Error: DB Cluster with ID '$ClusterId' not found in region '$Region'."
        exit 1
    fi

    # Extract the Security Group ID
    securityGroupId=$(echo "$dbClusterInfo" | jq -r '.DBClusters[0].VpcSecurityGroups[0].VpcSecurityGroupId')

    # Define the CIDR block for the local IP address (allowing all ports and all traffic)
    cidrBlock="$LocalIp/32"

    # Revoke Inbound traffic
    aws ec2 revoke-security-group-ingress --group-id $securityGroupId --protocol tcp --cidr $cidrBlock --port 0-65535 --region $Region || true

    # Check if revoked successfully
    if [ $? -eq 0 ]; then
        echo "Removed IP: $LocalIp from security group $securityGroupId."
    else
        echo "Failed to remove ip $LocalIp from security group $securityGroupId."
    fi
} # function remove_ip_from_db_sg
export -f remove_ip_from_db_sg

# ---------------- Create DB operations ----------------------
function create_aurora_rds_cluster {
    TestUsername=$1
    TestPassword=$2
    TestDatabase=$3
    ClusterId=$4
    NumInstances=$5
    Engine=$6
    EngineVersion=$7
    Region=$8
    ParameterGroup=$9

    echo "Creating RDS Cluster"

    # Find latest engine version
    if [ "$EngineVersion" = "latest" ]; then
        AllEngineVersions=$(aws rds describe-db-engine-versions\
            --engine $Engine\
            --query "DBEngineVersions[?!contains(EngineVersion, '-limitless')].EngineVersion"\
            --output text)
        LatestVersion=$(echo $AllEngineVersions | tr ' ' '\n' | sort -V | tail -n 1)
        EngineVersion=$LatestVersion
        echo "Using Latest Version: $EngineVersion"
    fi

    # Create RDS Cluster
    if [[ -n "$ParameterGroup" ]]; then
        echo "Parameter group used: $ParameterGroup"
        ClusterInfo=$(
            aws rds create-db-cluster\
                --db-cluster-identifier $ClusterId\
                --database-name $TestDatabase\
                --master-username $TestUsername\
                --master-user-password $TestPassword\
                --source-region $Region\
                --enable-iam-database-authentication\
                --engine  $Engine\
                --engine-version $EngineVersion\
                --storage-encrypted\
                --tags "Key=env,Value=test-runner"\
                --db-cluster-parameter-group-name $ParameterGroup
        )
    else
        ClusterInfo=$(
            aws rds create-db-cluster\
                --db-cluster-identifier $ClusterId\
                --database-name $TestDatabase\
                --master-username $TestUsername\
                --master-user-password $TestPassword\
                --source-region $Region\
                --enable-iam-database-authentication\
                --engine  $Engine\
                --engine-version $EngineVersion\
                --storage-encrypted\
                --tags "Key=env,Value=test-runner"
        )
    fi

    if [ $? -ne 0 ]; then
        echo "Failed to create RDS Cluster."
        exit 1
    fi

    i=1

    while [ $i -le $NumInstances ]
    do
        aws rds create-db-instance\
            --db-cluster-identifier $ClusterId\
            --db-instance-identifier  "$ClusterId-$i"\
            --db-instance-class "db.r5.large"\
            --engine $Engine\
            --engine-version $EngineVersion\
            --publicly-accessible\
            --tags "Key=env,Value=test-runner"

        if [ $? -ne 0 ]; then
            echo "Failed to create DB Instance."
            exit 1
        fi

        ((i++))
    done

    aws rds wait db-instance-available\
        --filters "Name=db-cluster-id,Values=${ClusterId}"
} # function create_aurora_rds_cluster
export -f create_aurora_rds_cluster

function create_limitless_rds_cluster {
    TestUsername=$1
    TestPassword=$2
    TestDatabase=$3
    ClusterId=$4
    ShardId=$5
    Engine=$6
    EngineVersion=$7
    AwsRdsMonitoringRoleArn=$8
    Region=$9

    echo "Creating Limitless RDS Cluster"

    # Find latest engine version
    if [ "$EngineVersion" = "latest" ]; then
        AllEngineVersions=$(aws rds describe-db-engine-versions\
            --engine $Engine\
            --query "DBEngineVersions[?contains(EngineVersion, '-limitless')].EngineVersion"\
            --output text)
        LatestVersion=$(echo $AllEngineVersions | tr ' ' '\n' | sort -V | tail -n 1)
        EngineVersion=$LatestVersion
        echo "Using Latest Version: $EngineVersion"
    fi

    # Create Limitless RDS Cluster
    ClusterInfo=$(
        aws rds create-db-cluster\
            --cluster-scalability-type "limitless"\
            --db-cluster-identifier $ClusterId\
            --master-username $TestUsername\
            --master-user-password $TestPassword\
            --region $Region\
            --engine $Engine\
            --engine-version $EngineVersion\
            --enable-cloudwatch-logs-export "postgresql"\
            --enable-iam-database-authentication\
            --enable-performance-insights\
            --monitoring-interval 5\
            --performance-insights-retention-period 31\
            --monitoring-role-arn $AwsRdsMonitoringRoleArn\
            --storage-type "aurora-iopt1"\
            --tags "Key=env,Value=test-runner"
    )

    if [ $? -ne 0 ]; then
        echo "Failed to create Limitless RDS Cluster."
        exit 1
    fi

    echo "Creating Limitless Shard Group"

    ShardInfo=$(
        aws rds create-db-shard-group\
            --db-cluster-identifier $ClusterId\
            --db-shard-group-identifier $ShardId\
            --min-acu 28.0\
            --max-acu 601.0\
            --publicly-accessible\
            --tags "Key=env,Value=test-runner"
    )

    if [ $? -ne 0 ]; then
        echo "Failed to create Limitless Shard Group."
        exit 1
    fi

    # Wait for availability for limitless
    maxRetries=2
    attempt=0
    waitSuccessful=$false

    # Retry logic
    while [[ $attempt -le $maxRetries && $waitSuccessful -ne $true ]]
    do
        echo "Attempt $((attempt + 1)): Checking DB cluster availability..."

        # Call the AWS CLI to wait for DB cluster to be available
        aws rds wait db-cluster-available --db-cluster-identifier "$ClusterId"

        if [ $? -eq 0 ]; then
            # If the command succeeds, it will return nothing, so we set the success flag
            waitSuccessful=$true
            echo "DB Cluster is now available."
            break
        else
            echo "Error: DB cluster is not available. Attempt $((attempt + 1)) failed."
            ((attempt++))
            if [ $attempt -le $maxRetries ]; then
                echo "Retrying... ($((maxRetries - attempt)) retries left)"
                sleep 30 # Wait for 30 seconds before retrying
            fi
        fi
    done

    if [ $waitSuccessful -ne $true ]; then
        echo "Failed to wait for DB cluster availability after $((maxRetries + 1)) attempts."
        exit 1
    else
        echo "Successfully detected DB cluster availability."
    fi
} # function create_limitless_rds_cluster
export -f create_limitless_rds_cluster

function create_custom_endpoint {
    EndpointName=$1
    ClusterId=$2
    NumInstances=$3
    StaticList=${4:-}

    Half=$((NumInstances/2))
    InstancesList=()
    for ((i=1; i<=Half; i++)); do
        InstancesList+=($(echo "${ClusterId}-${i}" | tr '[:upper:]' '[:lower:]'))
    done

    # Wait until cluster is ready for modification
    aws rds wait db-cluster-available --db-cluster-identifier ${ClusterId}

    if [[ "$StaticList" == "--static" ]]; then
        aws rds create-db-cluster-endpoint \
            --db-cluster-endpoint-identifier "$EndpointName" \
            --db-cluster-identifier "$ClusterId" \
            --endpoint-type any \
            --static-members "${InstancesList[@]}"
    else
        aws rds create-db-cluster-endpoint \
            --db-cluster-endpoint-identifier "$EndpointName" \
            --db-cluster-identifier "$ClusterId" \
            --endpoint-type any \
            --excluded-members "${InstancesList[@]}"
    fi

    # Wait for 30 seconds to allow cluster to update status
    sleep 30

    # Wait until cluster is ready after modification
    aws rds wait db-cluster-available --db-cluster-identifier ${ClusterId}
} # create_custom_endpoint
export -f create_custom_endpoint

# ---------------- Db Deletion operations ----------------------
function delete_dbshards {
    ShardId=$1

    # AWS CLI command to delete a DB shard
    aws rds delete-db-shard-group --db-shard-group-identifier $ShardId
} # delete_dbshards
export -f delete_dbshards

function delete_dbcluster {
    ClusterId=$1

    # AWS CLI command to delete the DB cluster
    aws rds delete-db-cluster --db-cluster-identifier $ClusterId --skip-final-snapshot
} # delete_dbcluster
export -f delete_dbcluster

function delete_dbinstances {
    ClusterId=$1

    echo "Deleting DBInstance"

    instances=$(
        aws rds describe-db-clusters \
            --db-cluster-identifier "$ClusterId" \
            --query 'DBClusters[0].DBClusterMembers[].DBInstanceIdentifier' \
            --output text
    )

    echo "$instances"

    for instance in $instances
    do
        echo "Deleting: $instance"
        aws rds delete-db-instance --skip-final-snapshot --db-instance-identifier "$instance"
    done
} # delete_dbinstances
export -f delete_dbinstances

function delete_aurora_db_cluster {
    ClusterId=$1
    Region=$2

    delete_dbinstances $ClusterId
    delete_dbcluster $ClusterId
} # delete_aurora_db_cluster
export -f delete_aurora_db_cluster

function delete_limitless_db_cluster {
    ClusterId=$1
    ShardId=$2
    Region=$3

    # Retry settings
    maxRetries=5
    attempt=0
    deleteShardsSuccessful=$false
    deleteClusterSuccessful=$false

    # Retry logic for deleting DB shards
    while [[ $attempt -lt $maxRetries && $deleteShardsSuccessful -eq $false ]]
    do
        output=$(delete_dbshards $ShardId 2>&1)
        last_code=$?
        if [[ $last_code -eq 0 ]]; then
            echo "Successfully called deletion command"
            deleteShardsSuccessful=$true
            break
        else
            if $(echo "$output" | grep -q "already being deleted"); then
                echo "Shard already in deletion phase"
                deleteShardsSuccessful=$true
                break
            else
                deleteShardsSuccessful=$false
            fi
        fi
        ((attempt++))

        if [ $deleteShardsSuccessful -ne $true ]; then
            if [ $attempt -lt $maxRetries ]; then
                attemptsLeft=$((maxRetries - attempt))
                echo "Retrying DB shard deletion... ($attemptsLeft retries left)"
                sleep 30  # Wait for 30 seconds before retrying
            fi
        fi
    done

    if [ $deleteShardsSuccessful -ne $true ]; then
        echo "Failed to delete DB shard $ShardId after $maxRetries attempts."
        exit 1
    else
        echo "Successfully deleted DB shard $ShardId."
    fi

    # Reset attempt counter for DB cluster deletion
    attempt=0

    # Retry logic for deleting DB cluster
    while [[ $attempt -lt $maxRetries && $deleteClusterSuccessful -eq $false ]]
    do
        delete_dbcluster $ClusterId
        last_code=$?
        if [[ $last_code -ne 0 ]]; then
            deleteClusterSuccessful=$false
        else
            deleteClusterSuccessful=$true
            break
        fi
        ((attempt++))

        if [ $deleteClusterSuccessful -ne $true ]; then
            if [ $attempt -lt $maxRetries ]; then
                attemptsLeft=$((maxRetries - attempt))
                echo "Retrying DB cluster deletion... ($attemptsLeft retries left)"
                sleep 30  # Wait for 30 seconds before retrying
            fi
        fi
    done

    if [ $deleteClusterSuccessful -ne $true ]; then
        echo "Failed to delete DB cluster $ClusterId after $maxRetries attempts."
        exit 1
    fi

    echo "Successfully deleted DB cluster $ClusterId."
    exit 0
} # delete_limitless_db_cluster
export -f delete_limitless_db_cluster

function delete_custom_endpoint {
    EndpointName=$1
    ClusterId=$2

    # Delete Custom Endpoint
    aws rds delete-db-cluster-endpoint --db-cluster-endpoint-identifier $EndpointName

    # Wait until delete operation completes
    aws rds wait db-cluster-available --db-cluster-identifier $ClusterId
} # delete_custom_endpoint
export -f delete_custom_endpoint

# ---------------- Get Cluster endpoint ----------------------
function get_cluster_endpoint {
    ClusterId=$1

    # Get the DB cluster details using AWS CLI
    echo $(aws rds describe-db-clusters --db-cluster-identifier $ClusterId --query DBClusters[0].Endpoint --output text)
} # get_cluster_endpoint
export -f get_cluster_endpoint

function describe_cluster_endpoint {
    ClusterId=$1

    # Get the DB cluster details using AWS CLI
    aws rds describe-db-clusters --db-cluster-identifier $ClusterId
} # get_cluster_endpoint
export -f get_cluster_endpoint

# ---------------- Secrets Manager Operations ----------------------
function create_db_secrets {
    Username=$1
    Password=$2
    Engine=$3
    ClusterEndpoint=$4

    # Define the secret name (you can adjust this if you want a different name)
    secretName="AWS-ODBC-Tests-$ClusterEndpoint"

    # Create a dictionary to hold key-value pairs for the secret
    jsonSecretValue=$(
        jq -n \
            --arg username "$Username" \
            --arg password "$Password" \
            --arg engine "$Engine" \
            --arg host "$ClusterEndpoint" \
            '{username: $username, password: $password, engine: $engine, host: $host}'
    )

    jsonResponse=$(
        aws secretsmanager create-secret\
            --name $secretName\
            --description "Secrets created by GH actions for DB auth"\
            --secret-string "$jsonSecretValue"
    )

    # Parse the ARN of the newly created secret from the output
    secretArn=$(echo $jsonResponse | jq -r '.ARN')
    echo $secretArn
} # create_db_secrets
export -f create_db_secrets

function delete_secrets {
    SecretsArn=$1
    aws secretsmanager delete-secret --secret-id $SecretsArn
} # delete_secrets
export -f delete_secrets

# ---------------- Parameter Group Operations ----------------------

function new_dbcluster_parameter_group {
    Name=$1
    Engine=$2
    EngineVersion=$3

    echo "Creating custom parameter group: $Name"

    # Handle latest engine version
    if [ "$EngineVersion" = "latest" ]; then
        AllEngineVersions=$(aws rds describe-db-engine-versions \
            --engine $Engine \
            --query "DBEngineVersions[?!contains(EngineVersion, '-limitless')].EngineVersion" \
            --output text)
        LatestVersion=$(echo $AllEngineVersions | tr ' ' '\n' | sort -V | tail -n 1)
        EngineVersion=$LatestVersion
        echo "Using Latest Version: $EngineVersion"
    fi

    # Determine parameter family
    if [[ "$Engine" == *"mysql"* ]]; then
        paramFamily="aurora-mysql-$EngineVersion"
    elif [[ "$Engine" == *"postgresql"* ]]; then
        EngineVersion=$(echo "$EngineVersion" | cut -d'.' -f1)
        echo "Updated Latest Version to remove minor: $EngineVersion"
        paramFamily="aurora-postgresql$EngineVersion"
    else
        echo "Unsupported engine: $Engine"
        exit 1
    fi

    # Create parameter group
    aws rds create-db-cluster-parameter-group \
        --db-cluster-parameter-group-name $Name \
        --db-parameter-group-family $paramFamily \
        --description "Custom parameter group for Blue/Green Deployment testing"

    # Set engine-specific parameters
    if [[ "$Engine" == *"mysql"* ]]; then
        echo "Setting MySQL parameter: binlog_format=ROW"
        aws rds modify-db-cluster-parameter-group \
            --db-cluster-parameter-group-name $Name \
            --parameters "ParameterName=binlog_format,ParameterValue=ROW,ApplyMethod=pending-reboot"
    elif [[ "$Engine" == *"postgresql"* ]]; then
        echo "Setting PostgreSQL parameter: rds.logical_replication=1"
        aws rds modify-db-cluster-parameter-group \
            --db-cluster-parameter-group-name $Name \
            --parameters "ParameterName=rds.logical_replication,ParameterValue=1,ApplyMethod=pending-reboot"
    fi
} # new_dbcluster_parameter_group
export -f new_dbcluster_parameter_group

function attach_dbcluster_parameter_group {
    ClusterId=$1
    ParameterGroupName=$2

    echo "Attaching parameter group $ParameterGroupName to cluster $ClusterId"

    aws rds modify-db-cluster \
        --db-cluster-identifier $ClusterId \
        --db-cluster-parameter-group-name $ParameterGroupName \
        --apply-immediately

    WriterInstance=$(aws rds describe-db-clusters \
        --db-cluster-identifier $ClusterId \
        --query 'DBClusters[0].DBClusterMembers[?IsClusterWriter==`true`].DBInstanceIdentifier' \
        --output text)

    aws rds reboot-db-instance \
        --db-instance-identifier $WriterInstance

    aws rds wait db-cluster-available --db-cluster-identifier $ClusterId

    aws rds wait db-instance-available \
        --filters "Name=db-cluster-id,Values=${ClusterId}"

    echo "Parameter group attached successfully"
} # attach_dbcluster_parameter_group
export -f attach_dbcluster_parameter_group

function delete_dbcluster_parameter_group {
    Name=$1

    echo "Deleting parameter group: $Name"

    aws rds delete-db-cluster-parameter-group \
        --db-cluster-parameter-group-name $Name

    echo "Parameter group deleted successfully"
} # delete_dbcluster_parameter_group
export -f delete_dbcluster_parameter_group

# ---------------- Blue/Green Deployment Operations ----------------------

function create_blue_green_deployment {
    BlueGreenDeploymentName=$1
    ClusterId=$2
    Engine=$3
    EngineVersion=$4

    # Get the cluster ARN to use as source
    dbClusterArn=$(aws rds describe-db-clusters \
        --db-cluster-identifier $ClusterId \
        --query 'DBClusters[0].DBClusterArn' \
        --output text)
    dbClusterArn=$(echo "$dbClusterArn" | xargs)

    if [ -z "$dbClusterArn" ]; then
        echo "Failed to get cluster ARN for cluster $ClusterId"
        exit 1
    fi

    # Handle latest engine version
    if [ "$EngineVersion" = "latest" ]; then
        AllEngineVersions=$(aws rds describe-db-engine-versions \
            --engine $Engine \
            --query "DBEngineVersions[?!contains(EngineVersion, '-limitless')].EngineVersion" \
            --output text)
        LatestVersion=$(echo $AllEngineVersions | tr ' ' '\n' | sort -V | tail -n 1)
        EngineVersion=$LatestVersion
    fi

    # Create blue-green deployment
    deploymentResult=$(aws rds create-blue-green-deployment \
        --blue-green-deployment-name $BlueGreenDeploymentName \
        --source $dbClusterArn \
        --target-engine-version $EngineVersion \
        --tags "Key=env,Value=test-runner" \
        --output json)

    # Parse and return the deployment identifier
    deploymentId=$(echo $deploymentResult | jq -r '.BlueGreenDeployment.BlueGreenDeploymentIdentifier')

    echo "$deploymentId"
} # create_blue_green_deployment
export -f create_blue_green_deployment

function wait_blue_green_deployment_ready {
    BlueGreenDeploymentId=$1

    echo "Waiting for Blue/Green Deployment $BlueGreenDeploymentId to be ready..."
    sleep 60

    echo "Performing dummy wait for Blue/Green Deployment $BlueGreenDeploymentId to be ready..., 30m/180s"
    sleep 600
    sleep 600
    sleep 600
    echo "Done dummy wait for Blue/Green Deployment $BlueGreenDeploymentId"
    exit 0

    # Retry settings
    maxRetries=30
    attempt=0
    waitSuccessful=0

    # ~30m
    while [[ $attempt -lt $maxRetries && $waitSuccessful -eq 0 ]]
    do
        ((attempt++))
        echo "Attempt $attempt / $maxRetries: Checking deployment status..."
        echo "  Current waitSuccessful: $waitSuccessful"

        # Get deployment details
        deploymentResult=$(aws rds describe-blue-green-deployments \
            --blue-green-deployment-identifier $BlueGreenDeploymentId)

        if [ $? -ne 0 ]; then
            echo "Error checking deployment status"
            exit 1
        fi

        status=$(echo "$deploymentResult" | jq -r '.BlueGreenDeployments[0].Status')
        echo "Deployment Status: $status"

        # Check if deployment is ready
        if [ "$status" = "AVAILABLE" ]; then
            echo "All tasks completed and deployment is ready for switchover."

            # Verify all switchover details are AVAILABLE
            allReady=1
            switchoverCount=$(echo "$deploymentResult" | jq '.BlueGreenDeployments[0].SwitchoverDetails | length')
            for ((i=0; i<switchoverCount; i++)); do
                switchoverStatus=$(echo "$deploymentResult" | jq -r ".BlueGreenDeployments[0].SwitchoverDetails[$i].Status")
                if [ "$switchoverStatus" != "AVAILABLE" ]; then
                    echo "Switchover detail not ready: $switchoverStatus"
                    allReady=0
                fi
            done

            if [ "$allReady" = 1 ]; then
                waitSuccessful=1
                echo "Blue/Green Deployment is ready!"
                return
            fi
        fi

        # Check if deployment is in a terminal state
        if [ "$status" = "SWITCHOVER_COMPLETED" ] || [ "$status" = "SWITCHOVER_FAILED" ] || [ "$status" = "INVALID_CONFIGURATION" ]; then
            echo "Deployment reached terminal state: $status"
            exit 1
        fi

        echo "Deployment not ready yet. Waiting..."

        if [ $attempt -lt $maxRetries ]; then
            echo "Retrying in 60 seconds..."
            sleep 60
        fi
    done

    echo "Timeout waiting for Blue/Green Deployment to be ready after $maxRetries attempts"
    echo "  Final deployment status: $status"
    exit 1
} # wait_blue_green_deployment_ready
export -f wait_blue_green_deployment_ready

function delete_blue_green_deployment {
    BlueGreenDeploymentId=$1
    Region=$2

    echo "Deleting Blue/Green Deployment $BlueGreenDeploymentId with old resources..."

    # Get deployment details to find the source cluster
    deploymentResult=$(aws rds describe-blue-green-deployments \
        --blue-green-deployment-identifier $BlueGreenDeploymentId)

    sourceArn=$(echo "$deploymentResult" | jq -r '.BlueGreenDeployments[0].Source')
    targetArn=$(echo "$deploymentResult" | jq -r '.BlueGreenDeployments[0].Target')

    # Extract cluster ID from ARN (format: arn:aws:rds:region:account:cluster:cluster-id)
    sourceClusterId=$(echo "$sourceArn" | sed 's/.*:cluster://')
    targetClusterId=$(echo "$targetArn" | sed 's/.*:cluster://')

    # Remove IP from security group
    remove_ip_from_db_sg "" $sourceClusterId $Region

    # Delete the blue-green deployment with target (green) resources
    aws rds delete-blue-green-deployment \
        --blue-green-deployment-identifier $BlueGreenDeploymentId

    echo "Blue/Green Deployment deletion initiated. Waiting for completion..."

    # Wait for deletion to complete
    # maxRetries=30
    # attempt=0
    # deleteSuccessful=0

    # while [[ $attempt -lt $maxRetries ]]
    # do
    #     ((attempt++))
    #     echo "Attempt $attempt / $maxRetries: Checking deletion status..."

    #     deploymentResult=$(aws rds describe-blue-green-deployments \
    #         --blue-green-deployment-identifier $BlueGreenDeploymentId)

    #     if [ $? -ne 0 ]; then
    #         echo "Error checking deletion status"
    #         if echo "$deploymentResult" | grep -q "does not exist"; then
    #             echo "Blue/Green Deployment has been deleted."
    #             deleteSuccessful=1
    #             break
    #         fi
    #         exit 1
    #     fi

    #     deploymentCount=$(echo "$deploymentResult" | jq '.BlueGreenDeployments | length')
    #     if [ $deploymentCount -eq 0 ]; then
    #         echo "Blue/Green Deployment has been deleted."
    #         deleteSuccessful=1
    #         break
    #     fi

    #     status=$(echo "$deploymentResult" | jq -r '.BlueGreenDeployments[0].Status')

    #     if [ "$status" = "DELETING" ]; then
    #         echo "Deployment is still being deleted. Waiting..."
    #         sleep 15
    #     elif [ "$status" = "DELETED" ] || [ $deploymentCount -eq 0 ]; then
    #         echo "Blue/Green Deployment deleted successfully."
    #         deleteSuccessful=1
    #         break
    #     elif [ "$status" = "SWITCHOVER_COMPLETED" ] || [ "$status" = "SWITCHOVER_FAILED" ]; then
    #         echo "Deployment reached terminal state: $status"
    #         exit 1
    #     else
    #         echo "Unexpected status: $status"
    #     fi

    #     if [ $attempt -lt $maxRetries ]; then
    #         echo "Retrying in 60 seconds..."
    #         sleep 60
    #     fi
    # done

    # if [ $deleteSuccessful -eq 0 ]; then
    #     echo "Failed to delete Blue/Green Deployment after $maxRetries attempts."
    #     exit 1
    # fi

    # NOTE - BG Deployment deleted does not ensure that clusters are deletable due to replication lag
    echo "Sleep to ensure BG Deletion for 30m / 1800s..."
    sleep 1800
    delete_aurora_db_cluster $targetClusterId $Region
    delete_aurora_db_cluster $sourceClusterId $Region
    # NOTE - Additional time for clusters to delete fully for parameter group deletion later
    echo "Sleep to ensure Cluster Deletion for 30m / 1800s..."
    sleep 1800

    echo "Successfully deleted Blue/Green Deployment $BlueGreenDeploymentId and associated resources."
} # delete_blue_green_deployment
export -f delete_blue_green_deployment
