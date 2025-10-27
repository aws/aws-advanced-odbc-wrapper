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
    #include <windowsx.h>
    #include <CommCtrl.h>
#endif

#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <odbcinst.h>
#include <wchar.h>
#include <map>
#include <regex>
#include <vector>
#include <commdlg.h>

#include "resource.h"
#include "setup.h"
#include "../util/connection_string_keys.h"
#include "../util/odbc_dsn_helper.h"
#include "../odbcapi_rds_helper.h"
#include "../util/rds_lib_loader.h"

enum ControlType {
    EDIT_TEXT,
    COMBO,
    CHECK
};

enum TabSelection {
    AWS_AUTH,
    FAILOVER,
    LIMITLESS
};

enum AuthModeSelection {
    EMPTY,
    IAM,
    SECRETS_MANAGER,
    ADFS,
    OKTA
};

static const std::regex SPECIAL_CHAR = std::regex(R"([\\\[\]\{\},;?\*=!@]+)");

#if defined(_WIN32)

#define RDS_SQLGetPrivateProfileString(app_name, key_name, default_val, buff, file_name) \
    SQLGetPrivateProfileString(RDS_TSTR(app_name).c_str(), RDS_TSTR(key_name).c_str(), RDS_TSTR(default_val).c_str(), buff, sizeof(buff)/sizeof(TCHAR), _T(file_name))
#define RDS_SQLReadFileDSN(file_name, app_name, key_name, buff, out_bytes) \
    SQLReadFileDSN(RDS_TSTR(file_name).c_str(), _T(app_name), RDS_TSTR(key_name).c_str(), buff, sizeof(buff)/sizeof(wchar_t), out_bytes)

const std::map<std::string, std::pair<int, ControlType>> MAIN_KEYS = {
    {KEY_DSN, {IDC_DSN_NAME, EDIT_TEXT}},
    {KEY_DESC, {IDC_DESC, EDIT_TEXT}},
    {KEY_SERVER, {IDC_SERVER, EDIT_TEXT}},
    {KEY_DB_USERNAME, {IDC_UID, EDIT_TEXT}},
    {KEY_DB_PASSWORD, {IDC_PWD, EDIT_TEXT}},
    {KEY_PORT, {IDC_PORT, EDIT_TEXT}},
    {KEY_DATABASE, {IDC_DB, EDIT_TEXT}},
    {KEY_BASE_DSN, {IDC_BASE_DSN, EDIT_TEXT}},
    {KEY_BASE_CONN, {IDC_BASE_CONN, EDIT_TEXT}},
    {KEY_BASE_DRIVER, {IDC_BASE_DRIVER, EDIT_TEXT}}
};

const std::map<std::string, std::pair<int, ControlType>> AUTH_KEYS = {
    {KEY_AUTH_TYPE,{IDC_AUTH_MODE, COMBO}},
    {KEY_REGION, {IDC_REGION, EDIT_TEXT}},
    {KEY_TOKEN_EXPIRATION, {IDC_EXPIRE, EDIT_TEXT}}
};

const std::map<std::string, std::pair<int, ControlType>> IAM_KEYS = {
    {KEY_EXTRA_URL_ENCODE, {IDC_URL_ENCODE, CHECK}},
    {KEY_IAM_HOST, {IDC_IAM_HOST, EDIT_TEXT}},
    {KEY_IAM_PORT, {IDC_IAM_PORT, EDIT_TEXT}}
};

const std::map<std::string, std::pair<int, ControlType>> SECRETS_KEYS = {
    {KEY_SECRET_ID, {IDC_SECRET, EDIT_TEXT}},
    {KEY_SECRET_REGION, {IDC_SECRET_REGION, EDIT_TEXT}},
    {KEY_SECRET_ENDPOINT, {IDC_SECRET_END, EDIT_TEXT}}
};

const std::map<std::string, std::pair<int, ControlType>> FED_AUTH_KEYS = {
    {KEY_IDP_USERNAME, {IDC_IDP_UID, EDIT_TEXT}},
    {KEY_IDP_PASSWORD, {IDC_IDP_PWD, EDIT_TEXT}},
    {KEY_IDP_ENDPOINT, {IDC_IDP_END, EDIT_TEXT}},
    {KEY_APP_ID, {IDC_APP_ID, EDIT_TEXT}},
    {KEY_IDP_ROLE_ARN, {IDC_ROLE_ARN, EDIT_TEXT}},
    {KEY_IAM_IDP_ARN, {IDC_IDP_ARN, EDIT_TEXT}},
    {KEY_IDP_PORT, {IDC_IDP_PORT, EDIT_TEXT}},
};

