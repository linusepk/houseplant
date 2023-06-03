#ifndef REBOUND_H
#define REBOUND_H

#define _GNU_SOURCE 1

/*=========================*/
// Includes
/*=========================*/

//  ____                   _
// | __ )  __ _ ___  ___  | |    __ _ _   _  ___ _ __
// |  _ \ / _` / __|/ _ \ | |   / _` | | | |/ _ \ '__|
// | |_) | (_| \__ \  __/ | |__| (_| | |_| |  __/ |
// |____/ \__,_|___/\___| |_____\__,_|\__, |\___|_|
//                                    |___/
// Base layer

/*=========================*/
// Context cracking
/*=========================*/

#ifdef _WIN32
#define RE_OS_WINDOWS
#error "Windows is currently not supported"
#endif // _WIN32

#ifdef __linux__
#define RE_OS_LINUX
#endif // linux

/*=========================*/
// API macros
/*=========================*/

#if defined(RE_DYNAMIC) && defined(RE_OS_WINDOWS)
    // Using compiled windows DLL.
    #define RE_API __declspec(dllimport)
#elif defined(RE_DYNAMIC) && defined(RE_COMPILE) && defined(RE_OS_WINDOWS)
    // Compiling windows DLL.
    #define RE_API __declspec(dllexport)
#elif defined(RE_DYNAMIC) && defined(RE_COMPILE) && defined(RE_OS_LINUX)
    // Compiling linux shared object.
    #define RE_API __attribute__((visibility("default")))
#else
    #define RE_API extern
#endif

/*=========================*/
// Basic types
/*=========================*/

typedef unsigned char     u8_t;
typedef unsigned short    u16_t;
typedef unsigned int      u32_t;
typedef unsigned long int u64_t;
typedef unsigned long     usize_t;

typedef signed char     i8_t;
typedef signed short    i16_t;
typedef signed int      i32_t;
typedef signed long int i64_t;
typedef signed long     isize_t;

typedef float  f32_t;
typedef double f64_t;

typedef char b8_t;
#ifndef true
#define true 1
#endif // true
#ifndef false
#define false 0
#endif // false

typedef u8_t *ptr_t;
#ifndef NULL
#define NULL ((void *) 0)
#endif // NULL

/*=========================*/
// Allocator interface
/*=========================*/

typedef void *(*re_allocactor_reserve_func_t)(usize_t size, void *ctx);
typedef void (*re_allocator_change_memory_func_t)(void *ptr, usize_t size, void *ctx);

typedef struct re_allocator_t re_allocator_t;
struct re_allocator_t {
    re_allocactor_reserve_func_t reserve;
    re_allocator_change_memory_func_t commit;
    re_allocator_change_memory_func_t decommit;
    re_allocator_change_memory_func_t release;
    void *ctx;
};

RE_API void re_allocator_change_memory_dummy(void *ptr, usize_t size, void *ctx);

#ifndef RE_NOLIBC
RE_API void *_re_libc_reserve(usize_t size, void *ctx);
RE_API void _re_libc_release(void *ptr, usize_t size, void *ctx);

static const re_allocator_t re_libc_allocator = {
    _re_libc_reserve,
    re_allocator_change_memory_dummy,
    re_allocator_change_memory_dummy,
    _re_libc_release,
    NULL
};
#endif // RE_NOLIBC

/*=========================*/
// Utils
/*=========================*/

#define _re_concat(A, B) A##B
#define re_concat(A, B) _re_concat(A, B)
#define re_macro_var(NAME) re_concat(re_concat(UNIQUE_MACRO_ID, __LINE__), NAME)

RE_API usize_t re_fvn1a_hash(const char *key, usize_t len);
RE_API void re_memset(void *dest, u8_t value, usize_t size);

/*=========================*/
// Hash table
/*=========================*/

#define RE_HT_INIT_CAP  8
#define RE_HT_MAX_FILL  0.75f
#define RE_HT_GROW_RATE 2

typedef usize_t (*re_hash_func_t)(const void *data, usize_t size);

#define re_ht_t(K, V) struct { \
    struct {                   \
        usize_t hash;          \
        V value;               \
        b8_t alive;            \
    } *entries;                \
    usize_t count;             \
    usize_t capacity;          \
    re_allocator_t allocator;  \
    usize_t key_size;          \
    K temp_key;                \
    re_hash_func_t hash_func;  \
} *

