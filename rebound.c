#include "rebound.h"

#ifdef RE_OS_LINUX
#include <dlfcn.h>
#include <sys/mman.h>
#endif

#ifndef RE_NOLIBC
#include <stdlib.h>
#endif // RE_NOLIBC

//  ____                   _
// | __ )  __ _ ___  ___  | |    __ _ _   _  ___ _ __
// |  _ \ / _` / __|/ _ \ | |   / _` | | | |/ _ \ '__|
// | |_) | (_| \__ \  __/ | |__| (_| | |_| |  __/ |
// |____/ \__,_|___/\___| |_____\__,_|\__, |\___|_|
//                                    |___/
// Base layer

/*=========================*/
// Allocator interface
/*=========================*/

void re_allocator_change_memory_dummy(void *ptr, usize_t size, void *ctx) {
    (void) ptr;
    (void) size;
    (void) ctx;
}

#ifndef RE_NOLIBC
void *_re_libc_reserve(usize_t size, void *ctx) {
    (void) ctx;
    return malloc(size);
}

void _re_libc_release(void *ptr, usize_t size, void *ctx) {
    (void) size;
    (void) ctx;
    free(ptr);
}
#endif // RE_NOLIBC

/*=========================*/
// Utils
/*=========================*/

usize_t re_fvn1a_hash(const char *key, usize_t len) {
    u32_t hash = 2166136261u;
    for (u32_t i = 0; i < len; i++) {
        hash ^= (u8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

void re_memset(void *dest, u8_t value, usize_t size) {
    u8_t *_dest = dest;
    while (size-- > 0) {
        _dest[size] = value;
    }
}

//  ____  _       _    __                        _
// |  _ \| | __ _| |_ / _| ___  _ __ _ __ ___   | |    __ _ _   _  ___ _ __
// | |_) | |/ _` | __| |_ / _ \| '__| '_ ` _ \  | |   / _` | | | |/ _ \ '__|
// |  __/| | (_| | |_|  _| (_) | |  | | | | | | | |__| (_| | |_| |  __/ |
// |_|   |_|\__,_|\__|_|  \___/|_|  |_| |_| |_| |_____\__,_|\__, |\___|_|
//                                                          |___/
// Platform layer

#ifdef RE_OS_LINUX
struct re_lib_t {
    void *handle;
    re_allocator_t alloc;
    b8_t valid;
};

/*=========================*/
// Dynamic library loading
/*=========================*/

re_lib_t *re_lib_load(const char *path, re_allocator_t allocator) {
    re_lib_t *lib = allocator.reserve(sizeof(re_lib_t), allocator.ctx);
    allocator.commit(lib, sizeof(re_lib_t), allocator.ctx);
    *lib = (re_lib_t) {0};
    lib->alloc = allocator;
    lib->handle = dlopen(path, RTLD_LAZY);
    if (lib->handle != NULL) {
        lib->valid = true;
    }
    return lib;
}

void re_lib_unload(re_lib_t *lib) {
    if (!lib->valid) {
        return;
    }

    dlclose(lib->handle);
    lib->handle = NULL;
    lib->valid = false;
    lib->alloc.release(lib, sizeof(re_lib_t), lib->alloc.ctx);
}

re_func_ptr_t re_lib_func(const re_lib_t *lib, const char *name) {
    if (!lib->valid) {
        return NULL;
    }

    re_func_ptr_t ptr;
    *((void **) &ptr) = dlsym(lib->handle, name);

    return ptr;
}

/*=========================*/
// Allocators
/*=========================*/

void *re_os_reserve(usize_t size) {
    void *ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return ptr;
}

void re_os_commit(void *ptr, usize_t size) {
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
    madvise(ptr, size, MADV_WILLNEED);
}

void re_os_decommit(void *ptr, usize_t size) {
    mprotect(ptr, size, PROT_NONE);
    madvise(ptr, size, MADV_DONTNEED);
}

void re_os_release(void *ptr, usize_t size) {
    munmap(ptr, size);
}
#endif // RE_OS_LINUX

void *_re_os_reserve(usize_t size, void *ctx) {
    (void) ctx;
    return re_os_reserve(size);
}

void _re_os_commit(void *ptr, usize_t size, void *ctx) {
    (void) ctx;
    re_os_commit(ptr, size);
}

void _re_os_decommit(void *ptr, usize_t size, void *ctx) {
    (void) ctx;
    re_os_decommit(ptr, size);
}

void _re_os_release(void *ptr, usize_t size, void *ctx) {
    (void) ctx;
    re_os_release(ptr, size);
}
