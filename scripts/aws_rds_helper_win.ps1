<#
Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
/#>

# ---------------- Create DB operations ----------------------
function Create-Aurora-RDS-Cluster {
    [OutputType([String])]
    Param(
        [Parameter(Mandatory=$true)]
        [string]$TestUsername,
        # The path to the signed file
        [Parameter(Mandatory=$true)]
        [string]$TestPassword,
        # Test Database
        [Parameter(Mandatory=$true)]
        [string]$TestDatabase,
        # Cluster Id
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        # NumInstances
        [Parameter(Mandatory=$true)]
        [int]$NumInstances,
        # The name of the signed AWS bucket
        [Parameter(Mandatory=$true)]
        [string]$Engine,
        # Engine version
        [Parameter(Mandatory=$true)]
        [string]$EngineVersion,
        [Parameter(Mandatory=$true)]
        [string]$Region,
        [Parameter()]
        [string]$ParameterGroup

    )

    Write-Host "Creating RDS Cluster"

    # Find latest engine version
    if ($EngineVersion -eq "latest") {
        $AllEngineVersions = aws rds describe-db-engine-versions `
            --engine $Engine `
            --query "DBEngineVersions[?!contains(EngineVersion, '-limitless')].EngineVersion" `
            --output json | ConvertFrom-Json
        $LatestVersion = $AllEngineVersions | Sort-Object {$_} -Descending | Select-Object -First 1
        $EngineVersion = $LatestVersion
        Write-Host "Using Latest Version: $EngineVersion"
    }

    # Create RDS Cluster
    if ($ParameterGroup) {
        $ClusterInfo = $( aws rds create-db-cluster `
                            --db-cluster-identifier $ClusterId `
                            --database-name $TestDatabase `
                            --master-username $TestUsername `
                            --master-user-password $TestPassword `
                            --source-region $Region `
                            --enable-iam-database-authentication `
                            --engine  $Engine `
                            --engine-version $EngineVersion `
                            --storage-encrypted `
                            --tags "Key=env,Value=test-runner" `
                            --db-cluster-parameter-group-name $ParameterGroup)
    } else {
        $ClusterInfo = $( aws rds create-db-cluster `
                            --db-cluster-identifier $ClusterId `
                            --database-name $TestDatabase `
                            --master-username $TestUsername `
                            --master-user-password $TestPassword `
                            --source-region $Region `
                            --enable-iam-database-authentication `
                            --engine  $Engine `
                            --engine-version $EngineVersion `
                            --storage-encrypted `
                            --tags "Key=env,Value=test-runner")
    }


    for($i=1; $i -le $NumInstances; $i++) {
        aws rds create-db-instance `
            --db-cluster-identifier $ClusterId `
            --db-instance-identifier  "${ClusterId}-${i}" `
            --db-instance-class "db.r5.large" `
            --engine $Engine `
            --engine-version $EngineVersion `
            --publicly-accessible `
            --tags "Key=env,Value=test-runner"
    }

    aws rds wait db-instance-available `
        --filters "Name=db-cluster-id,Values=${ClusterId}"

    Add-Ip-To-Db-Sg -ClusterId $ClusterId -Region $Region

    # Sleep to ensure IP is added and propagated to DBs before moving onto next step
    Start-Sleep -Seconds 60
}