const std::map<std::string, std::pair<int, ControlType>> FAILOVER_KEYS = {
    {KEY_ENABLE_FAILOVER, {IDC_ENABLE_FAILOVER, CHECK}},
    {KEY_FAILOVER_MODE, {IDC_FAILOVER_MODE, COMBO}},
    {KEY_ENDPOINT_TEMPLATE, {IDC_HOST_PATTERN, EDIT_TEXT}},
    {KEY_CLUSTER_ID, {IDC_CLUSTER_ID, EDIT_TEXT}},
    {KEY_REFRESH_RATE, {IDC_TRR, EDIT_TEXT}},
    {KEY_HIGH_REFRESH_RATE, {IDC_HIGH_TRR, EDIT_TEXT}},
    {KEY_FAILOVER_TIMEOUT, {IDC_FAILOVER_TIME, EDIT_TEXT}},
    {KEY_IGNORE_TOPOLOGY_REQUEST, {IDC_IGNORE_TR, EDIT_TEXT}},
    {KEY_HOST_SELECTOR_STRATEGY, {IDC_READER_HOST, COMBO}}
};

const std::map<std::string, std::pair<int, ControlType>> LIMITLESS_KEYS = {
    {KEY_ENABLE_LIMITLESS, {IDC_ENABLE_LIMITLESS, CHECK}},
    {KEY_LIMITLESS_MODE, {IDC_LIMITLESS_MODE, COMBO}},
    {KEY_LIMITLESS_MONITOR_INTERVAL_MS, {IDC_LIMITLESS_INTERVAL, EDIT_TEXT}},
    {KEY_LIMITLESS_MAX_RETRIES, {IDC_LIMITLESS_RETRIES, EDIT_TEXT}},
    {KEY_ROUTER_MAX_RETRIES, {IDC_ROUTER_RETRIES, EDIT_TEXT}}
};

const std::vector<std::pair<std::string, std::string>> AWS_AUTH_MODES = {
    {"Database", ""},
    {"IAM", VALUE_AUTH_IAM},
    {"Secrets Manager", VALUE_AUTH_SECRETS},
    {"ADFS", VALUE_AUTH_ADFS},
    {"OKTA", VALUE_AUTH_OKTA}
};

const std::vector<std::pair<std::string, std::string>> FAILOVER_MODES = {
    {"", ""},
    {"Strict Writer", VALUE_FAILOVER_MODE_STRICT_WRITER},
    {"Strict Reader", VALUE_FAILOVER_MODE_STRICT_READER},
    {"Reader or Writer", VALUE_FAILOVER_MODE_READER_OR_WRITER}
};

const std::vector<std::pair<std::string, std::string>> READER_SELECTION_MODES = {
    {"", ""},
    {"Random", VALUE_RANDOM_HOST_SELECTOR},
    {"Round Robin", VALUE_ROUND_ROBIN_HOST_SELECTOR },
    {"Highest Weight", VALUE_HIGHEST_WEIGHT_HOST_SELECTOR}
};

const std::vector<std::pair<std::string, std::string>> LIMITLESS_MODES = {
    {"", ""},
    {"Lazy", VALUE_LIMITLESS_MODE_LAZY},
    {"Immediate", VALUE_LIMITLESS_MODE_IMMEDIATE}
};

HINSTANCE ghInstance;
HWND tab_control;
HWND aws_auth_tab;
HWND failover_tab;
HWND limitless_tab;
HWND main_win;

std::string driver;
std::string current_dsn;
std::string connection_str;
std::string out_connection_str;
bool dialog_box_cancelled = false;
bool driver_connect = false;
bool disable_optional = false;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	ghInstance = hModule;
    return true;
}

void ChooseFile(HWND parent, int ctrl_id)
{
    TCHAR szFile[MAX_PATH];

    HWND control = GetDlgItem(parent, ctrl_id);
    GetWindowText(control, szFile, MAX_PATH);

    CHAR szFileNameOUT[MAX_PATH];

    OPENFILENAME ofn = {};
    ZeroMemory(&szFile, sizeof(szFile));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = _T("Any File\0*.*\0");
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = _T("Select File");
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        SetWindowText(control, ofn.lpstrFile);
    }
}

void AddTabToTabControl(std::string name, HWND tab_control, int index)
{
    TCITEM tie;
    tie.mask = TCIF_TEXT;
#ifdef UNICODE
    std::wstring str = ConvertUTF8ToWString(name);
    tie.pszText = const_cast<LPWSTR>(str.c_str());
#else
    tie.pszText = const_cast<LPSTR>(name.c_str());
#endif
    TabCtrl_InsertItem(tab_control, index, &tie);
}

