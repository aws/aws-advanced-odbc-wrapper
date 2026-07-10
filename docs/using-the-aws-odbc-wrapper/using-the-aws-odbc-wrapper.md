# Using the AWS Advanced ODBC Wrapper

The AWS Advanced ODBC Wrapper leverages community ODBC Drivers and enables support of AWS and Aurora functionalities. It has been tested with the [PostgreSQL ODBC Driver (psqlodbc)](https://github.com/postgresql-interfaces/psqlodbc) for Aurora PostgreSQL, and with both [MySQL Connector/ODBC](https://dev.mysql.com/downloads/connector/odbc/) and [MariaDB Connector/ODBC](https://mariadb.com/downloads/connectors/connectors-data-access/odbc-connector/) for Aurora MySQL. See [Underlying Driver Compatibility](#underlying-driver-compatibility) for details and known limitations.

## Underlying Driver Compatibility

The wrapper delegates all database I/O to an underlying ODBC driver. The following drivers have been tested:

### Aurora PostgreSQL

[psqlodbc](https://github.com/postgresql-interfaces/psqlodbc) is the tested driver for Aurora PostgreSQL. It is validated across the unit, compatibility, and integration test suites and supports all features, including IAM authentication.

### Aurora MySQL

Both [MariaDB Connector/ODBC](https://mariadb.com/downloads/connectors/connectors-data-access/odbc-connector/) and [MySQL Connector/ODBC](https://dev.mysql.com/downloads/connector/odbc/) have been tested against Aurora MySQL. The table below summarizes feature support for each:

| Feature | MariaDB Connector/ODBC | MySQL Connector/ODBC |
|------------------------------------|:----------------------:|:--------------------:|
| Failover | ✅ | ✅ |
| IAM Authentication | ✅ | ❌ |
| Secrets Manager Authentication | ✅ | ✅ |
| Aurora Initial Connection Strategy | ✅ | ✅ |
| Blue/Green Deployments | ✅ | ✅ |
| Custom Endpoint | ✅ | ✅ |
| Read/Write Splitting | ✅ | ✅ |

MariaDB Connector/ODBC is recommended when IAM authentication is required for Aurora MySQL. MySQL Connector/ODBC works for all other scenarios but **does not support IAM authentication** - see the limitation below.

> [!IMPORTANT]\
> **IAM authentication with Aurora MySQL:** MySQL Connector/ODBC has a restricted connection string length. The IAM authentication token causes the connection string to exceed this limit, resulting in a segmentation fault. A fix has been requested upstream ([mysql-connector-odbc#14](https://github.com/mysql/mysql-connector-odbc/pull/14)).
>
> If you need IAM authentication with Aurora MySQL, use one of these alternatives:
> - **MariaDB Connector/ODBC** (recommended) - it is wire-compatible with MySQL/Aurora MySQL, carries the IAM token without hitting the length limit, and is validated for IAM in our integration tests. Because IAM authentication requires the token to be sent over TLS, point `SSLCA` at the [RDS CA bundle](https://truststore.pki.rds.amazonaws.com/global/global-bundle.pem) (e.g. `SSLCA=/path/to/global-bundle.pem`); this enables a verified TLS connection so the token is transmitted securely. Setting `FORCETLS=1` without `SSLCA` fails with a certificate-verification error and is unnecessary when `SSLCA` is set.
> - **AWS Secrets Manager authentication** - use the [Secrets Manager plugin](./plugins/secrets-manager-plugin.md) instead of IAM if you prefer to keep MySQL Connector/ODBC.

When choosing a driver, also confirm your `SSLMode`/TLS settings: IAM authentication requires SSL to be enabled regardless of the underlying driver.

## Using the AWS Advanced ODBC Wrapper with plain RDS databases

It is possible to use the AWS Advanced ODBC Wrapper with plain RDS databases, but individual features may or may not be compatible. For example, failover handling is not compatible with plain RDS databases and the relevant plugins must be disabled. Plugins can be enabled or disabled when specifying the DSN or connection string. Plugin compatibility can be verified in the [plugins table](#list-of-available-plugins).

## ODBC Compliance

The AWS Advanced ODBC Wrapper is compliant with the Microsoft ODBC 3.8 specification, ensuring compatibility with existing applications. Internally, the ODBC Wrapper will compute any plugin functionality before loading and passing the actual calls to an underlying driver.

## Logging

Logs are generated for the AWS Advanced ODBC Wrapper as well as the underlying driver. Logs for the AWS Advanced ODBC Wrapper are saved in the user's `temp` directory under the folder `aws-odbc-wrapper`. For configuring the underlying ODBC driver, please refer to the individual driver's documentation.

## AWS Advanced ODBC Wrapper Parameters

These parameters are applicable to any instance of the AWS Advanced ODBC Wrapper.

| Parameter           | Type     | Required                              | Description                                                                                                                                                                                                                                                                             | Default Value |
|---------------------|----------|---------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|
| `SERVER`            | `String` | No                                    | Server/Host of the database. If using a Base DSN, and the DSN has a field covering `SERVER` then this value is not required for the ODBC Wrapper.                                                                                                                                       | `nil`         |
| `PORT`              | `Number` | No                                    | Port of the database. If using a Base DSN, and the DSN has a field covering `PORT` then this value is not required for the ODBC Wrapper.                                                                                                                                                | `nil`         |
| `DATABASE`          | `String` | No                                    | Name of database to connect to. If using a Base DSN, and the DSN has a field covering `DATABASE` then this value is not required for the ODBC Wrapper.                                                                                                                                  | `nil`         |
| `UID`               | `String` | No                                    | Username to authenticate to database to. This value is not required to be set if using an alternative authentication plugin such as Secrets Manager Authentication or if using a Base DSN, and the DSN has a field covering `UID` then this value is not required for the ODBC Wrapper. | `nil`         |
| `PWD`               | `String` | No                                    | Password to authenticate to database to. This value is not required to be set if using an alternative authentication plugin such as Secrets Manager Authentication or if using a Base DSN, and the DSN has a field covering `PWD` then this value is not required for the ODBC Wrapper. | `nil`         |
| `BASE_DSN`          | `String` | Yes (unless BASE_DRIVER is specified) | Name of an existing DSN to use for additional value pairs. Usually a DSN pointing to the underlying driver. See the Sample DSN Configuration section below.                                                                                                                             | `nil`         |
| `BASE_DRIVER`       | `String` | Yes (unless BASE_DSN is specified)    | Path to an underlying driver to make ODBC calls to. This is not required if the Base DSN is configured and points to an underlying ODBC driver. See the Sample DSN Configuration section below.                                                                                         | `nil`         |
| `BASE_CONN` / `nil` | `String` | No                                    | A connection string to specify underlying driver specific options and additional settings.                                                                                                                                                                                              | `nil`         |
| `DSN_ONLY_OUTPUT`   | `Boolean`| No                                    | When enabled (`1`), `SQLDriverConnect` returns only the DSN name (`DSN=<name>`) in the output connection string instead of the expanded connection attributes. The connection is still authenticated and validated internally; this prevents credentials from being exposed in the returned connection string. Requires a `DSN` to be specified. | `0`           |

## Sample DSN Configuration

There are several ways to configure the DSN when connecting to a PostgreSQL database using the AWS Advanced ODBC Wrapper and the [psqlodbc PostgreSQL ODBC Driver](https://github.com/postgresql-interfaces/psqlodbc), you can use either the BASE_DSN or the BASE_DRIVER

### Using the BASE_DSN

This is useful if you want to add [psqlodbc-specific configuration](https://odbc.postgresql.org/docs/config.html).

Example 1 below configures the database credentials in the BASE_DSN.

```bash
[ODBC Data Sources]
aws-odbc-wrapper-a = AWS Advanced ODBC Wrapper Ansi
psqlodbc = PostgreSQL ODBC Driver

[aws-odbc-wrapper-a]
Driver          = AWS Advanced ODBC Wrapper Ansi
SERVER          = database-cluster-name.cluster-XYZ.us-west-1.rds.amazonaws.com
DATABASE        = my-database-name
BASE_DSN        = psqlodbc

[psqlodbc]
Driver          = path-to\psqlodbca.dylib
UID             = username
PWD             = password
SSLMODE         = require
```

Example 2 below configures the database credentials directly in the ODBC wrapper DSN.

```bash
[ODBC Data Sources]
aws-odbc-wrapper-a = AWS Advanced ODBC Wrapper Ansi
psqlodbc = PostgreSQL ODBC Driver

[aws-odbc-wrapper-a]
Driver          = AWS Advanced ODBC Wrapper Ansi
SERVER          = database-cluster-name.cluster-XYZ.us-west-1.rds.amazonaws.com
DATABASE        = my-database-name
UID             = username
PWD             = password
BASE_DSN        = psqlodbc

[psqlodbc]
Driver          = path-to\psqlodbca.dylib
SSLMODE         = require
```

### Using the BASE_DRIVER

This is useful if you don't have any [psqlodbc-specific configuration](https://odbc.postgresql.org/docs/config.html).

```bash
[ODBC Data Sources]
aws-odbc-wrapper-a = AWS Advanced ODBC Wrapper Ansi

[aws-odbc-wrapper-a]
Driver          = AWS Advanced ODBC Wrapper Ansi
UID             = username
PWD             = password
SERVER          = database-cluster-name.cluster-XYZ.us-west-1.rds.amazonaws.com
DATABASE        = my-database-name
BASE_DRIVER     = path-to\psqlodbca.dylib
```

## Plugins

The AWS Advanced ODBC Wrapper uses plugins to execute ODBC methods. You can think of a plugin as an extensible code module that adds extra logic around any ODBC method calls. The AWS Advanced ODBC Wrapper has a number of [built-in plugins](#list-of-available-plugins) available for use.

### List of Available Plugins

The AWS Advanced ODBC Wrapper has several built-in plugins that are available to use. Please visit the individual plugin page for more details.

| Plugin Name                                                                                         | Database Compatibility           | Driver Compatibility           | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
|-----------------------------------------------------------------------------------------------------|----------------------------------|--------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [ADFS Authentication](./plugins/adfs-authentication-plugin.md)                                      | Aurora                           | psqlodbc                       | Enables users to authenticate using Federated Identity and then connect to their Amazon Aurora Cluster using AWS Identity and Access Management (IAM).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| [AWS IAM Identity Center (SSO) Authentication](./plugins/aws-sso-authentication-plugin.md)          | Aurora, RDS Multi-AZ DB Cluster  | psqlodbc                       | Enables users to authenticate through AWS IAM Identity Center (SSO) via an in-driver browser login (no AWS CLI required) and then connect to their database using AWS Identity and Access Management (IAM).                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [Aurora Initial Connection Strategy Plugin](./plugins/aurora-initial-connection-strategy-plugin.md) | Aurora                           | psqlodbc, MySQL Connector/ODBC | Allows users to configure their initial connection strategy to reader cluster endpoints. Prevents incorrectly opening a new connection to an old writer node when DNS records have not yet updated after a recent failover event. <br><br> This plugin is **strongly** suggested when using cluster writer endpoint, cluster reader endpoint or global database endpoint in the connection string. <br><br> :warning:**Note:** Contrary to the failover plugin, the initial connection strategy plugin doesn't implement failover support itself. It helps to eliminate opening wrong connections to an old writer instance after cluster failover is completed. |
| [Blue Green Plugin](./plugins/blue-green-plugin.md)                                                 | Aurora                           | psqlodbc, MySQL Connector/ODBC | Enables blue green support for reduced downtime during switchovers.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| [Custom Endpoint Plugin](./plugins/custom-endpoint-plugin.md)                                       | Aurora                           | psqlodbc, MySQL Connector/ODBC | Enables custom endpoint support.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| [Failover Plugin](./plugins/failover-plugin.md)                                                     | Aurora, RDS Multi-AZ DB Cluster  | psqlodbc, MySQL Connector/ODBC | Enables the failover functionality supported by Amazon Aurora and RDS Multi-AZ clusters. Prevents opening wrong connections to the old writer node due to stale DNS after a failover event.                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [IAM Authentication](./plugins/iam-authentication-plugin.md)                                        | Aurora, RDS Multi-AZ DB Cluster  | psqlodbc                       | Enables users to connect to their Amazon Aurora clusters using AWS Identity and Access Management (IAM).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| [Limitless Plugin](./plugins/limitless-plugin.md)                                                   | Aurora                           | psqlodbc, MySQL Connector/ODBC | Enables client-side load-balancing of Transaction Routers on Amazon Aurora Limitless Databases.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| [Okta Authentication](./plugins/okta-authentication-plugin.md)                                      | Aurora, RDS Multi-AZ DB Cluster  | psqlodbc                       | Enables users to authenticate using Okta's Federated Identity and then connect to their Amazon Aurora Cluster using AWS Identity and Access Management (IAM).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |
| [Read/Write Splitting Plugin](./plugins/read-write-splitting-plugin.md)                             | Aurora                           | psqlodbc, MySQL Connector/ODBC | Enables switching between writer and reader instances based on the connection's read-only attribute, using the cluster topology to select reader hosts.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| [Secrets Manager Authentication](./plugins/secrets-manager-plugin.md)                               | Aurora, RDS Multi-AZ DB Cluster  | psqlodbc, MySQL Connector/ODBC | Enables fetching database credentials from the AWS Secrets Manager service.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
| [Simple Read/Write Splitting Plugin](./plugins/simple-read-write-splitting-plugin.md)               | Aurora, RDS  Multi-AZ DB Cluster | psqlodbc, MySQL Connector/ODBC | Enables switching between user-specified writer and reader endpoints based on the connection's read-only attribute, without relying on cluster topology.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
