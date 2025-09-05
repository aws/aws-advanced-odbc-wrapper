## IAM Authentication Plugin for the AWS Advanced ODBC Wrapper

### What is IAM?
AWS Identity and Access Management (IAM) grants users access control across all Amazon Web Services. IAM supports granular permissions, giving you the ability to grant different permissions to different users. For more information on IAM and its use cases, please refer to the [IAM documentation](https://docs.aws.amazon.com/IAM/latest/UserGuide/introduction.html).

### Enable AWS IAM Database Authentication
To enable AWS IAM authentication, the following steps should be completed first for a PostgreSQL instance on AWS.

1. Enable AWS IAM database authentication on an existing database or create a new database with AWS IAM database authentication on the AWS RDS Console:
    1. If needed, review the documentation about [creating a new database](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_CreateDBInstance.html).
    1. If needed, review the documentation about [modifying an existing database](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/Overview.DBInstance.Modifying.html).
1. Set up an [AWS IAM policy](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.IAMPolicy.html) for AWS IAM database authentication.
1. [Create a database account](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.DBAccounts.html) using AWS IAM database authentication. This will be the user specified in the connection string or DSN window.

### Connection String / DSN Configuration for IAM Authentication Plugin Support

| Field | Connection Option Key | Value | Default Value | Sample Value |
|-------|-----------------------|-------|---------------|--------------|
| Authentication Type | `RDS_AUTH_TYPE` | Should be `IAM` | `database` | `IAM` |
| Server | `SERVER` | Database instance server host | nil | `database.us-east-1-rds.amazon.com` |
| Port | `PORT` | Port that the database is listening on | nil | 5432 |
| User Name | `UID` | Database user name for IAM authentication | nil | `iam_user` |
| IAM Host | `IAM_HOST` | The endpoint used to generate the authentication token. This is only required if you are connecting using custom endpoints such as an IP address. | nil | `database.us-east-1-rds.amazon.com` |
| Region | `REGION` | The region of the database for IAM authentication | `us-east-1` | `us-east-1` |
| Database | `DATABASE` | Default database that a user will work on | nil | `my_database` |
| Token Expiration | `TOKEN_EXPIRATION` | Token expiration in seconds, supported max value is 900 | 900 | 900 |

> [!WARNING]\
> Using IAM Authentication, connections to the database must have SSL enabled. Please refer to the underlying driver's specifications to enable this.

### Sample Code
[IAM Authentication Example](../../../examples/iam_authentication_sample.cpp)