std::string GetControlValue(HWND hwnd, std::pair<int, ControlType> pair)
{
    int id = pair.first;
    int control_type = pair.second;
    HWND control = GetDlgItem(hwnd, id);
    if (IsWindowEnabled(control)) {
        TCHAR buffer[MAX_KEY_SIZE] = {};
        if (control_type == EDIT_TEXT) {
            GetWindowText(control, buffer, MAX_KEY_SIZE);
#ifdef UNICODE
            return ConvertUTF16ToUTF8(reinterpret_cast<unsigned short*>(buffer));
#else
            return buffer;
#endif
        }

        if (control_type == COMBO) {
            int selection = ComboBox_GetCurSel(control);
            if (selection >= 0) {
                switch (id) {
                    case IDC_AUTH_MODE:
                        return AWS_AUTH_MODES[selection].second;
                    case IDC_FAILOVER_MODE:
                        return FAILOVER_MODES[selection].second;
                    case IDC_READER_HOST:
                        return READER_SELECTION_MODES[selection].second;
                    case IDC_LIMITLESS_MODE:
                        return LIMITLESS_MODES[selection].second;
                    default:
                        break;
                }
            }
        }

        if (control_type == CHECK) {
            LRESULT state = Button_GetCheck(control);
            if (state == BST_CHECKED) {
                return VALUE_BOOL_TRUE;
            }
            return VALUE_BOOL_FALSE;
        }
    }
    return "";
}

