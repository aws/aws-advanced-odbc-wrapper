## Okta Authentication Plugin for the AWS Advanced ODBC Wrapper
The driver supports authentication via an [Okta](https://www.okta.com/) federated identity and then database access via IAM.

However, the driver does not support 2FA. To disable 2FA for specific users or a groups, please read [Okta's article](https://support.okta.com/help/s/article/Exclude-from-OKta-Verify-MFA-user-doesn-t-have-a-phone) on how to do so.

### What is Federated Identity
Federated Identity allows users to use the same set of credentials to access multiple services or resources across different organizations. This works by having Identity Providers (IdP) that manage and authenticate user credentials, and Service Providers (SP) that are services or resources that can be internal, external, and/or belonging to various organizations. Multiple SPs can establish trust relationships with a single IdP.

When a user wants access to a resource, it authenticates with the IdP. From this, a security token generated and passed to the Service Providers (SP), which grants access to the specific resource. In the case of Okta, the user signs into their Okta application sign in page. This generates a SAML Assertion which acts as a security token. The user then passes the SAML Assertion to the SP when requesting access to resources. The SP verifies the SAML Assertion and grants access to the user.

### Enable Okta Authentication
1. Follow steps in `Enable AWS IAM Database Authentication` to setup IAM authentication.
1. Configure Okta as the AWS identity provider following [Okta's official documentation](https://help.okta.com/en-us/content/topics/deploymentguides/aws/aws-deployment.htm)

### Connection String / DSN Configuration for Okta Authentication Plugin Support

| Field | Connection Option Key | Value | Default Value | Sample Value |
|-------|-----------------------|-------|---------------|--------------|
| Authentication Type | `RDS_AUTH_TYPE` | Should be `OKTA` | `database` | `OKTA` |
| Server | `SERVER` | Database instance server host | nil | `database.us-east-1-rds.amazon.com` |
| Port | `PORT` | Port that the database is listening on | nil | 5432 |
| User Name | `UID` | Database user name for IAM authentication | nil | `iam_user` |
| IAM Host | `IAM_HOST` | The endpoint used to generate the authentication token. This is only required if you are connecting using custom endpoints such as an IP address. | nil | `database.us-east-1-rds.amazon.com` |
| Region | `REGION` | The region of the database for IAM authentication | `us-east-1` | `us-east-1` |
| Database | `DATABASE` | Default database that a user will work on | nil | `my_database` |
| Token Expiration | `TOKEN_EXPIRATION` | Token expiration in seconds, supported max value is 900 | 900 | 900 |
| IdP Endpoint | `IDP_ENDPOINT` | The ADFS host that is used to authenticate with | nil | `my-adfs-host.com` |
| IdP Port | `IDP_PORT` | The ADFS host port | 443 | 443 |
| IdP User Name | `IDP_USERNAME` | The user name for the IdP Endpoint server | nil | `user@email.com` |
| IdP Password | `IDP_PASSWORD` | The IdP user's password | nil | `my_password_123` |
| Role ARN | `IDP_ROLE_ARN` | The ARN of the IAM Role that is to be assumed for database access | nil | `arn:aws:iam::123412341234:role/ADFS-SAML-Assume` |
| IdP SAML Provider ARN | `IDP_SAML_ARN` | The ARN of the Identity Provider | nil | `arn:aws:iam::123412341234:saml-provider/ADFS-AWS-IAM` |
| HTTP Socket Timeout | `HTTP_SOCKET_TIMEOUT` | The socket timeout value in milliseconds for the HttpClient reading | 3000 | 3000 |
| HTTP Connect Timeout | `HTTP_CONNECT_TIMEOUT` | The connect timeout value in milliseconds for the HttpClient | 5000 | 5000 |
| Relaying Party ID | `RELAY_PARTY_ID` | The relaying party identifier | `urn:amazon:webservices` | `urn:amazon:webservices` |
| App ID | `APP_ID` | The application ID for AWS configured on | nil | `my-app-id` |

> [!WARNING]\
> Using IAM Authentication, connections to the database must have SSL enabled. Please refer to the underlying driver's specifications to enable this.
