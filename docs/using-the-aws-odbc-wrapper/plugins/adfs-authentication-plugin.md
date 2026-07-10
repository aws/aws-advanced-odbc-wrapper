## ADFS Authentication Plugin for the AWS Advanced ODBC Wrapper

> [!WARNING]\
> This plugin does not support MySQL Connector/ODBC due to a limitation in the connector. ADFS authentication relies on IAM, and MySQL Connector/ODBC has a restricted connection string length that is exceeded once the IAM authentication token is appended.
> Using this plugin with MySQL Connector/ODBC will cause a segmentation fault.
> A bug fix has been requested upstream: [mysql-connector-odbc#14](https://github.com/mysql/mysql-connector-odbc/pull/14).
>
> **Alternatives for Aurora MySQL IAM authentication:**
> - Use **MariaDB Connector/ODBC** as the underlying driver. It is wire-compatible with MySQL/Aurora MySQL and carries the IAM token without hitting the connection string length limit. This combination is validated in our integration tests. Because RDS IAM authentication requires the token to be sent over TLS, point `SSLCA` at the [RDS CA bundle](https://truststore.pki.rds.amazonaws.com/global/global-bundle.pem) (e.g. `SSLCA=/path/to/global-bundle.pem`) — this enables a verified TLS connection so the token is transmitted securely. (Note: setting `FORCETLS=1` without `SSLCA` fails with a certificate-verification error, and is unnecessary when `SSLCA` is set.)
> - Or use the [AWS Secrets Manager plugin](./secrets-manager-plugin.md) instead of IAM if you prefer to keep MySQL Connector/ODBC.
>
> See [Underlying Driver Compatibility](../using-the-aws-odbc-wrapper.md#underlying-driver-compatibility) for the full tested-driver matrix.

The driver supports authentication via [Microsoft's Active Directory Federation Services (ADFS)](https://learn.microsoft.com/en-us/windows-server/identity/ad-fs/ad-fs-overview) federated identity and then database access via IAM.

### What is Federated Identity

Federated Identity allows users to use the same set of credentials to access multiple services or resources across different organizations. This works by having Identity Providers (IdP) that manage and authenticate user credentials, and Service Providers (SP) that are services or resources that can be internal, external, and/or belonging to various organizations. Multiple SPs can establish trust relationships with a single IdP.

When a user wants access to a resource, it authenticates with the IdP. From this, a security token generated and passed to the Service Providers (SP), which grants access to the specific resource. In the case of ADFS, the user signs into the ADFS sign in page. This generates a SAML Assertion which acts as a security token. The user then passes the SAML Assertion to the SP when requesting access to resources. The SP verifies the SAML Assertion and grants access to the user.

### Enable ADFS Authentication

1. Follow steps in [Enable AWS IAM Database Authentication](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.Enabling.html) to set up IAM authentication.
2. Set up an IAM Identity Provider and IAM role. The IAM role should be using the IAM policy set up in step 1.
    1. If needed, review the documentation about [creating IAM identity providers](https://docs.aws.amazon.com/IAM/latest/UserGuide/id_roles_providers_create.html). For ADFS, see the documentation about [creating IAM SAML identity providers](https://docs.aws.amazon.com/IAM/latest/UserGuide/id_roles_providers_create_saml.html).

### Connection String / DSN Configuration for ADFS Authentication Plugin Support

| Field                 | Connection Option Key  | Value                                                                                                                                                                         | Default Value            | Sample Value                                           |
|-----------------------|------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------|--------------------------------------------------------|
| Authentication Type   | `RDS_AUTH_TYPE`        | Must be `ADFS`.                                                                                                                                                               | `database`               | `ADFS`                                                 |
| Server                | `SERVER`               | Database instance server host.                                                                                                                                                | nil                      | `database.us-east-1-rds.amazon.com`                    |
| Port                  | `PORT`                 | Port that the database is listening on.                                                                                                                                       | nil                      | 5432                                                   |
| User Name             | `UID`                  | Database user name for IAM authentication.                                                                                                                                    | nil                      | `iam_user`                                             |
| IAM Host              | `IAM_HOST`             | The endpoint used to generate the authentication token. This is only required if you are connecting using custom endpoints such as an IP address.                             | nil                      | `database.us-east-1-rds.amazon.com`                    |
| Region                | `REGION`               | The region of the database for IAM authentication.                                                                                                                            | `us-east-1`              | `us-east-1`                                            |
| Database              | `DATABASE`             | Default database that a user will work on.                                                                                                                                    | nil                      | `my_database`                                          |
| Token Expiration      | `TOKEN_EXPIRATION`     | Token expiration in seconds, supported max value is 900.                                                                                                                      | 900                      | 900                                                    |
| IdP Endpoint          | `IDP_ENDPOINT`         | The ADFS host that is used to authenticate with.                                                                                                                              | nil                      | `my-adfs-host.com`                                     |
| IdP Port              | `IDP_PORT`             | The ADFS host port.                                                                                                                                                           | 443                      | 443                                                    |
| IdP User Name         | `IDP_USERNAME`         | The user name for the IdP Endpoint server.                                                                                                                                    | nil                      | `user@email.com`                                       |
| IdP Password          | `IDP_PASSWORD`         | The IdP user's password.                                                                                                                                                      | nil                      | `my_password_123`                                      |
| Role ARN              | `IDP_ROLE_ARN`         | The ARN of the IAM Role that is to be assumed for database access.                                                                                                            | nil                      | `arn:aws:iam::123412341234:role/ADFS-SAML-Assume`      |
| IdP SAML Provider ARN | `IDP_SAML_ARN`         | The ARN of the Identity Provider.                                                                                                                                             | nil                      | `arn:aws:iam::123412341234:saml-provider/ADFS-AWS-IAM` |
| HTTP Socket Timeout   | `HTTP_SOCKET_TIMEOUT`  | The socket timeout value in milliseconds for the HttpClient reading.                                                                                                          | 3000                     | 3000                                                   |
| HTTP Connect Timeout  | `HTTP_CONNECT_TIMEOUT` | The connect timeout value in milliseconds for the HttpClient.                                                                                                                 | 5000                     | 5000                                                   |
| Relaying Party ID     | `RELAY_PARTY_ID`       | The relaying party identifier.                                                                                                                                                | `urn:amazon:webservices` | `urn:amazon:webservices`                               |
| Extra URL Encode      | `EXTRA_URL_ENCODE`     | Some ODBC drivers (e.g., psqlODBC) automatically URL-decode the password before sending it to the database. Enable this option to double-encode the IAM token so it arrives correctly after the driver decodes it. | `0`                      | `1`                                                    |

> [!WARNING]\
> When using ADFS Authentication, connections to the database must have SSL enabled. Please refer to the underlying driver's specifications to enable this.

> [!WARNING]\
> When using psqlODBC as the underlying driver, `EXTRA_URL_ENCODE=1` is required. psqlODBC URL-decodes the password before sending it to the database, which corrupts the `%` escapes in the IAM authentication token and results in a `PAM authentication failed` error.

### Sample Code

[ADFS Authentication Example](../../../examples/adfs_authentication_sample.cpp)
