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

#ifndef RW_SPLITTING_MOCK_OBJECTS_H_
#define RW_SPLITTING_MOCK_OBJECTS_H_

#include "auth_mock_objects.h"
#include "common_mock_objects.h"

#include "../../driver/driver.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/util/rds_lib_loader.h"

// ---------------------------------------------------------------------------
// RdsLibLoader mock – returns a no-op function pointer for every lookup.
// We define our own here rather than including failover_mock_objects.h to
// avoid pulling in the failover plugin dependency.
// ---------------------------------------------------------------------------
inline SQLRETURN RW_MockFunction() { return SQL_SUCCESS; }

class RW_MockRdsLibLoader : public RdsLibLoader {
public:
    RW_MockRdsLibLoader() : RdsLibLoader("") {}

    FUNC_HANDLE GetFunction(const std::string& function_name) override {
        return reinterpret_cast<FUNC_HANDLE>(&RW_MockFunction);
    }
};

#endif // RW_SPLITTING_MOCK_OBJECTS_H_
