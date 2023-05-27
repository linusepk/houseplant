#include "rebound.h"

#ifdef RE_OS_LINUX
#include <dlfcn.h>
#include <sys/mman.h>
#endif

#include <assert.h>
#include <malloc.h>
#include <stdlib.h>

//  ____                   _
// | __ )  __ _ ___  ___  | |    __ _ _   _  ___ _ __
// |  _ \ / _` / __|/ _ \ | |   / _` | | | |/ _ \ '__|
// | |_) | (_| \__ \  __/ | |__| (_| | |_| |  __/ |
// |____/ \__,_|___/\___| |_____\__,_|\__, |\___|_|
//                                    |___/

/*=========================*/
// User defined API
/*=========================*/

void *re_libc_reserve(usize_t size, void *ctx) {
    (void) ctx;
    return malloc(size);
}
void re_libc_commit(void *ptr, usize_t size, void *ctx) {
    (void) ptr;
    (void) size;
    (void) ctx;
}
void re_libc_decommit(void *ptr, usize_t size, void *ctx) {
    (void) ptr;
    (void) size;
    (void) ctx;
}
void re_libc_release(void *ptr, usize_t size, void *ctx) {
    (void) size;
    (void) ctx;
    free(ptr);
}

/*=========================*/
// Utils
/*=========================*/

u32_t fvn1a_hash(const char *key, u32_t len) {
    u32_t hash = 2166136261u;
    for (u32_t i = 0; i < len; i++) {
        hash ^= (u8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

//  ____  _       _    __                        _
// |  _ \| | __ _| |_ / _| ___  _ __ _ __ ___   | |    __ _ _   _  ___ _ __
// | |_) | |/ _` | __| |_ / _ \| '__| '_ ` _ \  | |   / _` | | | |/ _ \ '__|
// |  __/| | (_| | |_|  _| (_) | |  | | | | | | | |__| (_| | |_| |  __/ |
// |_|   |_|\__,_|\__|_|  \___/|_|  |_| |_| |_| |_____\__,_|\__, |\___|_|
//                                                          |___/

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
#endif // RE_OS_LINUX
}

void re_lib_unload(re_lib_t *lib) {
#ifdef RE_OS_LINUX
    if (!lib->valid) {
        return;
    }

    dlclose(lib->handle);
    lib->handle = NULL;
    lib->valid = false;
#endif // RE_OS_LINUX
}

re_func_ptr_t re_lib_func(re_lib_t lib, const char *name) {
#ifdef RE_OS_LINUX
    if (!lib.valid) {
        return NULL;
    }

    re_func_ptr_t ptr;
    *((void **) &ptr) = dlsym(lib.handle, name);

    return ptr;
#endif // RE_OS_LINUX
}

/*=========================*/
// Allocators
/*=========================*/

#ifdef RE_OS_LINUX
void *re_platform_reserve(usize_t size) {
    void *ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return ptr;
}

void re_platform_commit(void *ptr, usize_t size) {
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
    madvise(ptr, size, MADV_WILLNEED);
}

void re_platform_decommit(void *ptr, usize_t size) {
    mprotect(ptr, size, PROT_NONE);
    madvise(ptr, size, MADV_DONTNEED);
}

void re_platform_release(void *ptr, usize_t size) {
    munmap(ptr, size);
}
#endif // RE_OS_LINUX

void *_re_platform_reserve(usize_t size, void *ctx) {
    (void) ctx;
    return re_platform_reserve(size);
}

void _re_platform_commit(void *ptr, usize_t size, void *ctx) {
    (void) ctx;
    re_platform_commit(ptr, size);
}

void _re_platform_decommit(void *ptr, usize_t size, void *ctx) {
    (void) ctx;
    re_platform_decommit(ptr, size);
}

void _re_platform_release(void *ptr, usize_t size, void *ctx) {
    (void) ctx;
    re_platform_release(ptr, size);
}
