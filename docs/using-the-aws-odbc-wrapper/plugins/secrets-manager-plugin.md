## Secrets Manager Authentication Plugin for the AWS Advanced ODBC Wrapper

### What is Secrets Manager?

AWS Secrets Manager helps manage, retrieve, and rotate database credentials. For more information on Secrets Manager and its use cases, please refer to the [Secrets Manager documentation](https://docs.aws.amazon.com/secretsmanager/latest/userguide/intro.html).

### Enable Secrets Manager

To enable AWS Secrets Manager authentication, follow the AWS RDS documents on [Enabling master user password for DB instance](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/rds-secrets-manager.html#rds-secrets-manager-db-instance)

### Connection String / DSN Configuration for Secrets Manager Plugin Support

| Field                                    | Connection Option Key      | Value                                                                                               | Default Value | Sample Value                                                            |
|------------------------------------------|----------------------------|-----------------------------------------------------------------------------------------------------|---------------|-------------------------------------------------------------------------|
| Authentication Type                      | `RDS_AUTH_TYPE`            | Must be `SECRETS_MANAGER`.                                                                          | `database`    | `SECRETS_MANAGER`                                                       |
| Server                                   | `SERVER`                   | Database instance server host.                                                                      | nil           | `database.us-east-1-rds.amazon.com`                                     |
| Port                                     | `PORT`                     | Port that the database is listening on.                                                             | nil           | 5432                                                                    |
| User Name                                | `UID`                      | Database user name for IAM authentication.                                                          | nil           | `iam_user`                                                              |
| Region                                   | `SECRET_REGION`            | The Secrets Managers' Secret region.                                                                | `us-east-1`   | `us-east-1`                                                             |
| Database                                 | `DATABASE`                 | Default database that a user will work on.                                                          | nil           | `my_database`                                                           |
| Token Expiration                         | `TOKEN_EXPIRATION`         | Token expiration in seconds, supported max value is 900.                                            | 900           | 900                                                                     |
| Secret Id                                | `SECRET_ID`                | Secret ID which holds the database credentials.                                                     | nil           | `arn:aws:secretsmanager:us-west-2:123412341234:secret:rds!cluster-UUID` |
| Secrets Manager Endpoint                 | `SECRET_ENDPOINT`          | Endpoint to call to retrieve Secrets from.                                                          | nil           | `https://localhost:1234`                                                |
| Secrets Manager Secret Username Property | `SECRET_USERNAME_PROPERTY` | Set this value to be the key in the JSON secret that contains the username for database connection. | `username`    | `db_user`                                                               |
| Secrets Manager Secret Password Property | `SECRET_PASSWORD_PROPERTY` | Set this value to be the key in the JSON secret that contains the password for database connection. | `password`    | `db_pass`                                                               |

### Secret Value
The secret stored in the AWS Secrets Manager should be a JSON object containing the properties username and password. If the secret contains different key names, you can specify them with the `SECRET_USERNAME_PROPERTY` and `SECRET_PASSWORD_PROPERTY` parameters.

> [!NOTE]
> Only un-nested JSON format is supported at the moment.

### Sample Code

[Secrets Manager Authentication Example](../../../examples/secrets_manager_authentication_sample.cpp)
