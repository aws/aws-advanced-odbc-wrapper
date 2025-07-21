#ifndef RDS_LIB_LOADER_H
#define RDS_LIB_LOADER_H

#ifdef _WIN32
    #include <windows.h>
    #define MODULE_HANDLE HINSTANCE
    #define RDS_LOAD_MODULE_DEFAULTS(module_name) RDS_LOAD_MODULE(module_name, LOAD_WITH_ALTERED_SEARCH_PATH)
    #define RDS_LOAD_MODULE(module_name, load_flag) LoadLibraryEx(module_name, NULL, load_flag)
    #define RDS_FREE_MODULE(handle) FreeLibrary(handle)
    #define RDS_GET_FUNC(handle, fn_name) GetProcAddress(handle, fn_name)
#else // Unix (Linux / MacOS)
    #include <dlfcn.h>
    #define MODULE_HANDLE void*
    #define RDS_LOAD_MODULE_DEFAULTS(module_name) RDS_LOAD_MODULE(module_name, RTLD_LAZY | RTLD_LOCAL)
    #define RDS_LOAD_MODULE(module_name, load_flag) dlopen(module_name, load_flag)
    #define RDS_FREE_MODULE(handle) dlclose(handle)
    #define RDS_GET_FUNC(handle, fn_name) dlsym(handle, fn_name)
#endif

#endif // RDS_LIB_LOADER_H
