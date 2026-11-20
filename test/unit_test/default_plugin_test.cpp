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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../driver/driver.h"
#include "../../driver/error.h"
#include "../../driver/host_list_providers/host_list_provider.h"
#include "../../driver/odbcapi_rds_helper.h"
#include "../../driver/util/connection_string_keys.h"
#include "../../driver/util/plugin_service.h"
#include "../../driver/util/rds_lib_loader.h"
#include "../../driver/util/rds_strings.h"

namespace {
    const std::string BASE_DRIVER_PATH = "/nonexistent/default-plugin-test/libbase.so";
    const std::string CONN_STR = "DRIVER={AWS Advanced ODBC Wrapper};BASE_DRIVER=" + BASE_DRIVER_PATH
        + ";SERVER=db.example.com;DATABASE=postgres;UID=app";
    // The base driver's output connection string is longer than the wrapper's, as psqlODBC's is, and ends with an
    // option only it has.
    const std::string BASE_DRIVER_OPTION = "BaseDriverOnly=1";
    const std::string BASE_DRIVER_OPTIONS = "Options=" + std::string(300, 'x') + ";" + BASE_DRIVER_OPTION;
    constexpr SQLTCHAR FILL = 'Z';
    int fake_handle = 0;

    SQLRETURN FakeAllocHandle(SQLSMALLINT /*handle_type*/, SQLHANDLE /*input_handle*/, SQLHANDLE* output_handle) {
        *output_handle = static_cast<SQLHANDLE>(&fake_handle);
        return SQL_SUCCESS;
    }

    // The base driver writes its input string plus options of its own, cut to the buffer it gets.
    SQLRETURN FakeDriverConnect(SQLHDBC /*connection_handle*/, SQLHWND /*window_handle*/, SQLTCHAR* in,
                                SQLSMALLINT /*string_length_1*/, SQLTCHAR* out, SQLSMALLINT buffer_length,
                                SQLSMALLINT* string_length_2, SQLUSMALLINT /*driver_completion*/) {
        std::string text;
        for (const SQLTCHAR* c = in; *c != 0; ++c) {
            text.push_back(static_cast<char>(*c));
        }
        text += ";" + BASE_DRIVER_OPTIONS;
        bool truncated = false;
        if (out != nullptr && buffer_length > 0) {
            const size_t copied = std::min(text.size(), static_cast<size_t>(buffer_length) - 1);
            std::copy_n(text.begin(), copied, out);
            out[copied] = 0;
            truncated = copied < text.size();
        }
        if (string_length_2 != nullptr) {
            *string_length_2 = static_cast<SQLSMALLINT>(text.size());
        }
        return truncated ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
    }

    // Nothing is loaded from the path; RDS_InitializeConnection only compares it with BASE_DRIVER.
    class FakeBaseDriver : public RdsLibLoader {
    public:
        FakeBaseDriver() : RdsLibLoader(BASE_DRIVER_PATH) {}

        RdsPlatform::FuncHandle GetFunction(const std::string& function_name) override {
            if (function_name == RdsFuncNames::AllocHandle) {
                return reinterpret_cast<RdsPlatform::FuncHandle>(&FakeAllocHandle);
            }
            if (function_name == RdsFuncNames::DriverConnect) {
                return reinterpret_cast<RdsPlatform::FuncHandle>(&FakeDriverConnect);
            }
            return nullptr;
        }
    };

    // A plugin service whose host list provider queries no topology.
    class NoTopologyPluginService : public PluginService {
    public:
        NoTopologyPluginService(const std::shared_ptr<RdsLibLoader>& lib_loader, const ENV* env)
            : PluginService(lib_loader, env, {{KEY_SERVER, "db.example.com"}, {KEY_DRIVER, BASE_DRIVER_PATH}}) {}

        std::shared_ptr<HostListProvider> GetHostListProvider() override { return host_list_provider_; }
        void InitHostListProvider() override {}

    private:
        std::shared_ptr<HostListProvider> host_list_provider_ = std::make_shared<HostListProvider>("test");
    };

    std::vector<SQLTCHAR> AppString(const std::string& text) {
#ifdef UNICODE
        const std::vector<uint16_t> utf16 = ConvertUTF8ToUTF16(text);
        return {utf16.begin(), utf16.end()};
#else
        std::vector<SQLTCHAR> out(text.begin(), text.end());
        out.push_back(0);
        return out;
#endif
    }

    // The string at the start of the buffer, up to its terminator.
    std::string Text(const std::vector<SQLTCHAR>& buffer) {
        std::string text;
        for (size_t i = 0; i < buffer.size() && buffer[i] != 0; ++i) {
            text.push_back(static_cast<char>(buffer[i]));
        }
        return text;
    }
}  // namespace

