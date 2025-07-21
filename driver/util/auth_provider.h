#ifndef AUTH_PROVIDER_H_
#define AUTH_PROVIDER_H_

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/rds/RDSClient.h>

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "aws_sdk_helper.h"
#include "connection_string_keys.h"
#include "rds_strings.h"

typedef enum {
    PASSWORD,
    IAM,
    SECRETS_MANAGER,
    ADFS,
    OKTA,
    INVALID,
} AuthType;

static std::map<RDS_STR, AuthType> const auth_table = {
    {VALUE_AUTH_PASSWORD,   AuthType::PASSWORD},
    {VALUE_AUTH_IAM,        AuthType::IAM},
    {VALUE_AUTH_SECRETS,    AuthType::SECRETS_MANAGER},
    {VALUE_AUTH_ADFS,       AuthType::ADFS},
    {VALUE_AUTH_OKTA,       AuthType::OKTA},
};

struct TokenInfo {
    std::string token;
    std::chrono::time_point<std::chrono::system_clock> expiration_point;
}; // TokenInfo;

class AuthProvider {
public:
    AuthProvider() = default;
    AuthProvider(const std::string &region);
    AuthProvider(const std::string &region, Aws::Auth::AWSCredentials credentials);
    ~AuthProvider();

    virtual std::pair<std::string, bool> GetToken(
        const std::string &server,
        const std::string &region,
        const std::string &port,
        const std::string &username,
        bool use_cache = true,
        std::chrono::milliseconds time_to_expire_ms = DEFAULT_EXPIRATION_MS);

    static inline const std::chrono::milliseconds
        DEFAULT_EXPIRATION_MS = std::chrono::minutes(15);
    static AuthType AuthTypeFromString(const RDS_STR& auth_type);
    static std::string BuildCacheKey(
        const std::string &server,
        const std::string &region,
        const std::string &port,
        const std::string &username);
    static void ClearCache();

protected:
private:
    static inline std::unordered_map<std::string, TokenInfo> token_cache;
    static inline std::recursive_mutex token_cache_mutex;
    std::shared_ptr<Aws::RDS::RDSClient> rds_client;
};

#endif // AUTH_PROVIDER_H_
