## Limitless Plugin for the AWS Advanced ODBC Wrapper

## What is Amazon Aurora Limitless Database?

Amazon Aurora Limitless Database is a new type of database that can horizontally scale to handle millions of write transactions per second and manage petabytes of data. Users will be able to use the AWS Advanced ODBC Wrapper with Aurora Limitless Databases and optimize their experience using the Limitless Plugin. To learn more about Aurora Limitless Database, see the [Amazon Aurora Limitless documentation](https://aws.amazon.com/about-aws/whats-new/2023/11/amazon-aurora-limitless-database/).

## Why use the Limitless Plugin?

Aurora Limitless Database introduces a new endpoint for the databases - the DB shard group (limitless) endpoint that's managed by Route 53. When connecting to Aurora Limitless Database, clients will connect using this endpoint, and be routed to a transaction router via Route 53. Unfortunately, Route 53 is limited in its ability to load balance, and can allow uneven work loads on transaction routers. The Limitless Plugin addresses this by performing client-side load balancing with load awareness.

The Limitless Plugin achieves this by periodically polling for available transaction routers and their load metrics, and then caching them. When a new connection is made, the feature directs the connection to a transaction router selected from the cache using a weighted round-robin strategy. Routers with a higher load are assigned a lower weight, and routers with a lower load are assigned a higher weight.

## How to use the Limitless Plugin with the AWS Advanced ODBC Wrapper

The following DSN fields on a DSN window should be filled when using the Limitless Plugin.

### Connection String / DSN Configuration for Limitless Plugin Support

| Field                         | Connection Option Key           | Value                                                                                                                                                                                                                                                                                                                                                                      | Default Value | Sample Value |
|-------------------------------|---------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|--------------|
| Enable Limitless              | `ENABLE_LIMITLESS`              | Set to `1` to enable the Limitless Plugin.                                                                                                                                                                                                                                                                                                                                 | `0`           | `1`          |
| Limitless Mode                | `LIMITLESS_MODE`                | The limitless mode specifies whether a router monitor should attempt to query for limitless routers immediately after creation. When set to `IMMEDIATE`, the router monitor will query for limitless routers immediately after creation. When set to `LAZY`, the router monitor will wait for limitless monitoring interval to pass before querying for limitless routers. | `IMMEDIATE`   | `LAZY`       |
| Limitless Monitoring Interval | `LIMITLESS_MONITOR_INTERVAL_MS` | The limitless router monitor polling interval in milliseconds. The limitless router monitor will query for limitless routers after waiting the specified time to pass.                                                                                                                                                                                                     | `0`           | `1`          |
| Limitless Router Max Retries  | `LIMITLESS_ROUTER_MAX_RETRIES`  | If there are no limitless routers detected by the limitless router monitor, a direct query will be made to determine the routers. This value determines how many times the query will be retried before the connection fails.                                                                                                                                              | `5`           | `3`          |
| Limitless Max Retries         | `LIMITLESS_MAX_RETRIES`         | This value determines the maximum number of retries the Limitless Plugin will attempt to connect to the database.                                                                                                                                                                                                                                                          | `5`           | `3`          |

### Use with Other Features

> [!WARNING]\
> We don't recommend enabling both the Failover and Limitless Plugins at the same time.
> While it won't result in issues, the Failover feature was not designed to be used with Aurora Limitless Databases.
> Enabling both features will introduce unnecessary computation and memory overhead with no added benefits.

### Use with Connection Pools

Connection pools keep connections open for reuse, but this can work against the client-side load-balancing of the Limitless Plugin and cause an imbalanced load on transaction routers. To mitigate this, consider setting connection properties that can reduce the number of idle connections or increase the lifetime of connections.
