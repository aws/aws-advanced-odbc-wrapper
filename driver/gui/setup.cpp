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

#ifdef WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <odbcinst.h>

#define NOT_IMPLEMENTED \
     return SQL_ERROR

// GUI Related
// TODO - Impl ConfigDriver
// Later process
BOOL ConfigDriver(SQLHWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszArgs, LPSTR lpszMsg,
                WORD cbMsgMax, WORD* pcbMsgOut) {
    NOT_IMPLEMENTED;
}

// TODO - Impl ConfigDSN
// Later process
BOOL ConfigDSN(SQLHWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszAttributes) {
    NOT_IMPLEMENTED;
}