#define _re_ht_get_entry(ENTRIES, CAP, HASH, OUT_ENTRY) do {                                          \
    usize_t re_macro_var(index) = (HASH) % (CAP);                                                     \
    for (;;) {                                                                                        \
        if (!(ENTRIES)[re_macro_var(index)].alive || (ENTRIES)[re_macro_var(index)].hash == (HASH)) { \
            (OUT_ENTRY) = &(ENTRIES)[re_macro_var(index)];                                            \
            break;                                                                                    \
        }                                                                                             \
        re_macro_var(index)= (re_macro_var(index) + 1) % (CAP);                                       \
    }                                                                                                 \
} while (0)

#define _re_ht_resize(HT) do {                                                                                              \
    usize_t re_macro_var(new_cap) = (HT)->capacity * RE_HT_GROW_RATE;                                                       \
    usize_t re_macro_var(size) = re_macro_var(new_cap) * sizeof(__typeof__(*(HT)->entries));                                \
    __typeof__((HT)->entries) re_macro_var(new_entries) = (HT)->allocator.reserve(re_macro_var(size), (HT)->allocator.ctx); \
    (HT)->allocator.commit(re_macro_var(new_entries), re_macro_var(size), (HT)->allocator.ctx);                             \
    re_memset(re_macro_var(new_entries), 0, re_macro_var(size));                                                            \
    for (usize_t i = 0; i < (HT)->capacity; i++) {                                                                          \
        __typeof__(*(HT)->entries) re_macro_var(old) = (HT)->entries[i];                                                    \
        if (!re_macro_var(old).alive) {                                                                                     \
            continue;                                                                                                       \
        }                                                                                                                   \
        __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL;                                                             \
        _re_ht_get_entry(re_macro_var(new_entries), re_macro_var(new_cap), re_macro_var(old).hash, re_macro_var(entry));    \
        *re_macro_var(entry) = re_macro_var(old);                                                                           \
    }                                                                                                                       \
    (HT)->allocator.release((HT)->entries, (HT)->capacity * sizeof(__typeof__(*(HT)->entries)), (HT)->allocator.ctx);       \
    (HT)->entries = re_macro_var(new_entries);                                                                              \
    (HT)->capacity = re_macro_var(new_cap);                                                                                 \
} while (0)

#define re_ht_create(HT, ALLOC, HASH_FUNC) do {                                                        \
    (HT) = (ALLOC).reserve(sizeof(__typeof__(*(HT))), (ALLOC).ctx);                                    \
    (ALLOC).commit((HT), sizeof(__typeof__(*(HT))), (ALLOC).ctx);                                      \
    (HT)->entries = (ALLOC).reserve(RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (ALLOC).ctx); \
    (ALLOC).commit((HT)->entries, RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (ALLOC).ctx);   \
    re_memset((HT)->entries, 0, RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)));                  \
    (HT)->count = 0;                                                                                   \
    (HT)->capacity = RE_HT_INIT_CAP;                                                                   \
    (HT)->allocator = (ALLOC);                                                                         \
    (HT)->key_size = sizeof((HT)->temp_key);                                                           \
    (HT)->hash_func = (HASH_FUNC);                                                                     \
} while (0)

#define re_ht_destroy(HT) do {                                                                                        \
    (HT)->allocator.release((HT)->entries, sizeof(__typeof__(*(HT)->entries)) * (HT)->capacity, (HT)->allocator.ctx); \
    (HT)->allocator.release((HT), sizeof(*(HT)), (HT)->allocator.ctx);                                                \
    (HT) = NULL;                                                                                                      \
} while (0)

#define re_ht_set(HT, KEY, VALUE) do {                                                        \
    __typeof__(KEY) re_macro_var(temp_key) = KEY;                                             \
    if ((HT)->count + 1 > (HT)->capacity * RE_HT_MAX_FILL) {                                  \
        _re_ht_resize(HT);                                                                    \
    }                                                                                         \
    __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL;                                   \
    usize_t re_macro_var(hash) = (HT)->hash_func(&re_macro_var(temp_key), (HT)->key_size);    \
    _re_ht_get_entry((HT)->entries, (HT)->capacity, re_macro_var(hash), re_macro_var(entry)); \
    if (!re_macro_var(entry)->alive) {                                                        \
        (HT)->count++;                                                                        \
    }                                                                                         \
    re_macro_var(entry)->hash = re_macro_var(hash);                                           \
    re_macro_var(entry)->value = (VALUE);                                                     \
    re_macro_var(entry)->alive = true;                                                        \
} while (0)