function Create-Limitless-RDS-Cluster {
    [OutputType([String])]
    Param(
        # The path to the file to sign
        [Parameter(Mandatory=$true)]
        [string]$TestUsername,
        # The path to the signed file
        [Parameter(Mandatory=$true)]
        [string]$TestPassword,
        [Parameter(Mandatory=$true)]
        [string]$TestDatabase,
        # Cluster Id
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        # Shard Id
        [Parameter(Mandatory=$true)]
        [string]$ShardId,
        # The name of the signed AWS bucket
        [Parameter(Mandatory=$true)]
        [string]$Engine,
        # Engine version
        [Parameter(Mandatory=$true)]
        [string]$EngineVersion,
        # Aws Monitoring Arn
        [Parameter(Mandatory=$true)]
        [string]$AwsRdsMonitoringRoleArn,
        [Parameter(Mandatory=$true)]
        [string]$Region

    )

    Write-Host "Creating Limitless RDS Cluster"

    # Find latest engine version
    if ($EngineVersion -eq "latest") {
        $AllEngineVersions = aws rds describe-db-engine-versions `
            --engine $Engine `
            --query "DBEngineVersions[?contains(EngineVersion, '-limitless')].EngineVersion" `
            --output json | ConvertFrom-Json
        $LatestVersion = $AllEngineVersions | Sort-Object {$_} -Descending | Select-Object -First 1
        $EngineVersion = $LatestVersion
        Write-Host "Using Latest Version: $EngineVersion"
    }

    # Create RDS Cluster
    $ClusterInfo = $( aws rds create-db-cluster `
                        --cluster-scalability-type "limitless" `
                        --db-cluster-identifier $ClusterId `
                        --master-username $TestUsername `
                        --master-user-password $TestPassword `
                        --region $Region `
                        --engine  $Engine `
                        --engine-version $EngineVersion `
                        --enable-cloudwatch-logs-export "postgresql" `
                        --enable-iam-database-authentication `
                        --enable-performance-insights `
                        --monitoring-interval 5 `
                        --performance-insights-retention-period 31 `
                        --monitoring-role-arn $AwsRdsMonitoringRoleArn `
                        --storage-type "aurora-iopt1" `
                        --tags "Key=env,Value=test-runner")

    Write-Host "Creating Limitless Shard Group"

    $ShardInfo = $( aws rds create-db-shard-group `
                        --db-cluster-identifier $ClusterId `
                        --db-shard-group-identifier $ShardId `
                        --min-acu 28.0 `
                        --max-acu 601.0 `
                        --publicly-accessible `
                        --tags "Key=env,Value=test-runner")

    # Wait for availability for limitless
    $maxRetries = 2
    $attempt = 0
    $waitSuccessful = $false

    # Retry logic
    while ($attempt -le $maxRetries -and -not $waitSuccessful) {
        try {
            Write-Host "Attempt $($attempt + 1): Checking DB cluster availability..."

            # Call the AWS CLI to wait for DB cluster to be available
            $result = aws rds wait db-cluster-available --db-cluster-identifier $ClusterId

            # If the command succeeds, it will return nothing, so we set the success flag
            $waitSuccessful = $true
            Write-Host "DB Cluster is now available."

        } catch {

            Write-Host "Error: DB cluster is not available. Attempt $($attempt + 1) failed."
            $attempt++
            if ($attempt -le $maxRetries) {
                Write-Host "Retrying... ($($maxRetries - $attempt) retries left)"
                Start-Sleep -Seconds 30  # Wait for 30 seconds before retrying
            }
        }
    }

    if (-not $waitSuccessful) {
        Write-Host "Failed to wait for DB cluster availability after $($maxRetries + 1) attempts."

        throw [System.Exception] "Failed to wait for Cluster availability"

    } else {
        Write-Host "Successfully detected DB cluster availability."
    }

    Add-Ip-To-Db-Sg -ClusterId $ClusterId -Region $Region
}

# Creates a custom endpoint with half of the instances in the static or excluded list
function Create-Custom-Endpoint {
    param(
        [Parameter(Mandatory=$true)]
        [string]$EndpointName,
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [int]$NumInstances,
        [switch]$StaticList
    )

    $InstancesList = (1..($NumInstances / 2) | ForEach-Object { "$ClusterId-$_".ToLower() })

    # Before modifying, wait until ready
    aws rds wait db-cluster-available --db-cluster-identifier ${ClusterId}

    # Create custom endpoints
    if ($StaticList) {
        aws rds create-db-cluster-endpoint `
            --db-cluster-endpoint-identifier ${EndpointName} `
            --db-cluster-identifier ${ClusterId} `
            --endpoint-type any `
            --static-members ${InstancesList}
    } else {
        aws rds create-db-cluster-endpoint `
            --db-cluster-endpoint-identifier ${EndpointName} `
            --db-cluster-identifier ${ClusterId} `
            --endpoint-type any `
            --excluded-members ${InstancesList}
    }

    # Wait for 30 seconds to allow cluster to update status
    Start-Sleep -Seconds 30

    # Wait until create operation completes
    aws rds wait db-cluster-available --db-cluster-identifier ${ClusterId}
}

