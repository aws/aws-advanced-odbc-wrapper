

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
