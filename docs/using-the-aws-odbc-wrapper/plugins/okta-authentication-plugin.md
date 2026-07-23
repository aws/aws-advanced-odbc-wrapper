## Okta Authentication Plugin for the AWS Advanced ODBC Wrapper

> [!WARNING]\
> This plugin does not support MySQL Connector/ODBC due to a limitation in the connector. Okta authentication relies on IAM, and MySQL Connector/ODBC has a restricted connection string length that is exceeded once the IAM authentication token is appended.
> Using this plugin with MySQL Connector/ODBC will cause a segmentation fault.
> A bug fix has been requested upstream: [mysql-connector-odbc#14](https://github.com/mysql/mysql-connector-odbc/pull/14).
>
> **Alternatives for Aurora MySQL IAM authentication:**
> - Use **MariaDB Connector/ODBC** as the underlying driver. It is wire-compatible with MySQL/Aurora MySQL and carries the IAM token without hitting the connection string length limit. This combination is validated in our integration tests. Because RDS IAM authentication requires the token to be sent over TLS, point `SSLCA` at the [RDS CA bundle](https://truststore.pki.rds.amazonaws.com/global/global-bundle.pem) (e.g. `SSLCA=/path/to/global-bundle.pem`). This enables a verified TLS connection so the token is transmitted securely. (Note: setting `FORCETLS=1` without `SSLCA` fails with a certificate-verification error, and is unnecessary when `SSLCA` is set.)
> - Or use the [AWS Secrets Manager plugin](./secrets-manager-plugin.md) instead of IAM if you prefer to keep MySQL Connector/ODBC.
>
> See [Underlying Driver Compatibility](../using-the-aws-odbc-wrapper.md#underlying-driver-compatibility) for the full tested-driver matrix.

The driver supports authentication via an [Okta](https://www.okta.com/) federated identity and then database access via IAM.

### What is Federated Identity

Federated Identity allows users to use the same set of credentials to access multiple services or resources across different organizations. This works by having Identity Providers (IdP) that manage and authenticate user credentials, and Service Providers (SP) that are services or resources that can be internal, external, and/or belonging to various organizations. Multiple SPs can establish trust relationships with a single IdP.

When a user wants access to a resource, it authenticates with the IdP. From this, a security token generated and passed to the Service Providers (SP), which grants access to the specific resource. In the case of Okta, the user signs into their Okta application sign in page. This generates a SAML Assertion which acts as a security token. The user then passes the SAML Assertion to the SP when requesting access to resources. The SP verifies the SAML Assertion and grants access to the user.

### Choosing an Authentication Mode

The `OKTA` auth type supports two ways to authenticate with Okta. The mode is selected by
a single key: if `LOGIN_URL` is present, the plugin uses **browser mode**; otherwise it
uses **headless mode**.

| | Headless (username/password) | Browser (passwordless SAML) |
|---|---|---|
| **How the user signs in** | The plugin sends `IDP_USERNAME`/`IDP_PASSWORD` directly to Okta's `/api/v1/authn` API. No user interaction unless MFA is configured. | The plugin opens the system browser at `LOGIN_URL`; the user completes Okta's own sign-in page (FastPass, FIDO2/WebAuthn, push, password, or whatever else the Okta policy allows). |
| **Credentials in the DSN** | Okta username and password are stored in the DSN / connection string. | None. |
| **Supports passwordless (FastPass/FIDO2)** | No, the `/api/v1/authn` API requires a password. | Yes. |
| **MFA** | Configure with `MFA_TYPE`/`MFA_PORT`/`MFA_TIMEOUT` (TOTP or push). | Governed entirely by the Okta sign-in policy in the browser; the `MFA_*` keys are ignored (with a warning). |
| **Works unattended (services, jobs)** | Yes. | No, a user must be present to complete the browser login. |
| **Key that selects it** | `LOGIN_URL` absent | `LOGIN_URL` present |

**Use headless mode** for unattended or scripted connections where storing an Okta
service-account password in the DSN is acceptable. **Use browser mode** for interactive
users, or when your Okta org is configured for passwordless authentication (e.g. Okta
Verify for everything). In that case the `/api/v1/authn` password API cannot be used and
browser mode is the only path.

Both modes end the same way: the SAML assertion is exchanged for temporary AWS credentials
via `AssumeRoleWithSAML`, an RDS IAM authentication token is generated for `UID`, and the
wrapper connects to the database.

### Enable Okta Authentication

1. Follow steps in [Enable AWS IAM Database Authentication](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.Enabling.html) to setup IAM authentication.
2. Configure Okta as the AWS identity provider following [Okta's official documentation](https://help.okta.com/en-us/content/topics/deploymentguides/aws/aws-deployment.htm). In summary:
   1. **IAM SAML provider** created from the Okta app's federation metadata
      (`aws iam create-saml-provider --name Okta --saml-metadata-document file://metadata.xml`).
      The name must match the provider portion of `IDP_SAML_ARN`.
   2. **IAM role** with a trust policy allowing `sts:AssumeRoleWithSAML` from that provider,
      and an `rds-db:connect` policy for the database user.
   3. **Database user**: e.g. for PostgreSQL, `CREATE USER <uid>; GRANT rds_iam TO <uid>;`
   4. **Okta app** (`amazon_aws`) with the role mapped via group membership, and the user
      assigned to the group.
   5. (Optional, headless mode) Enable MFA. MFA through Okta Verify is supported for the Push and OTP methods. Please ensure there is a global session policy configured to require MFA.
3. When using Browser Mode, create a custom SAML 2.0 App, See [Additional Okta Setup for Browser Mode](#additional-okta-setup-for-browser-mode-passwordless).

### Additional Okta Setup for Browser Mode (Passwordless)

Browser mode captures the SAML assertion by listening on `http://localhost:<LISTEN_PORT>`.
The **AWS Account Federation catalog app (`amazon_aws`) cannot be used for this**: its
Assertion Consumer Service (ACS) URL is locked to `https://signin.aws.amazon.com/saml`
(the AWS console sign-in page) and is not editable, so the browser login completes but the
SAML response never reaches the driver. The connection then fails with
`No SAMLResponse received from browser flow` after the `IDP_RESPONSE_TIMEOUT` expires.

Instead, create a second, custom app for database access (keep the catalog app for
console access if you have one):

1. In the Okta Admin console, go to **Applications > Create App Integration > SAML 2.0**
   (the template, not the catalog app) and give it a name, e.g. `AWS RDS Database Access`.
2. On the **Configure SAML** step:
   - **Single sign on URL**: `http://localhost:8080` (must match the DSN's `LISTEN_PORT`;
     keep *Use this for Recipient URL and Destination URL* checked).
   - **Audience URI (SP Entity ID)**: `urn:amazon:webservices`
   - Under **Attribute Statements**, add:

     | Name | Name format | Value |
     |------|-------------|-------|
     | `https://aws.amazon.com/SAML/Attributes/Role` | URI Reference | `<IDP_SAML_ARN>,<IDP_ROLE_ARN>` (comma-separated, SAML provider ARN first followed by Role ARN, no spaces) |
     | `https://aws.amazon.com/SAML/Attributes/RoleSessionName` | Basic | `user.email` |
     | `https://aws.amazon.com/SAML/Attributes/SessionDuration` (optional) | Basic | `3600` |

3. Assign the user (or their group) to the new app.
4. **Update the IAM SAML provider metadata.** The new app signs with its own certificate,
   so an IAM SAML provider created from the catalog app's metadata will reject its
   assertions. Download the new app's federation metadata (Sign On tab) and update the
   existing provider (`aws iam update-saml-provider`) or create a new one, and point
   `IDP_SAML_ARN` at whichever provider holds this app's metadata.
5. **Add the ACS URL to the role trust policy.** The `SAML:aud` condition key matches the
   assertion's `SubjectConfirmationData` `Recipient` (the Single sign on URL from step 2),
   not the `<Audience>` element. Add the exact value (e.g. `http://localhost:8080`) to the
   trust policy's `SAML:aud` list, or `AssumeRoleWithSAML` fails with `Not authorized`:

   ```json
   {
     "Effect": "Allow",
     "Principal": { "Federated": "arn:aws:iam::<account-id>:saml-provider/Okta" },
     "Action": "sts:AssumeRoleWithSAML",
     "Condition": { "StringEquals": { "SAML:aud": "http://localhost:8080" } }
   }
   ```

6. Set the DSN's `LOGIN_URL` to the new app's **embed link**
   (Application > General > App Embed Link, e.g.
   `https://<org>.okta.com/home/<app-name>/<app-id>/<link-id>`).

### Connection String / DSN Configuration for Okta Authentication Plugin Support

#### Keys Used by Both Modes

| Field                 | Connection Option Key  | Value                                                                                                                                                                                                               | Default Value | Sample Value                                           |
|-----------------------|------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|--------------------------------------------------------|
| Authentication Type   | `RDS_AUTH_TYPE`        | Must be `OKTA`.                                                                                                                                                                                                     | `database`    | `OKTA`                                                 |
| Server                | `SERVER`               | Database instance server host.                                                                                                                                                                                      | nil           | `database.us-east-1-rds.amazon.com`                    |
| Port                  | `PORT`                 | Port that the database is listening on.                                                                                                                                                                             | nil           | `5432`                                                 |
| User Name             | `UID`                  | Database user name for IAM authentication.                                                                                                                                                                          | nil           | `iam_user`                                             |
| IAM Host              | `IAM_HOST`             | The endpoint used to generate the authentication token. This is only required if you are connecting using custom endpoints such as an IP address.                                                                   | nil           | `database.us-east-1-rds.amazon.com`                    |
| Region                | `REGION`               | The region of the database for IAM authentication.                                                                                                                                                                  | `us-east-1`   | `us-east-1`                                            |
| Database              | `DATABASE`             | Default database that a user will work on.                                                                                                                                                                          | nil           | `my_database`                                          |
| Token Expiration      | `TOKEN_EXPIRATION`     | Token expiration in seconds, supported max value is 900.                                                                                                                                                            | `900`         | `900`                                                  |
| IdP Endpoint          | `IDP_ENDPOINT`         | The Okta host that is used to authenticate with.                                                                                                                                                                    | nil           | `my-okta-instance.com`                                 |
| Role ARN              | `IDP_ROLE_ARN`         | The ARN of the IAM Role that is to be assumed for database access.                                                                                                                                                  | nil           | `arn:aws:iam::123412341234:role/OKTA-SAML-Assume`      |
| IdP SAML Provider ARN | `IDP_SAML_ARN`         | The ARN of the Identity Provider.                                                                                                                                                                                   | nil           | `arn:aws:iam::123412341234:saml-provider/OKTA-AWS-IAM` |
| STS Endpoint          | `STS_ENDPOINT`         | STS endpoint override for `AssumeRoleWithSAML`. Required for non-commercial partitions (e.g. GovCloud), where the SDK does not reliably resolve the regional STS endpoint from the region alone.                    | nil           | `https://sts.us-gov-west-1.amazonaws.com`              |
| Extra URL Encode      | `EXTRA_URL_ENCODE`     | Some ODBC drivers (e.g., psqlODBC) automatically URL-decode the password before sending it to the database. Enable this option to double-encode the IAM token so it arrives correctly after the driver decodes it. | `0`           | `1`                                                    |

#### Headless-Mode Keys (`LOGIN_URL` not set)

The `IDP_USERNAME`/`IDP_PASSWORD` pair is what authenticates the user in headless mode; these keys are ignored when browser mode is active.

| Field                 | Connection Option Key  | Value                                                                                                                                                                                                               | Default Value | Sample Value                                           |
|-----------------------|------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|--------------------------------------------------------|
| IdP User Name         | `IDP_USERNAME`         | The user name for the IdP Endpoint server.                                                                                                                                                                          | nil           | `user@email.com`                                       |
| IdP Password          | `IDP_PASSWORD`         | The IdP user's password.                                                                                                                                                                                            | nil           | `my_password_123`                                      |
| App ID                | `APP_ID`               | The application ID for AWS configured on.                                                                                                                                                                           | nil           | `my-app-id`                                            |
| IdP Port              | `IDP_PORT`             | The Okta host port.                                                                                                                                                                                                 | `443`         | `443`                                                  |
| HTTP Socket Timeout   | `HTTP_SOCKET_TIMEOUT`  | The socket timeout value in milliseconds for the HttpClient reading.                                                                                                                                                | `3000`        | `3000`                                                 |
| HTTP Connect Timeout  | `HTTP_CONNECT_TIMEOUT` | The connect timeout value in milliseconds for the HttpClient.                                                                                                                                                       | `5000`        | `5000`                                                 |
| MFA Type              | `MFA_TYPE`             | The MFA type the user specifies. The available options are: `TOTP`, `PUSH`. **Note**: the `TOTP` type requires a web browser to be used.                                                                            | nil           | `TOTP`                                                 |
| MFA Port              | `MFA_PORT`             | The port used to connect to `127.0.0.1` to provide the one time code when using TOTP as the MFA Type.                                                                                                               | `8080`        | `8000`                                                 |
| MFA Timeout           | `MFA_TIMEOUT`          | The time in seconds to complete the MFA challenge before the connection fails.                                                                                                                                      | `60`          | `30`                                                   |

#### Browser-Mode Keys (`LOGIN_URL` set)

Setting `LOGIN_URL` is what activates browser mode; the other keys tune the local redirect listener.

| Field                 | Connection Option Key  | Value                                                                                                                                                                                                               | Default Value | Sample Value                                           |
|-----------------------|------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|--------------------------------------------------------|
| Login URL             | `LOGIN_URL`            | Okta SSO/embed URL to open in the system browser. Its presence selects browser-based SAML mode. Use the embed link of a custom SAML 2.0 app whose ACS URL targets the local listener; see [Additional Okta Setup for Browser Mode](#additional-okta-setup-for-browser-mode-passwordless). | nil           | `https://<org>.okta.com/home/<app-name>/<app-id>/<link-id>` |
| Listen Port           | `LISTEN_PORT`          | Local port for the SAML redirect listener in browser mode.                                                                                                                                                          | `8080`        | `7890`                                                 |
| IdP Response Timeout  | `IDP_RESPONSE_TIMEOUT` | Seconds to wait for the browser login to complete in browser mode.                                                                                                                                                  | `60`          | `120`                                                  |

> [!WARNING]\
> When using Okta authentication, connections to the database must have SSL enabled. Please refer to the underlying driver's specifications to enable this.

> [!WARNING]\
> When using psqlODBC as the underlying driver, `EXTRA_URL_ENCODE=1` is required. psqlODBC URL-decodes the password before sending it to the database, which corrupts the `%` escapes in the IAM authentication token and results in a `PAM authentication failed` error.

### Sample Code

[Okta Authentication Example](../../../examples/okta_authentication_sample.cpp)
