# Getting Started

## Minimum Requirements

Before using the AWS Advanced ODBC Wrapper, you must install:

- The AWS Advanced ODBC Wrapper
- UnixODBC and ICU4C if using the pre-compiled artifacts on Mac (via Homebrew) or Linux (via APT)
- and an underlying ODBC Driver of your choice
    - To use the wrapper with Aurora with PostgreSQL compatibility, install the [psqlodbc PostgreSQL ODBC Driver](https://github.com/postgresql-interfaces/psqlodbc).

## Obtaining the AWS Advanced ODBC Wrapper

You can use pre-compiled packages that can be downloaded directly from [GitHub Releases](https://github.com/aws/aws-advanced-odbc-wrapper/releases).

## Installation

### Windows

Download the `.msi` Windows installer; execute the installer and follow the onscreen instructions. The default target installation location for the wrapper files is `C:\Program Files\AWS Advanced ODBC Wrapper`.
Two wrappers will be installed:

- AWS Advanced ODBC Wrapper Ansi
- AWS Advanced ODBC Wrapper Unicode

To uninstall the AWS Advanced ODBC Wrapper, open the same installer file, select the option to uninstall the wrapper and follow the onscreen instructions to successfully uninstall the wrapper.

### MacOS

> [!WARNING]\
> This wrapper currently only supports MacOS with Silicon chips.

Download the releases `aws-advanced-odbc-wrapper-<Version>-macos-arm64.zip` and extract the artifacts to a location of your choice.

For pre-built binaries, users must allow both the Wrapper and the dependencies to be used through [Gatekeeper](https://support.apple.com/en-ca/guide/security/sec5599b66df/web).
The following command can be used to bypass this for the downloaded files:
```
xattr -dr com.apple.quarantine /path/to/the/wrapper/
```

Once extracted, you can optionally perform a checksum to ensure match from GitHub build by running

```
shasum -a 256 aws-advanced-odbc-wrapper-a.dylib
shasum -a 256 aws-advanced-odbc-wrapper-w.dylib
```

### Linux

Download the releases `aws-advanced-odbc-wrapper-<Version>-linux-x64.tar.gz` and extract the artifacts to a location of your choice.

Once extracted, you can optionally perform a checksum to ensure match from GitHub build by running

```
sha256sum aws-advanced-odbc-wrapper-a.so
sha256sum aws-advanced-odbc-wrapper-w.so
```

### Configuring the Driver and DSN Entries

To configure the driver on Windows, use the `ODBC Data Source Administrator` tool to add or configure a DSN for either the `AWS Advanced ODBC Wrapper Ansi` or `AWS Advanced ODBC Wrapper Unicode`.
With this DSN you can specify the options for the desired connection.

To use the driver on Linux and MacOS systems, you need to create or modify two files: `odbc.ini` and `odbcinst.ini`, that will contain the configuration for the driver and the Data Source Name (DSN), see the following examples:

#### odbc.ini

```bash
[ODBC Data Sources]
aws-odbc-wrapper-a = AWS Advanced ODBC Wrapper Ansi
aws-odbc-wrapper-w = AWS Advanced ODBC Wrapper Unicode

[aws-odbc-wrapper-a]
Driver          = AWS Advanced ODBC Wrapper Ansi
SERVER          = database-cluster-name.cluster-XYZ.us-west-1.rds.amazonaws.com
DATABASE        = my-database-name
BASE_DRIVER     = path-to\psqlodbca.dylib

[aws-odbc-wrapper-w]
Driver          = AWS Advanced ODBC Wrapper Unicode
SERVER          = database-cluster-name.cluster-XYZ.us-west-1.rds.amazonaws.com
DATABASE        = my-database-name
BASE_DRIVER     = path-to\psqlodbcw.dylib
```

#### odbcinst.ini

```bash
[ODBC Drivers]
AWS Advanced ODBC Wrapper Ansi      = Installed
AWS Advanced ODBC Wrapper Unicode   = Installed

[AWS Advanced ODBC Wrapper Ansi]
# For MacOS
Driver = aws-advanced-odbc-wrapper-a.dylib # or `aws-advanced-odbc-wrapper-a.so` for Linux

[AWS Advanced ODBC Wrapper Unicode]
# For MacOS
Driver = aws-advanced-odbc-wrapper-w.dylib # or `aws-advanced-odbc-wrapper-w.so` for Linux
```

### Using the Wrapper

For more information on how to use and configure, visit [Using the AWS Advanced ODBC Wrapper page](./using-the-aws-odbc-wrapper/using-the-aws-odbc-wrapper.md)
