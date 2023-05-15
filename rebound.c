#include "rebound.h"

#ifdef RE_OS_LINUX
#include <dlfcn.h>
#endif

/*=========================*/
// Dynamic library loading
/*=========================*/

re_lib_t re_lib_load(const char *path) {
#ifdef RE_OS_LINUX
    re_lib_t lib = {0};
    lib.handle = dlopen(path, RTLD_LAZY);
#include <stdio.h>
    if (lib.handle != NULL) {
        lib.valid = true;
    }
    return lib;
#endif
}

void re_lib_unload(re_lib_t *lib) {
#ifdef RE_OS_LINUX
    if (!lib->valid) {
        return;
    }

    dlclose(lib->handle);
    lib->handle = NULL;
    lib->valid = false;
#endif
}

re_func_ptr_t re_lib_func(re_lib_t lib, const char *name) {
#ifdef RE_OS_LINUX
    if (!lib.valid) {
        return NULL;
    }

    re_func_ptr_t ptr;
    *((void **) &ptr) = dlsym(lib.handle, name);

    return ptr;
#endif
}