void SetInitialCheckBoxValue(HWND hwnd, int id, std::string key)
{
    HWND ctrl = GetDlgItem(hwnd, id);
    TCHAR buff[MAX_KEY_SIZE] = {};
    if (!current_dsn.empty()) {
        if (!driver_connect) {
            RDS_SQLGetPrivateProfileString(current_dsn, key, std::string(""), buff, ODBC_INI);
        } else {
            WORD cbDriver;
            RDS_SQLReadFileDSN(current_dsn, ODBC, key, buff, &cbDriver);
        }
    }

#ifdef UNICODE
    if (wcscmp(buff, RDS_TSTR(VALUE_BOOL_TRUE).c_str()) == 0) {
#else
    if (strcmp(buff, VALUE_BOOL_TRUE) == 0) {
#endif

        Button_SetCheck(ctrl, BST_CHECKED);
    } else {
        Button_SetCheck(ctrl, BST_UNCHECKED);
    }
}

void SetInitialComboBoxValue(HWND hwnd, int id, std::string key, std::vector<std::pair<std::string, std::string>> options)
{
    HWND ctrl = GetDlgItem(hwnd, id);
    TCHAR buff[MAX_KEY_SIZE] = {};
    if (!current_dsn.empty()) {
        if (!driver_connect) {
            RDS_SQLGetPrivateProfileString(current_dsn, key, std::string(""), buff, ODBC_INI);
        } else {
            WORD cbDriver;
            RDS_SQLReadFileDSN(current_dsn, ODBC, key, buff, &cbDriver);
        }
    }
    int index = 0;
    for (const auto& option : options) {
#ifdef UNICODE
        if (wcscmp(buff, ConvertUTF8ToWString(option.second).c_str()) == 0) {
#else
        if (strcmp(buff, option.second.c_str()) == 0) {
#endif
            ComboBox_SetCurSel(ctrl, index);
        }
        index++;
    }
}

void SetInitialEditTextValue(HWND hwnd, int id, std::string key, std::string value)
{
    HWND ctrl = GetDlgItem(hwnd, id);

    std::string key_value;
    if (!current_dsn.empty()) {
        TCHAR buff[MAX_KEY_SIZE] = {};
        if (!driver_connect) {
            if (value.empty()) {
                RDS_SQLGetPrivateProfileString(current_dsn, key, std::string(""), buff, ODBC_INI);
            } else {
#ifdef UNICODE
                swprintf(buff, MAX_KEY_SIZE, _T(RDS_WCHAR_FORMAT), RDS_TSTR(value).c_str());
#else
                snprintf(buff, MAX_KEY_SIZE, RDS_CHAR_FORMAT, value.c_str());
#endif
            }
            key_value = AS_UTF8_CSTR(buff);
        } else {
            WORD cbDriver;
            RDS_SQLReadFileDSN(current_dsn, ODBC, key, buff, &cbDriver);
            key_value = AS_UTF8_CSTR(buff);

            // If the key value read from a file is enclosed in curly braces, do not display the braces in the dialog box.
            if (key_value.starts_with("{") && key_value.ends_with("}")) {
                key_value = key_value.substr(1, key_value.length() - 2);
            }
        }

        SetWindowText(ctrl, RDS_TSTR(key_value).c_str());
    }
}

std::string AddKeyToConnectionString(std::string conn_str, std::string key, std::string value, bool test_conn) {
    if (!value.empty()) {
        if (key == KEY_BASE_CONN && test_conn) {
            conn_str += value;
            if (!value.ends_with(";")) {
                conn_str += ";";
            }
            return conn_str;
        }

        conn_str += key + "=";

        // Some symbols must be enclosed in curly braces to form the out connection string.
        std::smatch matches;
        if (!test_conn && !value.starts_with("{") && !value.ends_with("}") && std::regex_search(value, matches, SPECIAL_CHAR) && matches.length() > 0) {
            conn_str += "{" + value + "};";
        } else {
            conn_str += value + ";";
        }
    }

    return conn_str;
}

std::string GetDsn(bool test_conn)
{
    std::string conn_str;

    if (driver_connect) {
        conn_str = AddKeyToConnectionString(conn_str, KEY_SAVEFILE, current_dsn, test_conn);
        conn_str = AddKeyToConnectionString(conn_str, KEY_DRIVER, driver, test_conn);
    }

    std::map<std::string, std::pair<int, ControlType>> all_auth_keys = {};
    all_auth_keys.insert(AUTH_KEYS.begin(), AUTH_KEYS.end());
    all_auth_keys.insert(IAM_KEYS.begin(), IAM_KEYS.end());
    all_auth_keys.insert(SECRETS_KEYS.begin(), SECRETS_KEYS.end());
    all_auth_keys.insert(FED_AUTH_KEYS.begin(), FED_AUTH_KEYS.end());

    std::string value;
    for (const auto& keys : MAIN_KEYS) {
        value = GetControlValue(main_win, keys.second);
        if (!value.empty()) {
            conn_str = AddKeyToConnectionString(conn_str, keys.first, value, test_conn);
        }
    }

    for (const auto& keys : all_auth_keys) {
        value = GetControlValue(aws_auth_tab, keys.second);
        if (!value.empty()) {
            conn_str = AddKeyToConnectionString(conn_str, keys.first, value, test_conn);
        }
    }

    for (const auto& keys : FAILOVER_KEYS) {
        value = GetControlValue(failover_tab, keys.second);
        if (!value.empty()) {
            conn_str = AddKeyToConnectionString(conn_str, keys.first, value, test_conn);
        }
    }

    for (const auto& keys : LIMITLESS_KEYS) {
        value = GetControlValue(limitless_tab, keys.second);
        if (!value.empty()) {
            conn_str = AddKeyToConnectionString(conn_str, keys.first, value, test_conn);
        }
    }

    return conn_str;
}

void TestConnection(HWND hwnd)
{
    SQLHENV henv = SQL_NULL_HANDLE;
    SQLHDBC hdbc = SQL_NULL_HANDLE;

    RDS_AllocEnv(&henv);
    RDS_SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    RDS_AllocDbc(henv, &hdbc);

    std::string test_conn_str = GetDsn(true);

    SQLRETURN ret = RDS_SQLDriverConnect(
        hdbc,
        nullptr,
        AS_SQLTCHAR(test_conn_str),
        RDS_STR_LEN(test_conn_str.c_str()),
        0,
        MAX_KEY_SIZE,
        nullptr,
        SQL_DRIVER_NOPROMPT
    );

    if (SQL_SUCCEEDED(ret)) {
        MessageBox(hwnd, _T("Connection success"), _T("Test Connection"), MB_OK);
    } else {
        std::string fail_msg = "Connection Failed";
        SQLSMALLINT stmt_length;
        SQLINTEGER native_error;
        SQLTCHAR sql_state[MAX_KEY_SIZE] = {}, message[MAX_KEY_SIZE] = {};
        RDS_SQLError(henv, hdbc, nullptr, sql_state, &native_error, message, MAX_KEY_SIZE, &stmt_length);
        if (stmt_length > 1) {
            fail_msg += ": ";
            fail_msg += AS_UTF8_CSTR(message);
        }
        MessageBox(hwnd, RDS_TSTR(fail_msg).c_str(), _T("Test Connection"), MB_OK);
    }

    if (((ENV*)henv)->driver_lib_loader && ((DBC*)hdbc)->wrapped_dbc) {
        NULL_CHECK_CALL_LIB_FUNC(((ENV*)henv)->driver_lib_loader, RDS_FP_SQLDisconnect, RDS_STR_SQLDisconnect,
            ((DBC*)hdbc)->wrapped_dbc
        );
    }
    RDS_FreeConnect(hdbc);
    RDS_FreeEnv(henv);
}

void SaveKey(std::string profile, std::string key, std::string value)
{
    bool success = SQLWritePrivateProfileString(RDS_TSTR(profile).c_str(), RDS_TSTR(key).c_str(), RDS_TSTR(value).c_str(), _T(ODBC_INI));
    if (!success) {
        throw std::runtime_error("Unable to save value to DSN");
    }
}

bool SaveDsn()
{
    if (driver_connect) {
        connection_str = GetDsn(true);
        out_connection_str = GetDsn(false);
        return true;
    }

    std::string dsn = GetControlValue(main_win, { IDC_DSN_NAME, EDIT_TEXT });
    bool new_dsn = false;
    if (current_dsn.empty()) {
        new_dsn = true;
    } else if (dsn != current_dsn) {
        SQLRemoveDSNFromIni(RDS_TSTR(current_dsn).c_str());
        new_dsn = true;
    }

    if (!dsn.empty()) {
        if (new_dsn) {
            SaveKey(ODBC_DATA_SOURCES, dsn, driver);
        }
        
        TCHAR buff[MAX_KEY_SIZE] = {};
        RDS_SQLGetPrivateProfileString(driver, std::string(KEY_DRIVER), std::string(""), buff, ODBCINST_INI);
        SaveKey(dsn.c_str(), KEY_DRIVER, AS_UTF8_CSTR(buff));

        std::map<std::string, std::pair<int, ControlType>> all_auth_keys = {};
        all_auth_keys.insert(AUTH_KEYS.begin(), AUTH_KEYS.end());
        all_auth_keys.insert(IAM_KEYS.begin(), IAM_KEYS.end());
        all_auth_keys.insert(SECRETS_KEYS.begin(), SECRETS_KEYS.end());
        all_auth_keys.insert(FED_AUTH_KEYS.begin(), FED_AUTH_KEYS.end());

        try {
            for (const auto& keys : MAIN_KEYS) {
                if (keys.first != KEY_DSN) {
                    SaveKey(dsn.c_str(), keys.first.c_str(), GetControlValue(main_win, keys.second).c_str());
                }
            }
            for (const auto& keys : all_auth_keys) {
                SaveKey(dsn.c_str(), keys.first.c_str(), GetControlValue(aws_auth_tab, keys.second).c_str());
            }
            for (const auto& keys : FAILOVER_KEYS) {
                SaveKey(dsn.c_str(), keys.first.c_str(), GetControlValue(failover_tab, keys.second).c_str());
            }
            for (const auto& keys : LIMITLESS_KEYS) {
                SaveKey(dsn.c_str(), keys.first.c_str(), GetControlValue(limitless_tab, keys.second).c_str());
            }
        } catch (const std::runtime_error e) {
            MessageBox(main_win, _T("Failed to save DSN"), _T("Save DSN"), MB_OK);
            if (new_dsn) {
                SQLRemoveDSNFromIni(RDS_TSTR(current_dsn).c_str());
            }
        }

        return true;
    } else {
        MessageBox(main_win, _T("Please provide a data source name"), _T("Save DSN"), MB_OK);
        return false;
    }
}

void HandleEnableLimitless(HWND hwnd) {
    HWND check_box = GetDlgItem(hwnd, IDC_ENABLE_LIMITLESS);
    LRESULT state = Button_GetCheck(check_box);
    bool show_all = false;
    if (state == BST_CHECKED) {
        show_all = true;
    }

    for (const auto& keys : LIMITLESS_KEYS) {
        if (keys.first != KEY_ENABLE_LIMITLESS) {
            HWND ctrl = GetDlgItem(hwnd, keys.second.first);
            EnableWindow(ctrl, (show_all) ? TRUE : FALSE);
        }
    }
}

void HandleLimitlessInteraction(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id) {
    case IDC_ENABLE_LIMITLESS:
        HandleEnableLimitless(hwnd);
        break;
    default:
        break;
    }
}

BOOL LimitlessTabInit(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND limitless_mode = GetDlgItem(hwnd, IDC_LIMITLESS_MODE);
    for (int i = 0; i < LIMITLESS_MODES.size(); i++) {
        ComboBox_InsertString(limitless_mode, i, RDS_TSTR(LIMITLESS_MODES[i].first).c_str());
    }

    for (const auto& keys : LIMITLESS_KEYS) {
        if (keys.second.second == CHECK) {
            SetInitialCheckBoxValue(hwnd, keys.second.first, keys.first);
        }
        else if (keys.second.second == COMBO) {
            SetInitialComboBoxValue(hwnd, keys.second.first, keys.first, LIMITLESS_MODES);
        } else {
            SetInitialEditTextValue(hwnd, keys.second.first, keys.first, "");
        }
    }

    HandleEnableLimitless(hwnd);

    return false;
}

BOOL LimitlessDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        HANDLE_MSG(hwnd, WM_INITDIALOG, LimitlessTabInit);
        HANDLE_MSG(hwnd, WM_COMMAND, HandleLimitlessInteraction);
    default:
        break;
    }

    return false;
}

