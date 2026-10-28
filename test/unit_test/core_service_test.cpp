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

#include <cstdint>
#include <thread>
#include <vector>

#include "../../driver/driver.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/util/core_service.h"

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
