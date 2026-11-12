# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - Unreleased

### Deprecated

- `EXTRA_URL_ENCODE` is deprecated and ignored. The wrapper now encloses the credentials it supplies to the underlying driver (authentication tokens, Secrets Manager passwords and the `SQLConnect` password) in curly braces, so psqlODBC no longer URL-decodes them and a token no longer needs to be pre-encoded. A connection with the attribute enabled (`EXTRA_URL_ENCODE=1`) still succeeds, but returns `SQL_SUCCESS_WITH_INFO` with SQLSTATE `01S00` naming the attribute. The checkbox has been removed from the Windows DSN setup dialog, and saving a DSN in the dialog removes the attribute

### Changed

- **Breaking:** Connection strings are now parsed according to the ODBC connection string grammar: a value enclosed in curly braces may contain `;` and `=`, and a literal `}` inside it is written `}}`. Each value is passed to the underlying driver as the application wrote it, braces included, so the driver applies its own rules, for example psqlODBC URL-decodes a password that is not enclosed in braces. A connection string with an unclosed brace, or with text between a closing brace and the next `;`, is now rejected with SQLSTATE `08001`. See [Connection String Values Containing Special Characters](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/using-the-aws-odbc-wrapper.md#connection-string-values-containing-special-characters)
- **Breaking:** A password passed to `SQLConnect` is used as is. A password URL-encoded to work around psqlODBC's URL-decoding must now be passed unencoded
- **Breaking:** Curly braces stored in a DSN value are part of the value: the value is passed to the underlying driver as stored, enclosed in braces only when it contains `;` or starts with `{`. Previously the braces reached psqlODBC, which removed them
- The output connection string of `SQLDriverConnect` keeps the curly braces the application wrote, and encloses other values in braces when they contain `;` or start with `{`

### Fixed

- Passwords containing `%` or `+` are no longer corrupted by psqlODBC's URL-decoding when they are retrieved from Secrets Manager or passed to `SQLConnect`. Authentication tokens no longer need `EXTRA_URL_ENCODE`
- Connection string values containing `;`, such as passwords enclosed in curly braces, are no longer truncated
- Underlying driver names enclosed in curly braces in a connection string (`BASE_DRIVER={PostgreSQL Unicode}`) now resolve instead of failing to load
- Sensitive values are now fully redacted in logs when the value is enclosed in curly braces
- Debug builds no longer log the generated IAM authentication token [(PR #167)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/167)
- `SQLGetInfo(SQL_DRIVER_VER)` now reports the real wrapper version instead of a hardcoded `1.0.0`
- Discovery of the `odbcinst`/`iodbcinst` library on macOS and Linux, which ignored the configured `ODBC_ROOT`
- The ANSI static library is now compiled with `SQL_NOUNICODEMAP`, matching the ANSI shared library

## [1.3.1] - 2026-09-08

### Fixed

- Resolution of logical driver names in `odbcinst.ini`, preventing failures to load the underlying driver [(PR #160)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/160)
- Validation of numeric connection attribute parsing [(PR #162)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/162)
- IAM token generation now signs with the database region instead of the SSO region for cross-region AWS IAM Identity Center (SSO) authentication [(PR #168)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/168)

## [1.3.0] - 2026-07-30

### Added

- [AWS IAM Identity Center (SSO) Authentication Plugin](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/aws-sso-authentication-plugin.md) [(PR #151)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/151)
- [Browser-Based SAML (Passwordless) Mode for the Okta Authentication Plugin](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/okta-authentication-plugin.md#additional-okta-setup-for-browser-mode-passwordless) [(PR #154)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/154)
- Support for selecting a named AWS profile with the [AWS_PROFILE connection option](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/iam-authentication-plugin.md) [(PR #150)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/150)

### Fixed

- Segmentation faults from invalid ODBC handles and monitor thread races [(PR #157)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/157)
- Schema-qualified internal SQL queries for Blue/Green detection [(PR #158)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/158)

## [1.2.0] - 2026-06-16

### Added

- Support for [Aurora MySQL](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/using-the-aws-odbc-wrapper.md#aurora-mysql)
- Support for [Amazon RDS Multi-AZ DB Clusters](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/SupportForRDSMultiAzDBCluster.md) [(PR #141)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/141)
- [Read/Write Splitting Plugin](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/read-write-splitting-plugin.md) and [Simple Read/Write Splitting Plugin](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/simple-read-write-splitting-plugin.md) [(PR #107)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/107)
- [Blue/Green Deployment Support](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/blue-green-plugin.md) [(PR #101)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/101)
- [Aurora Initial Connection Strategy Plugin](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/using-the-aws-odbc-wrapper/plugins/aurora-initial-connection-strategy-plugin.md) [(PR #100)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/100)
- MacOS Releases Signed and Notarized [(PR #96)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/96)
- [Flag to Return DSN-Name Only](https://github.com/aws/aws-advanced-odbc-wrapper/pull/142) in SQLDriverConnect [(PR #142)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/142)

### Fixed

- Proper lifetime of the monitor DBC handle [(PR #133)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/133)
- Cluster topology monitor incorrectly running plugins [(PR #130)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/130)
- Improved SQLWCHAR encoding to support iODBC and unixODBC  [(PR #94)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/94)

## [1.1.0] - 2026-03-09

### Added

- 2FA for Okta Authentication Plugin [(PR #72)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/72)
- Ability to specify the username and password keys for Secret Manager Plugin [(PR #84)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/84)
- AWS Custom Endpoint Plugin [(PR #82)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/82)

### Fixed

- Aliased keywords for Server, Username, Password from base DSNs preventing authentication plugins from functioning properly [(PR #76)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/76)
- ODBC Data Source Administrator GUI Deadlocks when testing connections [(PR #83)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/83)
- Incorrect deletions of Limitless and cluster topology monitors [(PR #81)](https://github.com/aws/aws-advanced-odbc-wrapper/pull/81)

## [1.0.0] - 2025-11-25

The Amazon Web Services (AWS) Advanced ODBC Wrapper allows an application to take advantage of AWS authentication, failover feature of the clustered Aurora databases, and provides support for Aurora Limitless databases.

### Added

- Support for PostgreSQL
- AWS IAM Authentication Connection Plugin
- AWS Secrets Manager Authentication Connection Plugin
- AWS Federated Authentication with OKTA and ADFS Connection Plugins
- Limitless Connection Plugin
- Failover Connection Plugin

[1.3.1]: https://github.com/aws/aws-advanced-odbc-wrapper/compare/1.3.0...1.3.1
[1.3.0]: https://github.com/aws/aws-advanced-odbc-wrapper/compare/1.2.0...1.3.0
[1.2.0]: https://github.com/aws/aws-advanced-odbc-wrapper/compare/1.1.0...1.2.0
[1.1.0]: https://github.com/aws/aws-advanced-odbc-wrapper/compare/1.0.0...1.1.0
[1.0.0]: https://github.com/aws/aws-advanced-odbc-wrapper/releases/tag/v1.0.0