void HandleEnableFailover(HWND hwnd) {
    HWND check_box = GetDlgItem(hwnd, IDC_ENABLE_FAILOVER);
    LRESULT state = Button_GetCheck(check_box);
    bool show_all = false;
    if (state == BST_CHECKED) {
        show_all = true;
    }

    for (const auto& keys : FAILOVER_KEYS) {
        if (keys.first != KEY_ENABLE_FAILOVER) {
            HWND ctrl = GetDlgItem(hwnd, keys.second.first);
            EnableWindow(ctrl, (show_all) ? TRUE : FALSE);
        }
    }
}

void HandleFailoverInteraction(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id) {
        case IDC_ENABLE_FAILOVER:
            HandleEnableFailover(hwnd);
            break;
        default:
            break;
    }
}

BOOL FailoverTabInit(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND failover_mode = GetDlgItem(hwnd, IDC_FAILOVER_MODE);
    for (int i = 0; i < FAILOVER_MODES.size(); i++) {
        ComboBox_InsertString(failover_mode, i, RDS_TSTR(FAILOVER_MODES[i].first).c_str());
    }
    HWND reader_select_strat = GetDlgItem(hwnd, IDC_READER_HOST);
    for (int i = 0; i < READER_SELECTION_MODES.size(); i++) {
        ComboBox_InsertString(reader_select_strat, i, RDS_TSTR(READER_SELECTION_MODES[i].first).c_str());
    }

    for (const auto& keys : FAILOVER_KEYS) {
        if (keys.second.second == CHECK) {
            SetInitialCheckBoxValue(hwnd, keys.second.first, keys.first);
        } else if (keys.second.second == COMBO) {
            SetInitialComboBoxValue(hwnd, keys.second.first, keys.first, FAILOVER_MODES);
        } else {
            SetInitialEditTextValue(hwnd, keys.second.first, keys.first, "");
        }
    }

    HandleEnableFailover(hwnd);

    return false;
}