#define re_ht_get(HT, KEY, OUT) do {                                                          \
    __typeof__(KEY) re_macro_var(temp_key) = KEY;                                             \
    __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL;                                   \
    usize_t re_macro_var(hash) = (HT)->hash_func(&re_macro_var(temp_key), (HT)->key_size);    \
    _re_ht_get_entry((HT)->entries, (HT)->capacity, re_macro_var(hash), re_macro_var(entry)); \
    if (re_macro_var(entry)->alive) {                                                         \
        (OUT) = re_macro_var(entry)->value;                                                   \
    }                                                                                         \
} while (0)

#define re_ht_remove(HT, KEY) do {                                                            \
    __typeof__(KEY) re_macro_var(temp_key) = (KEY);                                           \
    __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL;                                   \
    usize_t re_macro_var(hash) = (HT)->hash_func(&re_macro_var(temp_key), (HT)->key_size);    \
    _re_ht_get_entry((HT)->entries, (HT)->capacity, re_macro_var(hash), re_macro_var(entry)); \
    if (re_macro_var(entry)->alive) {                                                         \
        (HT)->count--;                                                                        \
    }                                                                                         \
    re_macro_var(entry)->alive = false;                                                       \
} while (0)

#define re_ht_clear(HT) do {                                                                                                                                 \
    __typeof__((HT)->entries) re_macro_var(new_entries) = (HT)->allocator.reserve(RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (HT)->allocator.ctx); \
    (HT)->allocator.commit(re_macro_var(new_entries), RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (HT)->allocator.ctx);                             \
    re_memset(re_macro_var(new_entries), 0, RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)));                                                            \
    (HT)->allocator.release((HT)->entries, sizeof(__typeof__(*(HT)->entries)) * (HT)->capacity, (HT)->allocator.ctx);                                        \
    (HT)->entries = re_macro_var(new_entries);                                                                                                               \
    (HT)->capacity = RE_HT_INIT_CAP;                                                                                                                         \
    (HT)->count = 0;                                                                                                                                         \
} while (0)

//  ____  _       _    __                        _
// |  _ \| | __ _| |_ / _| ___  _ __ _ __ ___   | |    __ _ _   _  ___ _ __
// | |_) | |/ _` | __| |_ / _ \| '__| '_ ` _ \  | |   / _` | | | |/ _ \ '__|
// |  __/| | (_| | |_|  _| (_) | |  | | | | | | | |__| (_| | |_| |  __/ |
// |_|   |_|\__,_|\__|_|  \___/|_|  |_| |_| |_| |_____\__,_|\__, |\___|_|
//                                                          |___/
// Platform layer

/*=========================*/
// Dynamic library loading
/*=========================*/

typedef struct re_lib_t re_lib_t;

typedef void (*re_func_ptr_t)(void);

RE_API re_lib_t *re_lib_load(const char *path, re_allocator_t allocator);
RE_API void re_lib_unload(re_lib_t *lib);
RE_API re_func_ptr_t re_lib_func(const re_lib_t *lib, const char *name);

/*=========================*/
// Allocators
/*=========================*/

RE_API void *re_os_reserve(usize_t size);
RE_API void re_os_commit(void *ptr, usize_t size);
RE_API void re_os_decommit(void *ptr, usize_t size);
RE_API void re_os_release(void *ptr, usize_t size);

RE_API void *_re_os_reserve(usize_t size, void *ctx);
RE_API void _re_os_commit(void *ptr, usize_t size, void *ctx);
RE_API void _re_os_decommit(void *ptr, usize_t size, void *ctx);
RE_API void _re_os_release(void *ptr, usize_t size, void *ctx);

static const re_allocator_t re_os_allocator = {
    _re_os_reserve,
    _re_os_commit,
    _re_os_decommit,
    _re_os_release,
    NULL
};

#endif // REBOUND_H
