#include "rebound.h"

#ifdef RE_OS_LINUX
#include <dlfcn.h>
#include <sys/mman.h>
#include <pthread.h>
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

/*=========================*/
// Strings
/*=========================*/

re_str_t re_str(const char *cstr, usize_t len) {
    return (re_str_t) { len, cstr };
}

re_str_t re_str_sub(re_str_t string, usize_t start, usize_t end) {
    usize_t len = end - start + 1;
    len = re_clamp_max(len, string.len);
    return (re_str_t) {
        len,
        &string.str[start]
    };
}

re_str_t re_str_prefix(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(len, string.len);
    return (re_str_t) {
        clamped_len,
        string.str
    };
}

re_str_t re_str_suffix(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(len, string.len);
    return (re_str_t) {
        clamped_len,
        &string.str[string.len - clamped_len]
    };
}

re_str_t re_str_chop(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(len, string.len);
    return (re_str_t) {
        string.len - clamped_len,
        string.str
    };
}

re_str_t re_str_skip(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(string.len - len, string.len);
    return (re_str_t) {
        clamped_len,
        string.str + len
    };
}

//  ____  _       _    __                        _
// |  _ \| | __ _| |_ / _| ___  _ __ _ __ ___   | |    __ _ _   _  ___ _ __
// | |_) | |/ _` | __| |_ / _ \| '__| '_ ` _ \  | |   / _` | | | |/ _ \ '__|
// |  __/| | (_| | |_|  _| (_) | |  | | | | | | | |__| (_| | |_| |  __/ |
// |_|   |_|\__,_|\__|_|  \___/|_|  |_| |_| |_| |_____\__,_|\__, |\___|_|
//                                                          |___/
// Platform layer

#ifdef RE_OS_LINUX

/*=========================*/
// Platform specific structs
/*=========================*/

struct re_lib_t {
    void *handle;
    re_allocator_t alloc;
    b8_t valid;
};

struct re_mutex_t {
    re_allocator_t alloc;
    pthread_mutex_t handle;
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

/*=========================*/
// Multithreading
/*=========================*/

// Threads
re_thread_t re_thread_create(re_thread_func_t func, void *arg) {
    re_thread_t thread = {0};
	typedef void *(*_re_pthread_func_t) (void *);
    pthread_create(&thread.handle, NULL, *(_re_pthread_func_t *) &func, arg);
    return thread;
}

void re_thread_destroy(re_thread_t thread) {
    (void) thread;
}

void re_thread_wait(re_thread_t thread) {
    pthread_join(thread.handle, NULL);
}

// Mutexes
re_mutex_t *re_mutex_create(re_allocator_t alloc) {
    re_mutex_t *mutex = alloc.reserve(sizeof(re_mutex_t), alloc.ctx);
    alloc.commit(mutex, sizeof(re_mutex_t), alloc.ctx);
    mutex->alloc = alloc;
    mutex->handle = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    return mutex;
}

void re_mutex_destroy(re_mutex_t *mutex) {
    pthread_mutex_destroy(&mutex->handle);
    mutex->alloc.release(mutex, sizeof(re_mutex_t), mutex->alloc.ctx);
}

void re_mutex_lock(re_mutex_t *mutex)   { pthread_mutex_lock(&mutex->handle); }
void re_mutex_unlock(re_mutex_t *mutex) { pthread_mutex_unlock(&mutex->handle); }

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
