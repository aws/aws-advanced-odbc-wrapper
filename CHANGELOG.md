# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03-09

### Added

- 2FA for Okta Authentication Plugin
- Ability to specify the username and password keys for Secret Manager Plugin
- AWS Custom Endpoint Plugin

### Fixed

- Aliased keywords for Server, Username, Password from base DSNs preventing authentication plugins from functioning properly
- ODBC Data Source Administrator GUI Deadlocks when testing connections
- Incorrect deletions of limitless and cluster topology monitors

## [1.0.0] - 2025-11-25

The Amazon Web Services (AWS) Advanced ODBC Wrapper allows an application to take advantage of AWS authentication, failover feature of the clustered Aurora databases, and provides support for Aurora Limitless databases.

### Added

- Support for PostgreSQL
- AWS IAM Authentication Connection Plugin
- AWS Secrets Manager Authentication Connection Plugin
- AWS Federated Authentication with OKTA and ADFS Connection Plugins
- Limitless Connection Plugin
- Failover Connection Plugin

[1.1.0]: https://github.com/aws/aws-advanced-odbc-wrapper/compare/1.0.0...1.1.0
[1.0.0]: https://github.com/aws/aws-advanced-odbc-wrapper/releases/tag/v1.0.0
