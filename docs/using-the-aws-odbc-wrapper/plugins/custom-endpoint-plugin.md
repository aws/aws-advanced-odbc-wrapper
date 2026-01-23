## Custom Endpoint Plugin for the AWS Advanced ODBC Wrapper

### What are Custom Endpoints?

RDS Custom Endpoints are user defined subsets of DB instances. For more information, refer to the [AWS Custom Endpoints documentation](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/Aurora.Endpoints.Custom.html).
The Custom Endpoint Plugin adds support for RDS custom endpoints, such that connections respect the specified subsets when used in conjunction with other plugins, such as the [Failover Plugin](./failover-plugin.md).

### Enable Custom Endpoints

1. Create or use an existing custom endpoint with the AWS RDS Console. The endpoint should be used as the `SERVER` value. Documentation on creating a custom endpoint can be found [here](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/aurora-custom-endpoint-creating.html).
1. Set the [`ENABLE_CUSTOM_ENDPOINT`](#custom-endpoint-plugin-parameters) parameter value to `1`.

### Custom Endpoint Plugin Parameters

| Field                                 | Connection Option Key                     | Value                                                                                                                                                                                                                                                                     | Default Value | Sample Value  |
|---------------------------------------|-------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|---------------|
| Enable Custom Endpoint                | `ENABLE_CUSTOM_ENDPOINT`                  | Set to `1` to enable the Custom Endpoint Plugin.                                                                                                                                                                                                                          | `0`           | `1`           |
| Wait for Custom Endpoint Info         | `WAIT_FOR_CUSTOM_ENDPOINT_INFO`           | Set to `1` for connections and executions to wait for custom endpoint information. Waiting is only necessary if a connection's custom endpoint information has not been set recently. Disabling may result in connecting to instances out of the custom endpoint subset.  | `0`           | `1`           |
| Custom Endpoint Region                | `CUSTOM_ENDPOINT_REGION`                  | The Custom Endpoint's region. If not specified, the region will be parsed from the server's host.                                                                                                                                                                         | nil           | `us-west-1`   |
| Custom Endpoint Monitor Refresh       | `CUSTOM_ENDPOINT_MONITOR_INTERVAL_MS`     | Interval to fetch for custom endpoint information in milliseconds.                                                                                                                                                                                                        | `30000`       | `10000`       |
| Max Custom Endpoint Monitor Refresh   | `CUSTOM_ENDPOINT_MAX_MONITOR_INTERVAL_MS` | The maximum time between fetching custom endpoint information in milliseconds.                                                                                                                                                                                            | `300000`      | `100000`      |
| Throttle Exponential Backoff Rate     | `CUSTOM_ENDPOINT_BACKOFF_RATE`            | The exponential backoff rate for custom endpoint monitors refresh interval in the event of an AWS RDS SDK throttling exceptions. The refresh time will decrease by the same factor after successful custom endpoint information is fetched.                               | `2`           | `10`          |
| Wait for Custom Endpoint Info Timeout | `WAIT_FOR_CUSTOM_ENDPOINT_TIMEOUT_MS`     | The maximum amount of time in milliseconds that the custom endpoint plugin will wait for custom endpoint information from the monitor.                                                                                                                                                         | `5000`        | `1000`        |

### Use IAM authentication with the Custom Endpoint Plugin

When using IAM authentication make sure that IAM user has `rds:DescribeDBClusterEndpoints` permission granted.
