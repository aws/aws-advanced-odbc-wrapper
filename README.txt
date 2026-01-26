## Amazon Web Services (AWS) Advanced ODBC Wrapper

The Amazon Web Services (AWS) Advanced ODBC Wrapper is complementary to existing ODBC drivers and aims to extend the functionality of these driver to enable applications to take full advantage of the features of clustered databases such as Amazon Aurora. In other words, the AWS Advanced ODBC Wrapper does not connect directly to any database, but enables support of AWS and Aurora functionalities on top of an underlying ODBC driver of the user's choice. This approach enables service-specific enhancements, without requiring users to change their workflow and existing ODBC driver tooling.

The AWS Advanced ODBC Wrapper is targeted to work with any existing ODBC driver. Currently, the AWS Advanced ODBC Wrapper has been validated to support [psqlodbc, a PostgreSQL ODBC Driver](https://github.com/postgresql-interfaces/psqlodbc).

## Benefits of the AWS Advanced ODBC Wrapper for All Aurora and RDS Database Services

### Seamless AWS Authentication Service Integration

Built-in support for AWS Identity and Access Management (IAM) authentication eliminates the need to manage database passwords, while AWS Secrets Manager integration provides secure credential management for services that require password-based authentication.

#### Preserve Existing Workflows

The wrapper design allows developers to continue using their preferred ODBC drivers and existing database code while gaining service-specific enhancements. No application rewrites are required.

#### Modular Plugin Architecture

The plugin-based design ensures applications only load the functionality they need, reducing dependencies and overhead.

### Benefits of the AWS Advanced ODBC Wrapper for Aurora PostgreSQL

#### Faster Failover and Reduced Downtime

For Aurora PostgreSQL, the driver significantly reduces connection recovery time during database failovers. By maintaining a real-time cache of cluster topology and bypassing DNS resolution delays, applications can reconnect to healthy database instances in seconds rather than minutes.

## Getting Started

For more information on how to download the AWS Advanced ODBC Wrapper, minimum requirements to use it, and how to integrate it within your project and with your ODBC driver of choice, please visit the [Getting Started page](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/docs/getting-started.md).

## Documentation

Technical documentation regarding the functionality of the AWS Advanced ODBC Wrapper will be maintained in this GitHub repository. Since the AWS Advanced ODBC Wrapper requires an underlying ODBC driver, please refer to the individual driver's documentation for driver-specific information.

### Using the AWS Advanced ODBC Wrapper

To find all the documentation and concrete examples on how to use the AWS Advanced ODBC Wrapper, please refer to the [AWS Advanced ODBC Wrapper Documentation page](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/README.md).

### Known Limitations

#### Amazon Aurora Global Databases

AWS Authentication requires the proper region to be set to generate a proper credential token. If using the global endpoint to connect, the region may change when server-sided failover occurs. Please ensure region is updated to the correct database instance region when re-establishing connections.

## Getting Help & Opening Issues

If you encounter a bug with the AWS Advanced ODBC Wrapper, we would like to hear about it. Please search the [existing issues](https://github.com/aws/aws-advanced-odbc-wrapper/issues) to see if others are also experiencing the issue before reporting the problem in a new issue. GitHub issues are intended for bug reports and feature requests.

When opening a new issue, please fill in all required fields in the issue template to help expedite the investigation process.

## Contributing and Reporting Security Issues

See [CONTRIBUTING](https://github.com/aws/aws-advanced-odbc-wrapper/blob/main/CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This project is licensed under the Apache-2.0 License.