# ---------------- Security Group Operations ----------------------
function Add-Ip-To-Db-Sg {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [string]$Region
    )

    # Get the security group associated with the DB cluster using AWS CLI
    $dbClusterInfo = aws rds describe-db-clusters --db-cluster-identifier $ClusterId --region $Region | ConvertFrom-Json

    # Check if we got the DB Cluster information
    if ($dbClusterInfo.DBClusters.Count -eq 0) {
        Write-Host "Error: DB Cluster with ID '$ClusterId' not found in region '$Region'."
        return
    }

    $securityGroupId = $dbClusterInfo.DBClusters[0].VpcSecurityGroups[0].VpcSecurityGroupId

    # Get the local IP address of the machine running the script
    $localIp = (Invoke-RestMethod -Uri "http://checkip.amazonaws.com").Trim()
    $cidrBlock = "$localIp/32"

    # Allow inbound traffic from the local IP address to the DB security group using AWS CLI
    $authorizeResult = aws ec2 authorize-security-group-ingress --group-id $securityGroupId --protocol tcp --cidr $cidrBlock --port 0-65535 --region $Region

    # Check if the ingress rule was successfully added
    if ($?) {
        Write-Host "Inbound traffic allowed from IP $localIp to security group $securityGroupId on all ports."
    } else {
        Write-Host "Failed to add inbound rule to security group $securityGroupId."
    }
}

function Remove-Ip-From-Db-Sg {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [string]$Region
    )

    # Get the security group associated with the DB cluster using AWS CLI
    $dbClusterInfo = aws rds describe-db-clusters --db-cluster-identifier $ClusterId --region $Region | ConvertFrom-Json

    # Check if we got the DB Cluster information
    if ($dbClusterInfo.DBClusters.Count -eq 0) {
        Write-Host "Error: DB Cluster with ID '$ClusterId' not found in region '$Region'."
        return
    }

    # Extract the Security Group ID
    $securityGroupId = $dbClusterInfo.DBClusters[0].VpcSecurityGroups[0].VpcSecurityGroupId

    # Get the local IP address of the machine running the script
    $localIp = (Invoke-RestMethod -Uri "http://checkip.amazonaws.com").Trim()

    # Define the CIDR block for the local IP address (allowing all ports and all traffic)
    $cidrBlock = "$localIp/32"

    # Revoke Inbound traffic
    $authorizeResult = aws ec2 revoke-security-group-ingress --group-id $securityGroupId --protocol tcp --cidr $cidrBlock --port 0-65535 --region $Region

    # Check if revoked successfully
    if ($?) {
        Write-Host "Removed IP: $localIp from security group $securityGroupId."
    } else {
        Write-Host "Failed to remove ip $localIp from security group $securityGroupId."
    }
}

# ---------------- Db Deletion operations ----------------------
function Delete-DBShards {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ShardId
    )
    try {
        Write-Host "Attempt $($attempt + 1): Deleting DB shard $ShardId..."

        # AWS CLI command to delete a DB shard
        $result = aws rds delete-db-shard-group --db-shard-group-identifier $ShardId

        Write-Host "DB Shard $ShardId deleted successfully."
        return $true
    } catch {
        Write-Host "Error deleting DB shard $ShardId. Attempt $($attempt + 1) failed."
        return $false
    }
}

function Delete-DBCluster {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId
    )
    try {
        Write-Host "Attempt $($attempt + 1): Deleting DB cluster $ClusterId..."

        # AWS CLI command to delete the DB cluster
        $result = aws rds delete-db-cluster --db-cluster-identifier $ClusterId --skip-final-snapshot

        Write-Host "DB Cluster $ClusterId deletion initiated."
        return $true
    } catch {
        Write-Host "Error deleting DB cluster $ClusterId. Attempt $($attempt + 1) failed."
        return $false
    }
}