BOOL FailoverDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        HANDLE_MSG(hwnd, WM_INITDIALOG, FailoverTabInit);
        HANDLE_MSG(hwnd, WM_COMMAND, HandleFailoverInteraction);
        default:
            break;
    }

    return false;
}

void HandleAuthModeSelection(HWND hwnd) {
    HWND auth_mode_box = GetDlgItem(hwnd, IDC_AUTH_MODE);
    int selection = ComboBox_GetCurSel(auth_mode_box);

    std::map<std::string, std::pair<int, ControlType>> all_auth_keys = {};
    all_auth_keys.insert(AUTH_KEYS.begin(), AUTH_KEYS.end());
    all_auth_keys.insert(IAM_KEYS.begin(), IAM_KEYS.end());
    all_auth_keys.insert(SECRETS_KEYS.begin(), SECRETS_KEYS.end());
    all_auth_keys.insert(FED_AUTH_KEYS.begin(), FED_AUTH_KEYS.end());

    for (const auto& keys : all_auth_keys) {
        int id = keys.second.first;
        HWND ctrl = GetDlgItem(hwnd, id);
        bool show_ctrl = true;

        if (id != IDC_AUTH_MODE) {
            switch (selection) {
                case EMPTY:
                    show_ctrl = false;
                    break;
                case IAM:
                    if (!IAM_KEYS.contains(keys.first) && !AUTH_KEYS.contains(keys.first)) {
                        show_ctrl = false;
                    }
                    break;
                case SECRETS_MANAGER:
                    if (!SECRETS_KEYS.contains(keys.first)) {
                        show_ctrl = false;
                    }
                    break;
                case ADFS:
                    if (!IAM_KEYS.contains(keys.first) &&
                        !FED_AUTH_KEYS.contains(keys.first) &&
                        !AUTH_KEYS.contains(keys.first) ||
                        id == IDC_APP_ID ) {
                        show_ctrl = false;
                    }
                    break;
                case OKTA:
                    if (!IAM_KEYS.contains(keys.first) && !FED_AUTH_KEYS.contains(keys.first) && !AUTH_KEYS.contains(keys.first)) {
                        show_ctrl = false;
                    }
                    break;
                default:
                    break;
            }

            switch (keys.second.second) {
                case EDIT_TEXT:
                    SetInitialEditTextValue(hwnd, keys.second.first, keys.first, "");
                    break;
                case CHECK:
                    SetInitialCheckBoxValue(hwnd, keys.second.first, keys.first);
                    break;
                default:
                    break;
            }
        }

        EnableWindow(ctrl, show_ctrl);
    }
}

void HandleAuthInteraction(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id) {
        case IDC_AUTH_MODE:
            if (codeNotify == CBN_SELCHANGE) {
                HandleAuthModeSelection(hwnd);
            }
            break;
        default:
            break;
    }
}

BOOL AuthTabInit(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    HWND auth_mode_box = GetDlgItem(hwnd, IDC_AUTH_MODE);
    for (int i = 0; i < AWS_AUTH_MODES.size(); i++) {
        ComboBox_InsertString(auth_mode_box, i, RDS_TSTR(AWS_AUTH_MODES[i].first).c_str());
    }

    SetInitialComboBoxValue(hwnd, IDC_AUTH_MODE, KEY_AUTH_TYPE, AWS_AUTH_MODES);

    HandleAuthModeSelection(hwnd);

    return false;
}

BOOL AuthDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        HANDLE_MSG(hwnd, WM_INITDIALOG, AuthTabInit);
        HANDLE_MSG(hwnd, WM_COMMAND, HandleAuthInteraction);
        default:
            break;
    }

    return false;
}

