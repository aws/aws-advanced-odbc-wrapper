# Using the AWS Advanced ODBC Wrapper
The AWS Advanced ODBC Wrapper leverages community ODBC Drivers and enables support of AWS and Aurora functionalities. Currently, the [PostgreSQL ODBC Wrapper, psqlodbc](https://github.com/postgresql-interfaces/psqlodbc) is supported.

## Using the AWS Advanced ODBC Wrapper with plain RDS databases
It is possible to use the AWS Advanced ODBC Wrapper with plain RDS databases, but individual features may or may not be compatible. For example, failover handling is not compatible with plain RDS databases and the relevant plugins must be disabled. Plugins can be enabled or disabled when specifying the DSN or connection string. Plugin compatibility can be verified in the [plugins table](#list-of-available-plugins).

## ODBC Compliance
The AWS Advanced ODBC Wrapper is compliant with the Microsoft ODBC 3.8 specification, ensuring compatibility with existing applications. Internally, the ODBC Wrapper will compute any plugin functionality before loading and passing the actual calls to an underlying driver.

## Logging

Logs are generated for the AWS Advanced ODBC Wrapper as well as the underlying driver. Logs for the AWS Advanced ODBC Wrapper are saved in the user's `temp` directory under the folder `aws-odbc-wrapper`. For configuring the underlying ODBC driver, please refer to the individual driver's documentation.

## AWS Advanced ODBC Wrapper Parameters
These parameters are applicable to any instance of the AWS Advanced ODBC Wrapper.

| Parameter | Type  | Required | Description | Default Value |
|-----------|-------|----------|-------------|---------------|
| `SERVER` | `String` | No | Server/Host of the database. If using a Base DSN, and the DSN has a field covering `SERVER` then this value is not required for the ODBC Wrapper. | `nil` |
| `PORT` | `Number` | No | Port of the database. If using a Base DSN, and the DSN has a field covering `PORT` then this value is not required for the ODBC Wrapper. | `nil` |
| `DATABASE` | `String` | No | Name of database to connect to. If using a Base DSN, and the DSN has a field covering `DATABASE` then this value is not required for the ODBC Wrapper. | `nil` |
| `UID` | `String` | No | Username to authenticate to database to. This value is not required to be set if using an alternative authentication plugin such as Secrets Manager Authentication or if using a Base DSN, and the DSN has a field covering `UID` then this value is not required for the ODBC Wrapper. | `nil` |
| `PWD` | `String` | No | Password to authenticate to database to. This value is not required to be set if using an alternative authentication plugin such as Secrets Manager Authentication or if using a Base DSN, and the DSN has a field covering `PWD` then this value is not required for the ODBC Wrapper. | `nil` |
| `BASE_DSN` | `String` | No | Name of an existing DSN to use for additional value pairs. | `nil` |
| `BASE_DRIVER` | `String` | No | Path to an underlying driver to make ODBC calls to. This is not required if the Base DSN is given and the underlying driver set as `DRIVER` is wanted to be used. | `nil` |
| `BASE_CONN` / `nil` | `String` | No | A connection string to specify underlying driver specific options and additional settings. | `nil` |

## Plugins
The AWS Advanced ODBC Wrapper uses plugins to execute ODBC methods. You can think of a plugin as an extensible code module that adds extra logic around any ODBC method calls. The AWS Advanced ODBC Wrapper has a number of [built-in plugins](#list-of-available-plugins) available for use.

### List of Available Plugins
The AWS Advanced ODBC Wrapper has several built-in plugins that are available to use. Please visit the individual plugin page for more details.

| Plugin Name | Database Compatibility | Description |
|-------------|----------|-------------|
| [IAM Authentication](./plugins/iam-authentication-plugin.md) | Aurora, RDS Multi-AZ DB Cluster | Enables users to connect to their Amazon Aurora clusters using AWS Identity and Access Management (IAM). |
| [Secrets Manager Authentication](./plugins/secrets-manager-plugin.md) | Aurora, RDS Multi-AZ DB Cluster | Enables fetching database credentials from the AWS Secrets Manager service. |
| [ADFS Authentication](./plugins/adfs-authentication-plugin.md) | Aurora, RDS Multi-AZ DB Cluster | Enables users to authenticate using Federated Identity and then connect to their Amazon Aurora Cluster using AWS Identity and Access Management (IAM). |
| [Okta Authentication](./plugins/okta-authentication-plugin.md) | Aurora, RDS Multi-AZ DB Cluster | Enables users to authenticate using Okta's Federated Identity and then connect to their Amazon Aurora Cluster using AWS Identity and Access Management (IAM). |
| [Failover Plugin](./plugins/failover-plugin.md) | Aurora, RDS Multi-AZ DB Cluster | Enables the failover functionality supported by Amazon Aurora and RDS Multi-AZ clusters. Prevents opening wrong connections to the old writer node due to stale DNS after a failover event. |
| [Limitless Plugin](./plugins/limitless-plugin.md) | Aurora | Enables client-side load-balancing of Transaction Routers on Amazon Aurora Limitless Databases. |
