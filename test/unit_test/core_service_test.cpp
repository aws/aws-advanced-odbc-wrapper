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

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../../driver/driver.h"
#include "../../driver/odbcapi.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/util/core_service.h"
#include "../../driver/util/rds_lib_loader.h"

// The count is process wide, so every test measures against the count it started with
class CoreServiceTest : public testing::Test {
protected:
    uint32_t initial_count_ = 0;

    void SetUp() override {
        initial_count_ = CoreService::Instance().GetEnvCount();
    }

    void TearDown() override {
        EXPECT_EQ(initial_count_, CoreService::Instance().GetEnvCount());
    }

    [[nodiscard]] uint32_t Counted() const {
        return CoreService::Instance().GetEnvCount() - initial_count_;
    }
};

TEST_F(CoreServiceTest, SameInstanceAndServices) {
    CoreService& core_service = CoreService::Instance();
    EXPECT_EQ(&core_service, &CoreService::Instance());
    EXPECT_EQ(&core_service.GetStorageService(), &CoreService::Instance().GetStorageService());
    EXPECT_EQ(&core_service.GetMonitorService(), &CoreService::Instance().GetMonitorService());
}

TEST_F(CoreServiceTest, AppEnvCounted) {
    SQLHENV henv = SQL_NULL_HENV;
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocEnv(&henv));
    ASSERT_NE(nullptr, henv);
    EXPECT_TRUE(static_cast<ENV*>(henv)->counted_by_core_service);
    EXPECT_EQ(1u, Counted());

    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(henv));
    EXPECT_EQ(0u, Counted());
}

TEST_F(CoreServiceTest, InternalEnvNotCounted) {
    SQLHENV app_henv = SQL_NULL_HENV;
    SQLHENV internal_henv = SQL_NULL_HENV;
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocEnv(&app_henv));
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocInternalEnv(&internal_henv));
    EXPECT_FALSE(static_cast<ENV*>(internal_henv)->counted_by_core_service);
    EXPECT_EQ(1u, Counted());

    // Freeing it does not release the app's count
    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(internal_henv));
    EXPECT_EQ(1u, Counted());

    // And a monitor's ENV outliving the app's does not keep the count above 0
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocInternalEnv(&internal_henv));
    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(app_henv));
    EXPECT_EQ(0u, Counted());
    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(internal_henv));
    EXPECT_EQ(0u, Counted());
}

TEST_F(CoreServiceTest, InstanceSurvivesLastEnv) {
    const CoreService* core_service = &CoreService::Instance();
    SQLHENV henv = SQL_NULL_HENV;
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocEnv(&henv));
    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(henv));

    // A new ENV after the last one was freed reuses the same instance
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocEnv(&henv));
    EXPECT_EQ(core_service, &CoreService::Instance());
    EXPECT_EQ(1u, Counted());
    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(henv));
}

TEST_F(CoreServiceTest, ExtraFreeIgnored) {
    if (initial_count_ != 0) {
        GTEST_SKIP() << "Needs no other counted ENV";
    }
    CoreService::Instance().OnEnvFreed();
    EXPECT_EQ(0u, CoreService::Instance().GetEnvCount());

    // Still counts correctly afterwards
    CoreService::Instance().OnEnvAllocated();
    EXPECT_EQ(1u, Counted());
    CoreService::Instance().OnEnvFreed();
}

namespace {
    constexpr int THREAD_COUNT = 8;
    constexpr int ITERATIONS = 25;

    template <typename Work>
    void RunConcurrently(Work work) {
        std::vector<std::thread> threads;
        threads.reserve(THREAD_COUNT);
        for (int i = 0; i < THREAD_COUNT; i++) {
            threads.emplace_back([&work] {
                for (int j = 0; j < ITERATIONS; j++) {
                    work();
                }
            });
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
    }
}  // namespace

TEST_F(CoreServiceTest, ConcurrentCount) {
    // Crosses 0 repeatedly, releasing the monitors each time
    RunConcurrently([] {
        CoreService::Instance().OnEnvAllocated();
        CoreService::Instance().OnEnvFreed();
    });
    EXPECT_EQ(0u, Counted());
}

TEST_F(CoreServiceTest, ConcurrentAllocAndFree) {
    // Keeps logging initialized, so this does not also race LoggerWrapper's own initialize / shutdown
    SQLHENV keep_henv = SQL_NULL_HENV;
    ASSERT_EQ(SQL_SUCCESS, RDS_AllocEnv(&keep_henv));
    RunConcurrently([] {
        SQLHENV henv = SQL_NULL_HENV;
        if (RDS_AllocEnv(&henv) == SQL_SUCCESS) {
            RDS_FreeEnv(henv);
        }
    });
    EXPECT_EQ(1u, Counted());
    EXPECT_EQ(SQL_SUCCESS, RDS_FreeEnv(keep_henv));
}

namespace {
    // Calls to FakeAllocHandle for an ENV, and whether they fail
    std::atomic<int> env_alloc_calls{0};
    std::atomic<bool> env_alloc_fails{false};
    int fake_env = 0;