function Delete-DBInstances {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId
    )

    Write-Host "Deleting DBInstance"
    $instances = (aws rds describe-db-clusters `
        --db-cluster-identifier "$ClusterId" `
        --query 'DBClusters[0].DBClusterMembers[].DBInstanceIdentifier' `
        --output json) | ConvertFrom-Json

    Write-Host $instances

    foreach ($instance in $instances)
    {
        Write-Host "Deleting: $instance"
        aws rds delete-db-instance --skip-final-snapshot --db-instance-identifier "$instance"
    }
}

function Delete-Aurora-Db-Cluster {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [string]$Region
    )
    Remove-Ip-From-Db-Sg -ClusterId $ClusterId -Region $Region
    Delete-DBInstances -ClusterId $ClusterId
    Delete-DBCluster -ClusterId $ClusterId
}

function Delete-Limitless-Db-Cluster {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [string]$ShardId,
        [Parameter(Mandatory=$true)]
        [string]$Region
    )
    Remove-Ip-From-Db-Sg -ClusterId $ClusterId -Region $Region
    # Retry settings
    $maxRetries = 5
    $attempt = 0
    $deleteShardsSuccessful = $false
    $deleteClusterSuccessful = $false

    # Retry logic for deleting DB shards
    while ($attempt -lt $maxRetries -and -not $deleteShardsSuccessful) {
        $deleteShardsSuccessful = Delete-DBShards -ShardId $ShardId
        $attempt++
        if (-not $deleteShardsSuccessful) {
            if ($attempt -lt $maxRetries) {
                Write-Host "Retrying DB shard deletion... ($($maxRetries - $attempt) retries left)"
                Start-Sleep -Seconds 30  # Wait for 30 seconds before retrying
            }
        }
    }

    if (-not $deleteShardsSuccessful) {
        Write-Host "Failed to delete DB shard $ShardId after $maxRetries attempts."
    } else {
        Write-Host "Successfully deleted DB shard $ShardId."
    }

    # Reset attempt counter for DB cluster deletion
    $attempt = 0

    # Retry logic for deleting DB cluster
    while ($attempt -lt $maxRetries -and -not $deleteClusterSuccessful) {
        $deleteClusterSuccessful = Delete-DBCluster -ClusterId $ClusterId
        $attempt++
        if (-not $deleteClusterSuccessful) {
            if ($attempt -lt $maxRetries) {
                Write-Host "Retrying DB cluster deletion... ($($maxRetries - $attempt) retries left)"
                Start-Sleep -Seconds 30  # Wait for 30 seconds before retrying
            }
        }
    }

    if (-not $deleteClusterSuccessful) {
        Write-Host "Failed to delete DB cluster $ClusterId after $maxRetries attempts."
    } else {
        Write-Host "Successfully deleted DB cluster $ClusterId."
    }
}

function Delete-Custom-Endpoint {
    param (
        [Parameter(Mandatory=$true)]
        [string]$EndpointName,
        [Parameter(Mandatory=$true)]
        [string]$ClusterId
    )
    # Delete Custom Endpoint
    aws rds delete-db-cluster-endpoint --db-cluster-endpoint-identifier $EndpointName

    # Wait until delete operation completes
    aws rds wait db-cluster-available --db-cluster-identifier ${ClusterId}
}

# ---------------- Get Cluster endpoint ----------------------
function Get-Cluster-Endpoint {
    param (
        [Parameter(Mandatory=$true)]
        [string]$ClusterId
    )
    return aws rds describe-db-clusters --db-cluster-identifier $ClusterId --query DBClusters[0].Endpoint --output text
}

function Describe-Cluster-Endpoint {
    param (
        [Parameter(Mandatory=$true)]
        [string]$ClusterId
    )
    aws rds describe-db-clusters --db-cluster-identifier $ClusterId
}

# ---------------- Secrets Manager Operations ----------------------
function Create-Db-Secrets {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Username,
        [Parameter(Mandatory=$true)]
        [string]$Password,
        [Parameter(Mandatory=$true)]
        [string]$Engine,
        [Parameter(Mandatory=$true)]
        [string]$ClusterEndpoint
    )

    $randomNumber = Get-Random -Minimum 1000 -Maximum 9999
    $randomNumber = $randomNumber.ToString()
    # Define the secret name (you can adjust this if you want a different name)
    $secretName = "AWS-ODBC-Tests-$ClusterEndpoint-$randomNumber"

    # Create a dictionary to hold key-value pairs for the secret
    $secretValue = @{
        "username" = $Username
        "password" = $Password
        "engine" = $Engine
        "host" = $ClusterEndpoint
    }

    $jsonSecretValue = $secretValue | ConvertTo-Json
    $createSecretCommand = aws secretsmanager create-secret `
        --name $secretName `
        --description "Secrets created by GH actions for AWS PostgreSQL ODBC" `
        --secret-string $jsonSecretValue

    # Parse the ARN of the newly created secret from the output
    $secretArn = ($createSecretCommand | ConvertFrom-Json).ARN
    return $secretArn
}

