## IAM Authentication Plugin for the AWS Advanced ODBC Wrapper

> [!WARNING]\
> This plugin does not support MySQL Connector/ODBC due to a limitation in the connector. MySQL Connector/ODBC has a restricted connection string length, and the IAM authentication token causes the connection string to exceed this limit.
> Using this plugin with MySQL Connector/ODBC will cause a segmentation fault.
> A bug fix has been requested upstream: [mysql-connector-odbc#14](https://github.com/mysql/mysql-connector-odbc/pull/14).
>
> **Alternatives for Aurora MySQL IAM authentication:**
> - Use **MariaDB Connector/ODBC** as the underlying driver. It is wire-compatible with MySQL/Aurora MySQL and carries the IAM token without hitting the connection string length limit. This combination is validated in our integration tests. Because RDS IAM authentication requires the token to be sent over TLS, point `SSLCA` at the [RDS CA bundle](https://truststore.pki.rds.amazonaws.com/global/global-bundle.pem) (e.g. `SSLCA=/path/to/global-bundle.pem`) — this enables a verified TLS connection so the token is transmitted securely. (Note: setting `FORCETLS=1` without `SSLCA` fails with a certificate-verification error, and is unnecessary when `SSLCA` is set.)
> - Or use the [AWS Secrets Manager plugin](./secrets-manager-plugin.md) instead of IAM if you prefer to keep MySQL Connector/ODBC.
>
> See [Underlying Driver Compatibility](../using-the-aws-odbc-wrapper.md#underlying-driver-compatibility) for the full tested-driver matrix.

### What is IAM?

AWS Identity and Access Management (IAM) grants users access control across all Amazon Web Services. IAM supports granular permissions, giving you the ability to grant different permissions to different users. For more information on IAM and its use cases, please refer to the [IAM documentation](https://docs.aws.amazon.com/IAM/latest/UserGuide/introduction.html).

### Enable AWS IAM Database Authentication

To enable AWS IAM authentication, the following steps should be completed first for a PostgreSQL instance on AWS.

1. Enable AWS IAM database authentication on an existing database or create a new database with AWS IAM database authentication on the AWS RDS Console:
    1. If needed, review the documentation about [creating a new database](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_CreateDBInstance.html).
    2. If needed, review the documentation about [modifying an existing database](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/Overview.DBInstance.Modifying.html).
2. Set up an [AWS IAM policy](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.IAMPolicy.html) for AWS IAM database authentication.
3. [Create a database account](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.DBAccounts.html) using AWS IAM database authentication. This will be the user specified in the connection string or DSN window.

### Connection String / DSN Configuration for IAM Authentication Plugin Support

| Field               | Connection Option Key | Value                                                                                                                                                                         | Default Value | Sample Value                        |
|---------------------|-----------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|-------------------------------------|
| Authentication Type | `RDS_AUTH_TYPE`       | Must be `IAM`.                                                                                                                                                                | `database`    | `IAM`                               |
| Server              | `SERVER`              | Database instance server host.                                                                                                                                                | nil           | `database.us-east-1-rds.amazon.com` |
| Port                | `PORT`                | Port that the database is listening on.                                                                                                                                       | nil           | 5432                                |
| User Name           | `UID`                 | Database user name for IAM authentication.                                                                                                                                    | nil           | `iam_user`                          |
| IAM Host            | `IAM_HOST`            | The endpoint used to generate the authentication token. This is only required if you are connecting using custom endpoints such as an IP address.                             | nil           | `database.us-east-1-rds.amazon.com` |
| Region              | `REGION`              | The region of the database for IAM authentication.                                                                                                                            | `us-east-1`   | `us-east-1`                         |
| AWS Profile         | `AWS_PROFILE`         | Name of an AWS profile in `~/.aws/credentials` or `~/.aws/config` to source AWS credentials from. Supports any profile type resolvable by the AWS SDK default provider chain (static access keys, AWS IAM Identity Center (SSO), assume-role via `source_profile`/`role_arn`, and `credential_process`). When unset, the default AWS credential provider chain (including the `AWS_PROFILE` environment variable) is used. If `REGION` is not set, the profile's configured region is used when available. | nil           | `dev`                               |
| Database            | `DATABASE`            | Default database that a user will work on.                                                                                                                                    | nil           | `my_database`                       |
| Token Expiration    | `TOKEN_EXPIRATION`    | Token expiration in seconds, supported max value is 900.                                                                                                                      | 900           | 900                                 |
| Extra URL Encode    | `EXTRA_URL_ENCODE`    | Some ODBC drivers (e.g., psqlODBC) automatically URL-decode the password before sending it to the database. Enable this option to double-encode the IAM token so it arrives correctly after the driver decodes it. | `0`           | `1`                                 |

> [!WARNING]\
> When using IAM Authentication, connections to the database must have SSL enabled. Please refer to the underlying driver's specifications to enable this.

> [!WARNING]\
> When using psqlODBC as the underlying driver, `EXTRA_URL_ENCODE=1` is required. psqlODBC URL-decodes the password before sending it to the database, which corrupts the `%` escapes in the IAM authentication token and results in a `PAM authentication failed` error.

> [!NOTE]\
> `AWS_PROFILE` resolves credentials through the AWS SDK's default provider chain for the named profile, supporting profiles backed by static access keys (`~/.aws/credentials`), AWS IAM Identity Center (SSO), assume-role (`source_profile`/`role_arn`), and `credential_process` (`~/.aws/config`). When `AWS_PROFILE` is not set, the default AWS credential provider chain is used, which still honors the `AWS_PROFILE` environment variable.

### Sample Code

[IAM Authentication Example](../../../examples/iam_authentication_sample.cpp)