void HandleGuiInteraction(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id) {
        case IDC_FILE_SELECT:
            ChooseFile(main_win, IDC_BASE_DRIVER);
            break;
        case IDCANCEL:
            dialog_box_cancelled = true;
            if (codeNotify == BN_CLICKED) {
                if (driver_connect) {
                    connection_str = GetDsn(true);
                    out_connection_str = GetDsn(false);
                }
                EndDialog(hwnd, NULL);
            }
            break;
        case IDOK:
            if (codeNotify == BN_CLICKED) {
                if (SaveDsn()) {
                    EndDialog(hwnd, NULL);
                }
            }
            break;
        case IDC_SAVE:
            if (codeNotify == BN_CLICKED) {
                bool saved = SaveDsn();
                if (saved) { // If SaveDsn fails, a message is created within the function.
                    MessageBox(main_win, _T("DSN saved successfully"), _T("Save DSN"), MB_OK);
                }
            }
            break;
        case IDTEST:
            if (codeNotify == BN_CLICKED) {
                TestConnection(hwnd);
            }
            break;
        default:
            break;
    }
}

void OnSelChange(HWND hwnd)
{
    int selection = TabCtrl_GetCurSel(tab_control);
    ShowWindow(aws_auth_tab, (selection == AWS_AUTH) ? SW_SHOW : SW_HIDE);
    ShowWindow(failover_tab, (selection == FAILOVER) ? SW_SHOW : SW_HIDE);
    ShowWindow(limitless_tab, (selection == LIMITLESS) ? SW_SHOW : SW_HIDE);
}

BOOL FormMainInit(HWND hwnd, HWND hwndFocus, LPARAM lParam)
{
    tab_control = GetDlgItem(hwnd, IDD_TABCONTROL);
    aws_auth_tab = CreateDialog(ghInstance, MAKEINTRESOURCE(IDC_TAB_AWS_AUTH), tab_control, (DLGPROC)AuthDlgProc);
    failover_tab = CreateDialog(ghInstance, MAKEINTRESOURCE(IDC_TAB_FAILOVER), tab_control, (DLGPROC)FailoverDlgProc);
    limitless_tab = CreateDialog(ghInstance, MAKEINTRESOURCE(IDC_TAB_LIMITLESS), tab_control, (DLGPROC)LimitlessDlgProc);

    if (driver_connect) {
        HWND dsn_text = GetDlgItem(hwnd, IDC_DSN_NAME);
        HWND save_btn = GetDlgItem(hwnd, IDC_SAVE);
        EnableWindow(dsn_text, SW_HIDE);
        EnableWindow(save_btn, SW_HIDE);
    }

    for (const auto& keys : MAIN_KEYS) {
        if (keys.first == KEY_DSN && !driver_connect) {
            SetInitialEditTextValue(hwnd, keys.second.first, keys.first, current_dsn);
        } else {
            SetInitialEditTextValue(hwnd, keys.second.first, keys.first, "");
        }
    }

    AddTabToTabControl("Authentication", tab_control, AWS_AUTH);
    AddTabToTabControl("Failover Settings", tab_control, FAILOVER);
    AddTabToTabControl("Limitless", tab_control, LIMITLESS);

    SendMessage(tab_control, TCM_SETCURSEL, 0, 0);
    OnSelChange(hwnd);

    if (disable_optional) {
        std::vector<int> optional_main_ctrls = {IDC_DESC, IDC_BASE_DSN, IDC_BASE_CONN};
        for (const auto& id : optional_main_ctrls) {
            HWND control = GetDlgItem(main_win, id);
            EnableWindow(control, SW_HIDE);
        }

        std::map<std::string, std::pair<int, ControlType>> all_auth_keys = {};
        all_auth_keys.insert(AUTH_KEYS.begin(), AUTH_KEYS.end());
        all_auth_keys.insert(IAM_KEYS.begin(), IAM_KEYS.end());
        all_auth_keys.insert(SECRETS_KEYS.begin(), SECRETS_KEYS.end());
        all_auth_keys.insert(FED_AUTH_KEYS.begin(), FED_AUTH_KEYS.end());
        for (const auto& keys : all_auth_keys) {
            HWND control = GetDlgItem(aws_auth_tab, keys.second.first);
            EnableWindow(control, SW_HIDE);
        }
        EnableWindow(tab_control, SW_HIDE);
    }

    return false;
}

BOOL FormMainDlgProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    main_win = hwnd;

    switch (msg) {
        HANDLE_MSG(hwnd, WM_COMMAND, HandleGuiInteraction);
        HANDLE_MSG(hwnd, WM_INITDIALOG, FormMainInit);
        case WM_NOTIFY:
            switch (((LPNMHDR)lParam)->code)
            {
            case TCN_SELCHANGE:
                OnSelChange(hwnd);
                break;
            }
            break;
        default:
            break;
    }
    return false;
}

void GetSaveFileFromConnectionString(std::string conn_str, HWND hwndParent) {
    std::smatch matches;
    std::regex dsn_pattern = std::regex("SAVEFILE=([^;]*)(;)?");
    if (std::regex_search(conn_str, matches, dsn_pattern) && !matches.empty()) {
        std::string match = matches[1];
        current_dsn = match;
    }
}

void GetDsnFromConnectionString(std::string conn_str, HWND hwndParent) {
    std::smatch matches;
    std::regex dsn_pattern = std::regex("DSN=([^;]*)(;)?");
    if (std::regex_search(conn_str, matches, dsn_pattern) && !matches.empty()) {
        std::string match = matches[1];
        current_dsn = match;
    }
}

