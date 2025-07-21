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
#define BOOL_FALSE TEXT("0")
#define BOOL_TRUE TEXT("1")

#define KEY_DATABASE TEXT("DATABASE")
#define KEY_SERVER TEXT("SERVER")
#define KEY_PORT TEXT("PORT")

#define KEY_BASE_DRIVER TEXT("BASE_DRIVER")
#define KEY_DRIVER TEXT("DRIVER")
#define KEY_BASE_DSN TEXT("BASE_DSN")
#define KEY_DSN TEXT("DSN")

/* Auth */
#define KEY_AUTH_TYPE TEXT("RDS_AUTH_TYPE")
#define KEY_DB_USERNAME TEXT("UID")
#define KEY_DB_PASSWORD TEXT("PWD")

/* Auth Types */
#define VALUE_AUTH_DATABASE TEXT("DATABSE")
#define VALUE_AUTH_IAM TEXT("IAM")
#define VALUE_AUTH_SECRETS TEXT("SECRETS_MANAGER")
#define VALUE_AUTH_ADFS TEXT("ADFS")
#define VALUE_AUTH_OKTA TEXT("OKTA")

/* Common Auth Settings */
#define KEY_REGION TEXT("REGION")
#define KEY_TOKEN_EXPIRATION TEXT("TOKEN_EXPIRATION")
#define KEY_SSL_MODE TEXT("SSL_MODE")

/* IAM */
/* Secrets Manager */
#define KEY_SECRET_ID TEXT("SECRET_ID")
/* ADFS */
#define KEY_IDP_ENDPOINT TEXT("IDP_ENDPOINT")
#define KEY_IDP_PORT TEXT("IDP_PORT")
#define KEY_IDP_USERNAME TEXT("IDP_USERNAME")
#define KEY_IDP_PASSWORD TEXT("IDP_PASSWORD")
#define KEY_IDP_ROLE_ARN TEXT("IDP_ROLE_ARN")
#define KEY_IDP_IDP_ARN TEXT("IDP_IDP_ARN")
#define KEY_HTTP_SOCKET_TIMEOUT TEXT("HTTP_SOCKET_TIMEOUT")
#define KEY_HTTP_CONNECT_TIMEOUT TEXT("HTTP_CONNECT_TIMEOUT")
#define KEY_RELAY_PARTY_ID TEXT("RELAY_PARTY_ID")
/* OKTA */
#define KEY_APP_ID TEXT("APP_ID")

#endif // CONNECTION_STRING_KEYS_H