class DefaultPluginTest : public testing::Test {
protected:
    ENV env_;
    std::vector<std::unique_ptr<DBC>> dbcs_;

    void SetUp() override {
        env_.driver_lib_loader = std::make_shared<FakeBaseDriver>();
    }

    DBC& NewHandle() {
        dbcs_.push_back(std::make_unique<DBC>());
        DBC& dbc = *dbcs_.back();
        dbc.env = &env_;
        dbc.plugin_service = std::make_shared<NoTopologyPluginService>(env_.driver_lib_loader, &env_);
        return dbc;
    }

    // SQLDriverConnect on a new handle, as an application calls it.
    SQLRETURN DriverConnect(SQLTCHAR* out, SQLSMALLINT buffer_length, SQLSMALLINT* length) {
        std::vector<SQLTCHAR> in = AppString(CONN_STR);
        return RDS_SQLDriverConnect(&NewHandle(), nullptr, in.data(), SQL_NTS, out, buffer_length, length,
                                    SQL_DRIVER_NOPROMPT);
    }

    // The wrapper's output connection string, from a buffer with room for it.
    std::string WrapperString() {
        std::vector<SQLTCHAR> out(1024, FILL);
        SQLSMALLINT length = 0;
        EXPECT_EQ(SQL_SUCCESS, DriverConnect(out.data(), static_cast<SQLSMALLINT>(out.size()), &length));
        return Text(out);
    }
};

// The buffer gets only the wrapper's string, with or without a length pointer.
TEST_F(DefaultPluginTest, DriverConnect_BufferHoldsOnlyWrapperString) {
    for (const bool with_length : {true, false}) {
        SCOPED_TRACE(with_length ? "with a length pointer" : "without a length pointer");
        std::vector<SQLTCHAR> out(1024, FILL);
        SQLSMALLINT length = -1;
        EXPECT_EQ(SQL_SUCCESS, DriverConnect(out.data(), static_cast<SQLSMALLINT>(out.size()),
                                             with_length ? &length : nullptr));
        const std::string text = Text(out);
        EXPECT_NE(std::string::npos, text.find("SERVER=db.example.com"));
        // Past the terminator too
        EXPECT_EQ(std::string::npos, std::string(out.begin(), out.end()).find(BASE_DRIVER_OPTION));
        if (with_length) {
            EXPECT_EQ(static_cast<SQLSMALLINT>(text.size()), length);
        }
    }
}

// Without a buffer, the length is that of the wrapper's string.
TEST_F(DefaultPluginTest, DriverConnect_NoBuffer_ReturnsWrapperStringLength) {
    const std::string expected = WrapperString();
    SQLSMALLINT length = -1;
    EXPECT_EQ(SQL_SUCCESS, DriverConnect(nullptr, 0, &length));
    EXPECT_EQ(static_cast<SQLSMALLINT>(expected.size()), length);
}

// A buffer too short for the wrapper's string gets a terminated prefix of it, its full length and 01004.
TEST_F(DefaultPluginTest, DriverConnect_ShortBuffer_ReturnsTerminatedPrefixAnd01004) {
    const std::string expected = WrapperString();
    std::vector<SQLTCHAR> out(expected.size(), FILL);  // one character short
    SQLSMALLINT length = -1;
    EXPECT_EQ(SQL_SUCCESS_WITH_INFO, DriverConnect(out.data(), static_cast<SQLSMALLINT>(out.size()), &length));
    EXPECT_EQ(expected.substr(0, out.size() - 1), Text(out));
    EXPECT_EQ(static_cast<SQLSMALLINT>(expected.size()), length);
    const DBC& dbc = *dbcs_.back();
    ASSERT_NE(nullptr, dbc.err);
    EXPECT_EQ("01004", dbc.err->sqlstate);
}

#ifdef UNICODE
// An application with a 4-byte SQLWCHAR, as with iODBC, gets the whole string in a buffer it just fits in.
TEST_F(DefaultPluginTest, DriverConnect_FourByteApp_GetsWholeWrapperString) {
    const std::string expected = WrapperString();
    env_.use_4_bytes_user_app.store(true);
    std::vector<uint32_t> in(CONN_STR.begin(), CONN_STR.end());
    in.push_back(0);
    std::vector<uint32_t> out(expected.size() + 1, FILL);
    SQLSMALLINT length = -1;
    EXPECT_EQ(SQL_SUCCESS, RDS_SQLDriverConnect(&NewHandle(), nullptr, reinterpret_cast<SQLTCHAR*>(in.data()), SQL_NTS,
                                                reinterpret_cast<SQLTCHAR*>(out.data()),
                                                static_cast<SQLSMALLINT>(out.size()), &length, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(expected, std::string(out.begin(), std::find(out.begin(), out.end(), 0U)));
    EXPECT_EQ(static_cast<SQLSMALLINT>(expected.size()), length);
}
#endif
