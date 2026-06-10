# Support for Amazon RDS Multi-AZ DB Cluster

The AWS Advanced ODBC Wrapper supports Amazon RDS Multi-AZ DB Cluster Deployments. By leveraging the topology information exposed by an RDS Multi-AZ DB Cluster, the wrapper can detect a Multi-AZ cluster at connect time and switch a connection over to a new writer node quickly after a failover, minor version upgrade, or OS maintenance upgrade without waiting for DNS records to propagate.

This is distinct from Amazon Aurora support. An RDS Multi-AZ DB Cluster is a separate deployment type (one writer and two readable standby instances) that exposes its topology through different catalog functions and tables than Aurora. The wrapper detects which deployment it is connected to and selects the correct topology queries automatically.

## General Usage

Using the AWS Advanced ODBC Wrapper with an RDS Multi-AZ DB Cluster is largely the same as using it with an Aurora cluster. All parameters, DSN configuration, and plugins remain consistent. Instead of pointing `SERVER` at a generic database endpoint, use the **Cluster Writer Endpoint** provided by the RDS Multi-AZ DB Cluster.

The wrapper determines the database dialect automatically during connection establishment:

1. On connect, the wrapper connects through the underlying driver and runs a lightweight detection probe on known base driver names (psqlodbc, mysql-odbc-connector).
2. If the server reports the RDS Multi-AZ cluster markers (the `rds_tools` topology function on PostgreSQL, or the `report_host` variable on MySQL), the wrapper upgrades the active dialect to `MULTI_AZ_POSTGRESQL` or `MULTI_AZ_MYSQL`.

If you prefer not to rely on auto-detection, you can set the dialect explicitly with the `DATABASE_DIALECT` parameter (see [Specifying the Dialect Explicitly](#specifying-the-dialect-explicitly) below).

> [!NOTE]
> Failover behavior for RDS Multi-AZ DB Clusters is provided by the [Failover Plugin](./plugins/failover-plugin.md). Enable it with `ENABLE_CLUSTER_FAILOVER=1`. Without the Failover Plugin enabled, the wrapper still detects the Multi-AZ dialect but will not switch connections after a failover event.

### PostgreSQL

The topology information for RDS for PostgreSQL Multi-AZ DB Clusters is exposed through the `rds_tools` extension. Per AWS documentation, the topology information is populated in Amazon RDS for PostgreSQL versions 13.12, 14.9, 15.4, or higher (starting from revision R3). Ensure you have a supported PostgreSQL version deployed.

The `rds_tools` extension must be installed on the target cluster before the topology information becomes available. Run the following DDL once as an administrative user:

```sql
CREATE EXTENSION rds_tools;
```

The extension must also be accessible to all non-administrative users who need database access. Without access to `rds_tools`, non-admin users cannot use the wrapper's advanced features, including failover support. Grant the necessary permissions with:

```sql
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA rds_tools TO non-admin-username;
```

Sample connection string (using a base driver):

```
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=cluster-writer-endpoint;PORT=5432;DATABASE=my-database;UID=username;PWD=password;BASE_DRIVER=/path/to/psqlodbca.so;ENABLE_CLUSTER_FAILOVER=1
```

### MySQL

For RDS for MySQL Multi-AZ DB Clusters, topology information is exposed through the `mysql.rds_topology` table. Extra permissions must be granted to all non-administrative users who need database access. Without proper access, these users cannot use many of the wrapper's advanced features, including failover support.

Grant the necessary permissions with:

```sql
GRANT SELECT ON mysql.rds_topology TO 'non-admin-username'@'%';
```

Sample connection string (using a base driver):

```
DRIVER={AWS Advanced ODBC Wrapper Ansi};SERVER=cluster-writer-endpoint;PORT=3306;DATABASE=my-database;UID=username;PWD=password;BASE_DRIVER=/path/to/myodbc.so;DATABASE_DIALECT=MULTI_AZ_MYSQL;ENABLE_CLUSTER_FAILOVER=1
```

## Specifying the Dialect Explicitly

The wrapper accepts a `DATABASE_DIALECT` parameter in the DSN or connection string to override auto-detection. Supported values:

| `DATABASE_DIALECT` Value      | Deployment                              |
|-------------------------------|-----------------------------------------|
| `AURORA_POSTGRESQL`           | Aurora PostgreSQL                       |
| `AURORA_POSTGRESQL_LIMITLESS` | Aurora PostgreSQL Limitless             |
| `AURORA_MYSQL`                | Aurora MySQL                            |
| `MULTI_AZ_POSTGRESQL`         | RDS Multi-AZ DB Cluster (PostgreSQL)    |
| `MULTI_AZ_MYSQL`              | RDS Multi-AZ DB Cluster (MySQL)         |

When `DATABASE_DIALECT` is omitted, the wrapper attempts to detect the dialect automatically as described in [General Usage](#general-usage).

## How Detection and Switchover Work

| Step | PostgreSQL | MySQL |
|------|-----------|-------|
| Multi-AZ detection probe | `SELECT multi_az_db_cluster_source_dbi_resource_id FROM rds_tools.multi_az_db_cluster_source_dbi_resource_id()` | `SHOW VARIABLES LIKE 'report_host'` |
| Topology query | `SELECT id, endpoint, port FROM rds_tools.show_topology()` | `SELECT id, endpoint, port FROM mysql.rds_topology` |
| Writer identification | Compares the connected instance's `dbi_resource_id` against the replica source id | `SHOW REPLICA STATUS` (`Source_Server_Id`), falling back to `SELECT @@server_id` on the writer |

When the Failover Plugin is enabled, the wrapper maintains a cached view of the cluster topology and uses it to reconnect to the promoted writer after a failover, bypassing DNS propagation delays.

## Supported Plugins

The following plugins are supported with Amazon RDS Multi-AZ DB Clusters in the AWS Advanced ODBC Wrapper:

- [Failover Plugin](./plugins/failover-plugin.md)
- [IAM Authentication Plugin](./plugins/iam-authentication-plugin.md)
- [Okta Authentication Plugin](./plugins/okta-authentication-plugin.md)
- [Secrets Manager Authentication](./plugins/secrets-manager-plugin.md) 
- [Simple Read/Write Splitting Plugin](./plugins/simple-read-write-splitting-plugin.md)

Other plugins have not been validated against RDS Multi-AZ DB Clusters at this time. They may function as expected or may result in unhandled behavior — use at your own discretion. See the [plugins compatibility table](./using-the-aws-odbc-wrapper.md#list-of-available-plugins) for the authoritative per-plugin support matrix.

## Additional Reading

- [Using the AWS Advanced ODBC Wrapper](./using-the-aws-odbc-wrapper.md)
- [AWS blog: Achieve one second or less downtime when upgrading Amazon RDS Multi-AZ DB clusters](https://aws.amazon.com/blogs/database/achieve-one-second-or-less-downtime-with-the-advanced-jdbc-wrapper-driver-when-upgrading-amazon-rds-multi-az-db-clusters/)
- [Amazon RDS Multi-AZ DB Cluster deployments (AWS documentation)](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/multi-az-db-clusters-concepts.html)