    SQLRETURN FakeAllocHandle(SQLSMALLINT handle_type, SQLHANDLE /*input_handle*/, SQLHANDLE* output_handle) {
        if (handle_type != SQL_HANDLE_ENV) {
            return SQL_ERROR;
        }
        ++env_alloc_calls;
        if (env_alloc_fails) {
            return SQL_ERROR;
        }
        *output_handle = &fake_env;
        return SQL_SUCCESS;
    }

    // Nothing is loaded from the path, only SQLAllocHandle is found
    class PinMockRdsLibLoader : public RdsLibLoader {
    public:
        explicit PinMockRdsLibLoader(std::string driver_path) : RdsLibLoader(std::move(driver_path)) {}

        RdsPlatform::FuncHandle GetFunction(const std::string& function_name) override {
            if (function_name == RdsFuncNames::AllocHandle) {
                return reinterpret_cast<RdsPlatform::FuncHandle>(&FakeAllocHandle);
            }
            return nullptr;
        }
    };
}  // namespace

// Tests PinBaseDriver, which keeps one ENV of each base driver open so it never tears down its global state
// Pins are process wide and never freed, so every test pins paths of its own
class CoreServicePinTest : public testing::Test {
protected:
    void SetUp() override {
        env_alloc_calls = 0;
        env_alloc_fails = false;
    }

    [[nodiscard]] static std::string NewDriverPath() {
        static std::atomic<int> next_id{0};
        return "/nonexistent/core-service-pin-test-" + std::to_string(next_id++) + ".so";
    }
};

TEST_F(CoreServicePinTest, PinsOncePerDriverPath) {
    const std::string driver_path = NewDriverPath();
    EXPECT_FALSE(CoreService::Instance().IsBaseDriverPinned(driver_path));

    CoreService::Instance().PinBaseDriver(std::make_shared<PinMockRdsLibLoader>(driver_path));
    EXPECT_EQ(1, env_alloc_calls);
    EXPECT_TRUE(CoreService::Instance().IsBaseDriverPinned(driver_path));

    // Every ENV has its own loader, a new one for the same driver is not pinned again
    CoreService::Instance().PinBaseDriver(std::make_shared<PinMockRdsLibLoader>(driver_path));
    EXPECT_EQ(1, env_alloc_calls);

    const std::string other_driver_path = NewDriverPath();
    CoreService::Instance().PinBaseDriver(std::make_shared<PinMockRdsLibLoader>(other_driver_path));
    EXPECT_EQ(2, env_alloc_calls);
    EXPECT_TRUE(CoreService::Instance().IsBaseDriverPinned(other_driver_path));
}

TEST_F(CoreServicePinTest, FailedPinRetried) {
    const std::string driver_path = NewDriverPath();
    const auto lib_loader = std::make_shared<PinMockRdsLibLoader>(driver_path);

    env_alloc_fails = true;
    CoreService::Instance().PinBaseDriver(lib_loader);
    EXPECT_EQ(1, env_alloc_calls);
    EXPECT_FALSE(CoreService::Instance().IsBaseDriverPinned(driver_path));

    env_alloc_fails = false;
    CoreService::Instance().PinBaseDriver(lib_loader);
    EXPECT_EQ(2, env_alloc_calls);
    EXPECT_TRUE(CoreService::Instance().IsBaseDriverPinned(driver_path));
}

TEST_F(CoreServicePinTest, MissingAllocHandleNotPinned) {
    // The base loader finds no function in a module that did not load
    const std::string driver_path = NewDriverPath();
    CoreService::Instance().PinBaseDriver(std::make_shared<RdsLibLoader>(driver_path));
    EXPECT_FALSE(CoreService::Instance().IsBaseDriverPinned(driver_path));
}

TEST_F(CoreServicePinTest, NullLoaderIgnored) {
    CoreService::Instance().PinBaseDriver(nullptr);
    EXPECT_EQ(0, env_alloc_calls);
}

TEST_F(CoreServicePinTest, ConcurrentPinsOnce) {
    const std::string driver_path = NewDriverPath();
    RunConcurrently([&driver_path] {
        CoreService::Instance().PinBaseDriver(std::make_shared<PinMockRdsLibLoader>(driver_path));
    });
    EXPECT_EQ(1, env_alloc_calls);
    EXPECT_TRUE(CoreService::Instance().IsBaseDriverPinned(driver_path));
}
