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

#ifndef ODBC_HELPER_H
#define ODBC_HELPER_H

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>

#include <string>
#include <memory>

#include "../dialect/dialect.h"

static const int MAX_HOST_SIZE = 1024;
std::string GetNodeId(SQLHDBC hdbc, const std::shared_ptr<Dialect>& dialect);

#endif //ODBC_HELPER_H
