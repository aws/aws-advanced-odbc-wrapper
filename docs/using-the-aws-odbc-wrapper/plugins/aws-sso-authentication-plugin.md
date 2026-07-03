## AWS IAM Identity Center (SSO) Authentication Plugin for the AWS Advanced ODBC Wrapper

The driver supports authenticating to RDS/Aurora through [AWS IAM Identity Center](https://aws.amazon.com/iam/identity-center/) (formerly AWS SSO) entirely from the ODBC DSN / connection string, with **no dependency on the AWS CLI** (`aws configure sso` / `aws sso login`).

> [!NOTE]\
> If you only want the driver to *reuse* an SSO token that the AWS CLI already produced, see the [`AWS_PROFILE` option on the IAM Authentication Plugin](./iam-authentication-plugin.md) instead. This plugin performs the SSO login itself.

### How it works

This plugin runs the OAuth 2.0 **authorization code grant with PKCE** against your IAM Identity Center instance, the same grant used by browser-based SSO sign-in:

1. The driver registers a public OIDC client and opens your IAM Identity Center sign-in page in the system browser.
2. After you approve, IAM Identity Center redirects to a short-lived `http://127.0.0.1:<port>` listener inside the driver, which captures the authorization code.
3. The driver exchanges the code (plus the PKCE verifier) for an SSO access token, then calls `sso:GetRoleCredentials` to obtain temporary AWS credentials for the configured account and role.
4. Those credentials are used to generate a standard RDS IAM authentication token, which is sent to the database as the password — identical to the IAM, ADFS, and Okta plugins.

The access token, refresh token, and client registration are cached in a driver-owned cache directory (`~/.aws/sso/cache`). A later connection refreshes the token silently (`refresh_token` grant) without opening the browser, which is what enables non-interactive reconnection (for example, from a connection pool) after the first interactive login.

### Prerequisites

1. Follow the steps in [Enable AWS IAM Database Authentication](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.Enabling.html) to set up IAM database authentication on your cluster.
2. [Enable AWS IAM Identity Center](https://docs.aws.amazon.com/singlesignon/latest/userguide/get-set-up-for-idc.html) and assign the user access to an account and a permission set (role) that is permitted to connect to the database via IAM.
3. Note the IAM Identity Center **start URL**, the **account ID**, and the **role (permission set) name**.

### Connection String / DSN Configuration for AWS SSO Authentication Plugin Support

| Field                     | Connection Option Key       | Value                                                                                                                                                                                                               | Default Value | Sample Value                                  |
|---------------------------|-----------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|-----------------------------------------------|
| Authentication Type       | `RDS_AUTH_TYPE`             | Must be `AWS_SSO`.                                                                                                                                                                                                   | `database`    | `AWS_SSO`                                      |
| Server                    | `SERVER`                    | Database instance server host.                                                                                                                                                                                      | nil           | `database.us-east-1.rds.amazonaws.com`        |
| Port                      | `PORT`                      | Port that the database is listening on.                                                                                                                                                                             | nil           | `5432`                                         |
| User Name                 | `UID`                       | Database user name for IAM authentication.                                                                                                                                                                          | nil           | `iam_user`                                     |
| SSO Start URL             | `SSO_START_URL`             | The AWS IAM Identity Center start (portal) URL used to authenticate.                                                                                                                                                 | nil           | `https://my-sso.awsapps.com/start`            |
| SSO Account ID            | `SSO_ACCOUNT_ID`            | The AWS account ID that the role to assume belongs to.                                                                                                                                                              | nil           | `123456789012`                                 |
| SSO Role Name             | `SSO_ROLE_NAME`             | The IAM Identity Center permission set (role) name to assume for database access.                                                                                                                                   | nil           | `DatabaseAccess`                               |
| SSO Region                | `SSO_REGION`                | The region of the IAM Identity Center instance. Falls back to `REGION`, then the region derived from `SERVER`, then `us-east-1`.                                                                                     | derived       | `us-east-1`                                    |
| Region                    | `REGION`                    | The region of the database for IAM authentication.                                                                                                                                                                  | `us-east-1`   | `us-east-1`                                    |
| IAM Host                  | `IAM_HOST`                  | The endpoint used to generate the authentication token. Only required when connecting using custom endpoints such as an IP address.                                                                                 | nil           | `database.us-east-1.rds.amazonaws.com`        |
| Token Expiration          | `TOKEN_EXPIRATION`          | RDS IAM token expiration in seconds, supported max value is 900.                                                                                                                                                    | `900`         | `900`                                          |
| SSO Session Name          | `SSO_SESSION_NAME`          | Optional name used as the SSO token cache key. When omitted, the start URL is used.                                                                                                                                 | nil           | `my-company`                                   |
| SSO Listen Port           | `SSO_LISTEN_PORT`           | The local `127.0.0.1` port the browser is redirected to in order to capture the authorization code.                                                                                                                  | `8080`        | `8000`                                         |
| SSO IdP Response Timeout  | `SSO_IDP_RESPONSE_TIMEOUT`  | The time in seconds to complete the browser login before the connection fails. Minimum 10.                                                                                                                          | `120`         | `60`                                           |
| SSO Allow Interactive     | `SSO_ALLOW_INTERACTIVE`     | Allow the browser login to open even on connections that would otherwise be non-interactive, such as `SQLConnect` (DSN only) or `SQLDriverConnect` called with `SQL_DRIVER_NOPROMPT`. Enable this for DSN-only callers and BI tools that cannot request an interactive `SQLDriverConnect`. Cache/refresh is still tried first, so the browser only opens when no valid token is available. | `0`           | `1`                                            |
| Extra URL Encode          | `EXTRA_URL_ENCODE`          | Some ODBC drivers (e.g., psqlODBC) automatically URL-decode the password before sending it to the database. Enable this option to double-encode the IAM token so it arrives correctly after the driver decodes it.   | `0`           | `1`                                            |

> [!WARNING]\
> When using AWS SSO authentication, connections to the database must have SSL enabled. Please refer to the underlying driver's specifications to enable this.

> [!IMPORTANT]\
> By default the browser login only opens on an interactive connection — a `SQLDriverConnect` call with a `DriverCompletion` other than `SQL_DRIVER_NOPROMPT`. Under `SQL_DRIVER_NOPROMPT` (the mode used by connection pools and many headless callers) or `SQLConnect` (DSN only, which has no `DriverCompletion`), the driver does **not** open a browser: it uses a cached or refreshed token if one is available, and otherwise fails with a clear message asking you to reconnect interactively.
>
> To open the browser login on these connections anyway, set `SSO_ALLOW_INTERACTIVE=1`. This lets DSN-only callers (`SQLConnect`) and BI tools that only ever pass `SQL_DRIVER_NOPROMPT` complete the first browser login. A cached or refreshable token is still preferred, so the browser only opens when no valid token is available. Alternatively, establish one interactive connection first (on a host with a browser) so a refreshable token is cached.

> [!NOTE]\
> Even with `SSO_ALLOW_INTERACTIVE=1`, the first connection on a headless/SSH host with no display and no cached token cannot complete, because the browser cannot be launched. The connection-string path still works once a token has been cached on a machine with a browser, since the cache (`~/.aws/sso/cache`) is shared with the AWS CLI/SDK.