function Delete-Secrets {
    param(
        [Parameter(Mandatory=$true)]
        [string]$SecretsArn
    )
    aws secretsmanager delete-secret --secret-id $SecretsArn
}

# ---------------- Parameter Group Operations ----------------------

function New-DbClusterParameterGroup {
    [OutputType([String])]
    Param(
        [Parameter(Mandatory=$true)]
        [string]$Name,
        [Parameter(Mandatory=$true)]
        [string]$Engine,
        [Parameter(Mandatory=$true)]
        [string]$EngineVersion
    )

    Write-Host "Creating custom parameter group: $Name"

    # Handle latest engine version
    if ($EngineVersion -eq "latest") {
        $AllEngineVersions = aws rds describe-db-engine-versions `
            --engine $Engine `
            --query "DBEngineVersions[?!contains(EngineVersion, '-limitless')].EngineVersion" `
            --output json | ConvertFrom-Json
        $LatestVersion = $AllEngineVersions | Sort-Object {$_} -Descending | Select-Object -First 1
        $EngineVersion = $LatestVersion
        Write-Host "Using Latest Version: $EngineVersion"
    }

    # Determine parameter family
    $paramFamily = if ($Engine -like "*mysql*") {
        "aurora-mysql-$EngineVersion"
    } elseif ($Engine -like "*postgresql*") {
        $EngineVersion = ($EngineVersion -split '\.')[0]
        Write-Host "Updated Latest Version to remove minor: $EngineVersion"
        "aurora-postgresql$EngineVersion"
    } else {
        throw "Unsupported engine: $Engine"
    }

    # Create parameter group
    $createResult = aws rds create-db-cluster-parameter-group `
        --db-cluster-parameter-group-name $Name `
        --db-parameter-group-family $paramFamily `
        --description "Custom parameter group for Blue/Green Deployment testing"

    if (-not $createResult) {
        throw "Failed to create parameter group"
    }

    Write-Host "Parameter group created successfully"

    # Set engine-specific parameters
    if ($Engine -like "*mysql*") {
        Write-Host "Setting MySQL parameter: binlog_format=ROW"
        $modifyResult = aws rds modify-db-cluster-parameter-group `
            --db-cluster-parameter-group-name $Name `
            --parameters "ParameterName=binlog_format,ParameterValue=ROW,ApplyMethod=pending-reboot"
    } elseif ($Engine -like "*postgresql*") {
        Write-Host "Setting PostgreSQL parameter: rds.logical_replication=1"
        $modifyResult = aws rds modify-db-cluster-parameter-group `
            --db-cluster-parameter-group-name $Name `
            --parameters "ParameterName=rds.logical_replication,ParameterValue=1,ApplyMethod=pending-reboot"
    }

    if (-not $modifyResult) {
        throw "Failed to modify parameter group"
    }

    Write-Host "Parameter group configured successfully"
    return $Name
}

function Attach-DbClusterParameterGroup {
    [OutputType([String])]
    Param(
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [string]$ParameterGroupName
    )

    Write-Host "Attaching parameter group $ParameterGroupName to cluster $ClusterId"

    $result = aws rds modify-db-cluster `
        --db-cluster-identifier $ClusterId `
        --db-cluster-parameter-group-name $ParameterGroupName `
        --apply-immediately

    if (-not $result) {
        throw "Failed to attach parameter group to cluster"
    }

    $writer_instance = aws rds describe-db-clusters `
        --db-cluster-identifier $ClusterId `
        --query 'DBClusters[0].DBClusterMembers[?IsClusterWriter==`true`].DBInstanceIdentifier' `
        --output text
    $result = aws rds reboot-db-instance `
        --db-instance-identifier $writer_instance

    if (-not $result) {
        throw "Failed to reboot cluster"
    }

    $result = aws rds wait db-cluster-available --db-cluster-identifier $ClusterId

    $result = aws rds wait db-instance-available `
        --filters "Name=db-cluster-id,Values=${ClusterId}"

    Write-Host "Parameter group attached successfully"
    return $ClusterId
}

function Delete-DbClusterParameterGroup {
    [OutputType([String])]
    Param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )

    Write-Host "Deleting parameter group: $Name"

    $result = aws rds delete-db-cluster-parameter-group `
        --db-cluster-parameter-group-name $Name

    Write-Host "Parameter group deleted successfully"
    return $true
}

# ---------------- Blue/Green Deployment Operations ----------------------

function Create-BlueGreen-Deployment {
    [OutputType([String])]
    Param(
        [Parameter(Mandatory=$true)]
        [string]$BlueGreenDeploymentName,
        [Parameter(Mandatory=$true)]
        [string]$ClusterId,
        [Parameter(Mandatory=$true)]
        [string]$Engine,
        [Parameter(Mandatory=$true)]
        [string]$EngineVersion
    )

    Write-Host "Creating Blue/Green Deployment"

    # Get the cluster ARN to use as source
    $dbClusterInfo = aws rds describe-db-clusters --db-cluster-identifier $ClusterId --query 'DBClusters[0].DBClusterArn' --output text
    $clusterArn = $dbClusterInfo.Trim()

    if (-not $clusterArn) {
        throw [System.Exception] "Failed to get cluster ARN for cluster $ClusterId"
    }

    Write-Host "Source Cluster ARN: $clusterArn"

    # Handle latest engine version
    if ($EngineVersion -eq "latest") {
        $AllEngineVersions = aws rds describe-db-engine-versions `
            --engine $Engine `
            --query "DBEngineVersions[?!contains(EngineVersion, '-limitless')].EngineVersion" `
            --output json | ConvertFrom-Json
        $LatestVersion = $AllEngineVersions | Sort-Object {$_} -Descending | Select-Object -First 1
        $EngineVersion = $LatestVersion
        Write-Host "Using Latest Version: $EngineVersion"
    }

    # Create blue-green deployment
    $deploymentResult = aws rds create-blue-green-deployment `
        --blue-green-deployment-name $BlueGreenDeploymentName `
        --source $clusterArn `
        --target-engine-version $EngineVersion `
        --tags "Key=env,Value=test-runner"

    # Parse and return the deployment identifier
    $deploymentJson = $deploymentResult | ConvertFrom-Json
    $deploymentId = $deploymentJson.BlueGreenDeployment.BlueGreenDeploymentIdentifier

    Write-Host "Blue/Green Deployment Created: $deploymentId"
    return $deploymentId
}