void GetDriverFromConnectionString(std::string conn_str, HWND hwndParent) {
    std::smatch matches;
    std::regex driver_pattern = std::regex("DRIVER=([^;]*)(;)?");
    if (std::regex_search(conn_str, matches, driver_pattern) && !matches.empty()) {
        std::string match = matches[1];
        driver = match;
    }
}

std::tuple<std::string, std::string, bool> StartDialogForSqlDriverConnect(HWND hwnd, SQLTCHAR* InConnectionString, SQLTCHAR* OutConnectionString, bool complete_required) {
    connection_str = "";
    out_connection_str = "";
    driver_connect = true;
    disable_optional = complete_required;
    std::string converted_str;

    // Check if SAVEFILE is specified.
#ifdef UNICODE
    converted_str = ConvertUTF16ToUTF8(reinterpret_cast<unsigned short*>(InConnectionString));
#else
    converted_str = reinterpret_cast<char*>(InConnectionString);
#endif
    GetSaveFileFromConnectionString(converted_str, hwnd);
    GetDriverFromConnectionString(converted_str, hwnd);

    if (current_dsn.empty()) {
        // Check for DSN and DRIVER from the connection string.
        GetDsnFromConnectionString(converted_str, hwnd);
        if (!current_dsn.empty() || !driver.empty()) {
            return { converted_str, converted_str, false };
        }
    }

    // If SAVEFILE is specified, or if the connection string does not contain either the DRIVER, DSN, or FILEDSN keywords, the Driver Manager displays the Data Sources dialog box.
    DialogBox(ghInstance, MAKEINTRESOURCE(IDD_DIALOG_MAIN), hwnd, (DLGPROC)FormMainDlgProc);

    driver_connect = false;
    disable_optional = false;
    return { connection_str, out_connection_str, dialog_box_cancelled };
}
#endif

std::string ConvertNullSeparatedToSemicolon(LPCSTR lpszAttributes) {
    // https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/configdsn-function?view=sql-server-ver17#arguments
    // lpszAttributes is a doubly null-terminated list of attributes like so "Server=test\0Port=1234\0\0"
    std::string result;
    const char* ptr = lpszAttributes;
    while (*ptr) {
        if (!result.empty()) result += ";";
        result += ptr;
        ptr += strlen(ptr) + 1;
    }
    return result;
}

BOOL ConfigDSN(HWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszAttributes) {
    driver = lpszDriver;
    const std::string conn_str = ConvertNullSeparatedToSemicolon(lpszAttributes);
    GetDsnFromConnectionString(conn_str, hwndParent);

    switch (fRequest) {
       case ODBC_ADD_DSN:
       case ODBC_CONFIG_DSN:

           if (hwndParent) {
               main_win = hwndParent;
               DialogBox(ghInstance, MAKEINTRESOURCE(IDD_DIALOG_MAIN), hwndParent, (DLGPROC)FormMainDlgProc);
               break;
           }

           // Handle the case where hwndParent is NULL.
           // This can happen if ConfigDSN is called from a non-GUI context, like the command line.

           if (!conn_str.empty() && !current_dsn.empty()) {
               SQLWritePrivateProfileString(_T(ODBC_DATA_SOURCES), RDS_TSTR(current_dsn).c_str(), RDS_TSTR(driver).c_str(), _T(ODBC_INI));
               TCHAR driver_path[MAX_KEY_SIZE] = {};
               RDS_SQLGetPrivateProfileString(driver, std::string(KEY_DRIVER), std::string(""), driver_path, ODBCINST_INI);
               SQLWritePrivateProfileString(RDS_TSTR(current_dsn).c_str(), _T(KEY_DRIVER), driver_path, _T(ODBC_INI));

               // Parse key-value pair like so: Server=test;Port=1234
               const std::regex kv_pattern(R"(([^=;]+)=([^;]*)(;|$))");
               std::sregex_iterator iter(conn_str.begin(), conn_str.end(), kv_pattern);

               for (const std::sregex_iterator end; iter != end; ++iter) {
                   std::string key = (*iter)[1].str();
                   std::string value = (*iter)[2].str();

                   if (key != "DSN") {
                       SQLWritePrivateProfileString(RDS_TSTR(current_dsn).c_str(), RDS_TSTR(key).c_str(), RDS_TSTR(value).c_str(), _T(ODBC_INI));
                   }
               }
           }
           break;

       case ODBC_REMOVE_DSN:
           SQLRemoveDSNFromIni(RDS_TSTR(current_dsn).c_str());
           break;
       default:
           break;
    }
    return true;
}

// This function has been left empty intentionally, using WiX for driver installation instead.
BOOL ConfigDriver(HWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszArgs, LPSTR lpszMsg,
    WORD cbMsgMax, WORD* pcbMsgOut) {
    return true;
}
