#include "rds_lib_loader.h"

#include <mutex>

RdsLibLoader::RdsLibLoader(RDS_STR library_path)
{
    driver_path = library_path;
    driver_handle = RDS_LOAD_MODULE_DEFAULTS(library_path.c_str());
}

RdsLibLoader::~RdsLibLoader()
{
    if (driver_handle) {
        RDS_FREE_MODULE(driver_handle);
    }
}

RDS_STR RdsLibLoader::GetDriverPath()
{
    return driver_path;
}

FUNC_HANDLE RdsLibLoader::GetFunction(RDS_STR func_name)
{
    FUNC_HANDLE driver_function = RDS_GET_FUNC(driver_handle, AS_CONST_CHAR(func_name.c_str()));
    if (driver_function) {
        std::unique_lock lock(cache_lock);
        function_cache.insert_or_assign(func_name, driver_function);
    }
    return driver_function;
}
