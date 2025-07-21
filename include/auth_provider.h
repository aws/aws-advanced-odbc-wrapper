#ifndef AUTH_PROVIDER_H_
#define AUTH_PROVIDER_H_

#include <map>

#include "connection_string_keys.h"
#include "rds_strings.h"

typedef enum {
    DATABASE,
    IAM,
    SECRETS_MANAGER,
    ADFS,
    OKTA,
    INVALID,
} AuthType;

static std::map<RDS_STR, AuthType> const auth_table = {
    {VALUE_AUTH_DB, AuthType::DATABASE},
    {VALUE_AUTH_IAM, AuthType::IAM},
    {VALUE_AUTH_SECRETS, AuthType::SECRETS_MANAGER},
    {VALUE_AUTH_ADFS, AuthType::ADFS},
    {VALUE_AUTH_OKTA, AuthType::OKTA},
};

AuthType AuthTypeFromString(const RDS_STR& auth_type) {
    auto itr = auth_table.find(auth_type);
    if (itr != auth_table.end()) {
        return itr->second;
    } else {
        return AuthType::INVALID;
    }
}

#endif // AUTH_PROVIDER_H_