function Wait-BlueGreen-Deployment-Ready {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$BlueGreenDeploymentId
    )

    Write-Host "Waiting for Blue/Green Deployment $BlueGreenDeploymentId to be ready..."
    Start-Sleep -Seconds 60

    # Retry settings
    $maxRetries = 30
    $attempt = 0
    $waitSuccessful = $false

    # ~30m
    while ($attempt -lt $maxRetries -and -not $waitSuccessful) {
        $attempt++
        Write-Host "Attempt $($attempt) / ${maxRetries}: Checking deployment status..."

        try {
            # Get deployment details
            $deploymentResult = aws rds describe-blue-green-deployments `
                --blue-green-deployment-identifier $BlueGreenDeploymentId

            $deploymentJson = $deploymentResult | ConvertFrom-Json
            $deployment = $deploymentJson.BlueGreenDeployments[0]

            $status = $deployment.Status
            Write-Host "Deployment Status: $status"

            # Check if deployment is ready
            if ($status -eq "AVAILABLE") {
                Write-Host "All tasks completed and deployment is ready for switchover."

                # Verify all switchover details are AVAILABLE
                $allReady = $true
                foreach ($switchover in $deployment.SwitchoverDetails) {
                    if ($switchover.Status -ne "AVAILABLE") {
                        Write-Host "Switchover detail not ready: $($switchover.SourceMember) - $($switchover.Status)"
                        $allReady = $false
                    }
                }

                if ($allReady) {
                    $waitSuccessful = $true
                    Write-Host "Blue/Green Deployment is ready!"
                    return
                }
            }

            # Check if deployment is in a terminal state
            if ($status -eq "SWITCHOVER_COMPLETED" -or $status -eq "SWITCHOVER_FAILED" -or $status -eq "INVALID_CONFIGURATION") {
                Write-Host "Deployment reached terminal state: $status"
                throw [System.Exception] "Blue/Green deployment reached terminal state: $status"
            }

            Write-Host "Deployment not ready yet. Waiting..."

        } catch {
            Write-Host "Error checking deployment status: $($_.Exception.Message)"
            throw [System.Exception] "Failed to check deployment status"
        }

        if ($attempt -lt $maxRetries) {
            Write-Host "Retrying in 60 seconds..."
            Start-Sleep -Seconds 60
        }
    }

    throw [System.Exception] "Timeout waiting for Blue/Green Deployment to be ready after $maxRetries attempts"
}

function Delete-BlueGreen-Deployment {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$BlueGreenDeploymentId,
        [Parameter(Mandatory=$true)]
        [string]$Region
    )

    Write-Host "Deleting Blue/Green Deployment $BlueGreenDeploymentId with old resources..."

    # Get deployment details to find the source cluster
    $deploymentResult = aws rds describe-blue-green-deployments `
        --blue-green-deployment-identifier $BlueGreenDeploymentId

    $deploymentJson = $deploymentResult | ConvertFrom-Json
    $deployment = $deploymentJson.BlueGreenDeployments[0]
    $sourceArn = $deployment.Source
    $targetArn = $deployment.Target

    # Extract cluster ID from ARN (format: arn:aws:rds:region:account:cluster:cluster-id)
    $sourceClusterId = $sourceArn -replace '.*:cluster:', ''
    $targetClusterId = $targetArn -replace '.*:cluster:', ''

    # Remove IP from security group
    Remove-Ip-From-Db-Sg -ClusterId $sourceClusterId -Region $Region

    # Delete the blue-green deployment with target (green) resources
    aws rds delete-blue-green-deployment `
        --blue-green-deployment-identifier $BlueGreenDeploymentId

    Write-Host "Blue/Green Deployment deletion initiated. Waiting for completion..."

    # Wait for deletion to complete
    $maxRetries = 30
    $attempt = 0
    $deleteSuccessful = $false

    while ($attempt -lt $maxRetries) {
        $attempt++
        Write-Host "Attempt $($attempt) / ${maxRetries}: Checking deletion status..."

        try {
            $deploymentResult = aws rds describe-blue-green-deployments `
                --blue-green-deployment-identifier $BlueGreenDeploymentId

            $deploymentJson = $deploymentResult | ConvertFrom-Json
            if ($deploymentJson.BlueGreenDeployments.Count -eq 0) {
                Write-Host "Blue/Green Deployment has been deleted."
                $deleteSuccessful = $true
                break
            }

            $deployment = $deploymentJson.BlueGreenDeployments[0]
            $status = $deployment.Status

            if ($status -eq "DELETING") {
                Write-Host "Deployment is still being deleted. Waiting..."
                Start-Sleep -Seconds 15
            } elseif ($status -eq "DELETED" -or $deploymentJson.BlueGreenDeployments.Count -eq 0) {
                Write-Host "Blue/Green Deployment deleted successfully."
                $deleteSuccessful = $true
                break
            } elseif ($status -eq "SWITCHOVER_COMPLETED" -or $status -eq "SWITCHOVER_FAILED") {
                Write-Host "Deployment reached terminal state: $status"
                throw [System.Exception] "Cannot delete deployment in state: $status"
            } else {
                Write-Host "Unexpected status: $status"
            }

        } catch {
            Write-Host "Error checking deletion status: $($_.Exception.Message)"
            if ($_.Exception.Message -match "does not exist") {
                Write-Host "Blue/Green Deployment has been deleted."
                $deleteSuccessful = $true
                break
            }
            throw [System.Exception] "Failed to check deletion status"
        }

        if ($attempt -lt $maxRetries) {
            Write-Host "Retrying in 60 seconds..."
            Start-Sleep -Seconds 60
        }
    }

    if (-not $deleteSuccessful) {
        Write-Host "Failed to delete Blue/Green Deployment after $maxRetries attempts."
        throw [System.Exception] "Failed to delete Blue/Green Deployment"
    }

    # NOTE - BG Deployment deleted does not ensure that clusters are deletable due to replication lag
    Write-Host "Sleep to ensure BG Deletion for 30m / 1800s..."
    Start-Sleep -Seconds 1800
    Delete-Aurora-Db-Cluster -ClusterId $targetArn -Region $Region
    Delete-Aurora-Db-Cluster -ClusterId $sourceArn -Region $Region
    # NOTE - Additional time for clusters to delete fully for parameter group deletion later
    Write-Host "Sleep to ensure Cluster Deletion for 10m / 600s..."
    Start-Sleep 600

    Write-Host "Successfully deleted Blue/Green Deployment $BlueGreenDeploymentId and associated resources."
}
