// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CONNECTION_STRING_KEYS_H
#define CONNECTION_STRING_KEYS_H

#include "rds_strings.h"

/* Generic */
#define VALUE_BOOL_FALSE ("0")
#define VALUE_BOOL_TRUE ("1")

#define KEY_DATABASE ("DATABASE")
#define KEY_SERVER ("SERVER")
#define KEY_PORT ("PORT")

#define KEY_BASE_DRIVER ("BASE_DRIVER")
#define KEY_DRIVER ("DRIVER")
#define KEY_BASE_DSN ("BASE_DSN")
#define KEY_DSN ("DSN")

#define KEY_SAVEFILE ("SAVEFILE")
#define KEY_FILEDSN ("FILEDSN")
#define KEY_DESC ("DESCRIPTION")
#define KEY_BASE_CONN ("BASE_CONN")

#define ODBC ("ODBC")
#define ODBC_DATA_SOURCES ("ODBC Data Sources")

/* Auth */
#define KEY_AUTH_TYPE ("RDS_AUTH_TYPE")
#define KEY_DB_USERNAME ("UID")
#define KEY_DB_PASSWORD ("PWD")

/* Auth Types */
#define VALUE_AUTH_DATABASE ("DATABASE")
#define VALUE_AUTH_IAM ("IAM")
#define VALUE_AUTH_SECRETS ("SECRETS_MANAGER")
#define VALUE_AUTH_ADFS ("ADFS")
#define VALUE_AUTH_OKTA ("OKTA")

/* Common Auth Settings */
#define KEY_REGION ("REGION")
#define KEY_TOKEN_EXPIRATION ("TOKEN_EXPIRATION")
#define KEY_EXTRA_URL_ENCODE ("EXTRA_URL_ENCODE")

/* IAM */
#define KEY_IAM_HOST ("IAM_HOST")
#define KEY_IAM_PORT ("IAM_PORT")
#define KEY_IAM_IDP_ARN ("IAM_IDP_ARN")
#define KEY_IDP_PORT ("IDP_PORT")
/* Secrets Manager */
#define KEY_SECRET_ID ("SECRET_ID")
#define KEY_SECRET_REGION ("SECRET_REGION")
#define KEY_SECRET_ENDPOINT ("SECRET_ENDPOINT")
/* Federated Auth */
#define KEY_IDP_ENDPOINT ("IDP_ENDPOINT")
#define KEY_IDP_USERNAME ("IDP_USERNAME")
#define KEY_IDP_PASSWORD ("IDP_PASSWORD")
#define KEY_IDP_ROLE_ARN ("IDP_ROLE_ARN")
#define KEY_IDP_SAML_ARN ("IDP_SAML_ARN")
#define KEY_HTTP_SOCKET_TIMEOUT ("HTTP_SOCKET_TIMEOUT")
#define KEY_HTTP_CONNECT_TIMEOUT ("HTTP_CONNECT_TIMEOUT")
/* ADFS */
#define KEY_RELAY_PARTY_ID ("RELAY_PARTY_ID")
/* OKTA */
#define KEY_APP_ID ("APP_ID")

/* Host Selectors */
#define KEY_HOST_SELECTOR_STRATEGY ("HOST_SELECTOR_STRATEGY")
#define VALUE_HIGHEST_WEIGHT_HOST_SELECTOR ("HIGHEST_WEIGHT")
#define VALUE_RANDOM_HOST_SELECTOR ("RANDOM_HOST")
#define VALUE_ROUND_ROBIN_HOST_SELECTOR ("ROUND_ROBIN")

/* Database Dialect */
#define KEY_DATABASE_DIALECT ("DATABASE_DIALECT")
#define VALUE_DB_DIALECT_AURORA_POSTGRESQL ("AURORA_POSTGRESQL")
#define VALUE_DB_DIALECT_AURORA_POSTGRESQL_LIMITLESS ("AURORA_POSTGRESQL_LIMITLESS")

/* Failover */
#define KEY_ENABLE_FAILOVER ("ENABLE_CLUSTER_FAILOVER")
#define KEY_FAILOVER_MODE ("FAILOVER_MODE")
#define VALUE_FAILOVER_MODE_STRICT_READER ("STRICT_READER")
#define VALUE_FAILOVER_MODE_STRICT_WRITER ("STRICT_WRITER")
#define VALUE_FAILOVER_MODE_READER_OR_WRITER ("READER_OR_WRITER")
#define KEY_ENDPOINT_TEMPLATE ("HOST_PATTERN")
#define KEY_IGNORE_TOPOLOGY_REQUEST ("IGNORE_TOPOLOGY_REQUEST_MS")
#define KEY_HIGH_REFRESH_RATE ("TOPOLOGY_HIGH_REFRESH_RATE_MS")
#define KEY_REFRESH_RATE ("TOPOLOGY_REFRESH_RATE_MS")
#define KEY_FAILOVER_TIMEOUT ("FAILOVER_TIMEOUT_MS")
#define KEY_CLUSTER_ID ("CLUSTER_ID")

/* Limitless */
#define KEY_ENABLE_LIMITLESS ("ENABLE_LIMITLESS")
#define KEY_LIMITLESS_MODE ("LIMITLESS_MODE")
#define KEY_LIMITLESS_MONITOR_INTERVAL_MS ("LIMITLESS_MONITOR_INTERVAL_MS")
#define KEY_ROUTER_MAX_RETRIES ("LIMITLESS_ROUTER_MAX_RETRIES")
#define KEY_LIMITLESS_MAX_RETRIES ("LIMITLESS_MAX_RETRIES")
#define VALUE_LIMITLESS_MODE_LAZY ("LAZY")
#define VALUE_LIMITLESS_MODE_IMMEDIATE ("IMMEDIATE")

#endif // CONNECTION_STRING_KEYS_H
