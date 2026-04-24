## Blue Green Deployment Plugin for the AWS Advanced ODBC Wrapper

### What is Blue/Green Deployment?

The [Blue/Green Deployment](https://docs.aws.amazon.com/whitepapers/latest/blue-green-deployments/introduction.html) technique enables organizations to release applications by seamlessly shifting traffic between two identical environments running different versions of the application. This strategy effectively mitigates common risks associated with software deployment, such as downtime and limited rollback capability.

The AWS Advanced ODBC Wrapper leverages the Blue/Green Deployment approach by intelligently managing traffic distribution between blue and green nodes, minimizing the impact of stale DNS data and connectivity disruptions on user applications.

### Prerequisites
- AWS cluster and instance endpoints must be directly accessible from the client side
- :warning: Extra permissions are required for non-admin users so that the blue/green metadata table/function can be properly queried. If the permissions are not granted, the metadata table/function will not be visible and blue/green plugin functionality will not work properly. Please see the [Connecting with non-admin users](#connecting-with-non-admin-users) section below.

> [!WARNING]\
> Currently Supported Database Deployments:
> - Aurora MySQL and PostgreSQL clusters
>
> Unsupported Database Deployments and Configurations:
> - RDS MySQL and PostgreSQL Multi-AZ clusters
> - Aurora Global Database for MySQL and PostgreSQL
>
> Additional Requirements:
> - Connecting to database nodes using CNAME aliases is not supported
>
> **Blue/Green Support Behaviour and Version Compatibility:**
>
> Supported Aurora PostgreSQL Versions: Engine Release `17.5, 16.9, 15.13, 14.18, 13.21` and above.<br>

### What is Blue/Green Deployment Plugin?

During a [Blue/Green switchover](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/blue-green-deployments-switching.html), several significant changes occur to your database configuration:
- Connections to blue nodes terminate at a specific point during the transition
- Node connectivity may be temporarily impacted due to reconfigurations and potential node restarts
- Cluster and instance endpoints are redirected to different database nodes
- Internal database node names undergo changes
- Internal security certificates are regenerated to accommodate the new node names

All factors mentioned above may cause application disruption. The AWS Advanced ODBC Wrapper aims to minimize the application disruption during Blue/Green switchover by performing the following actions:
- Actively monitors Blue/Green switchover status and implements appropriate measures to suspend, pass-through, or re-route database traffic
- Prior to Blue/Green switchover initiation, compiles a comprehensive inventory of cluster and instance endpoints for both blue and green nodes along with their corresponding IP addresses
- During the active switchover phase, temporarily suspends execution of ODBC calls to blue nodes, which helps unload database nodes and reduces transaction lag for green nodes, thereby enhancing overall switchover performance
- Substitutes provided hostname with corresponding IP addresses when establishing new blue connections, effectively eliminating stale DNS data and ensuring connections to current blue nodes
- During the brief post-switchover period, continuously monitors DNS entries, confirms that blue endpoints have been reconfigured, and discontinues hostname-to-IP address substitution as it becomes unnecessary
- Automatically rejects new connection requests to green nodes when the switchover is completed but DNS entries for green nodes remain temporarily available
- Intelligently detects switchover failures and rollbacks to the original state, implementing appropriate connection handling measures to maintain application stability

### Blue/Green Deployment Plugin Parameters

| Field                                 | Connection Option Key     | Value                                                                                                                                                                                                                                                                                                                                                                                                                                                     | Default Value | Sample Value  |
|---------------------------------------|---------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|---------------|
| Enable Blue Green Deployment Plugin   | `ENABLE_BLUE_GREEN`       | Set to `1` to enable the Blue Green Deployment Plugin.                                                                                                                                                                                                                                                                                                                                                                                                    | `0`           | `1`           |
| Blue Green Deployment ID              | `BG_ID`                   | This parameter is optional and defaults to `1`. When supporting multiple Blue/Green Deployments (BGDs), this parameter becomes mandatory. Each connection string must include the `BG_ID` parameter with a value that can be any number or string. However, all connection strings associated with the same Blue/Green Deployment must use identical `BG_ID` values, while connection strings belonging to different BGDs must specify distinct values.   | `BG-1`        | `1`           |
| Blue Green Connect Timeout            | `BG_CONNECT_TIMEOUT_MS`   | Maximum waiting time (in milliseconds) for establishing new connections during a Blue/Green switchover when blue and green traffic is temporarily suspended.                                                                                                                                                                                                                                                                                              | `30000`       | `60000`       |
| Custom Endpoint Monitor Refresh       | `BG_BASELINE_REFRESH_MS`  | The baseline interval (ms) for checking Blue/Green Deployment status. It's highly recommended to keep this parameter below 900000ms (15 minutes).                                                                                                                                                                                                                                                                                                         | `60000`       | `90000`       |
| Max Custom Endpoint Monitor Refresh   | `BG_INCREASED_REFRESH_MS` | The increased interval (ms) for checking Blue/Green Deployment status. Configure this parameter within the range of 500-2000 milliseconds.                                                                                                                                                                                                                                                                                                                | `1000`        | `2000`        |
| Throttle Exponential Backoff Rate     | `BG_HIGH_REFRESH_MS`      | The high-frequency interval (ms) for checking Blue/Green Deployment status.    Configure this parameter within the range of 50-500 milliseconds.                                                                                                                                                                                                                                                                                                          | `100`         | `500`         |
| Wait for Custom Endpoint Info Timeout | `BG_SWITCH_TIMEOUT_MS`    | Maximum duration (in milliseconds) allowed for switchover completion. If the switchover process stalls or exceeds this timeframe, the driver will automatically assume completion and resume normal operations.                                                                                                                                                                                                                                           | `180000`      | `360000`      |
