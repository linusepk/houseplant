#ifndef REBOUND_H
#define REBOUND_H

#define _GNU_SOURCE 1

/*=========================*/
// Includes
/*=========================*/

#include <string.h> // memset

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

typedef unsigned char      u8_t;
typedef unsigned short     u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;
typedef unsigned long      usize_t;

typedef signed char      i8_t;
typedef signed short     i16_t;
typedef signed int       i32_t;
typedef signed long long i64_t;
typedef signed long      isize_t;

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

RE_API void *_re_libc_reserve(usize_t size, void *ctx);
RE_API void  _re_libc_release(void *ptr, usize_t size, void *ctx);

static const re_allocator_t re_libc_allocator = {
    _re_libc_reserve,
    re_allocator_change_memory_dummy,
    re_allocator_change_memory_dummy,
    _re_libc_release,
    NULL
};

#define re_allocator_null { NULL, NULL, NULL, NULL, NULL }

RE_API void   *re_malloc(usize_t size, re_allocator_t allocator);
RE_API void   *re_calloc(usize_t n, usize_t size, re_allocator_t allocator);
RE_API void   *re_realloc(void *ptr, usize_t size, re_allocator_t allocator);
RE_API void    re_free(void *ptr, re_allocator_t allocator);
RE_API usize_t re_malloc_size(void *ptr);

/*=========================*/
// Utils
/*=========================*/

typedef usize_t (*re_hash_func_t)(const void *data, usize_t size);

#define _re_concat(A, B) A##B
#define re_concat(A, B) _re_concat(A, B)
#define re_macro_var(NAME) re_concat(re_concat(UNIQUE_MACRO_ID, __LINE__), NAME)

#define re_clamp(V, MIN, MAX) (V) > (MAX) ? (MAX) : (V) < (MIN) ? (MIN) : (V)
#define re_clamp_max(V, MAX) (V) > (MAX) ? (MAX) : (V)
#define re_clamp_min(V, MIN) (V) < (MIN) ? (MIN) : (V)
#define re_max(A, B) (A) > (B) ? (A) : (B)
#define re_min(A, B) (A) < (B) ? (A) : (B)

#define re_ptr_to_usize(PTR) ((usize_t) ((u8_t *) (PTR) - (u8_t) 0))
#define re_usize_to_ptr(N) ((void *) ((u8_t *) + N))
#define re_offsetof(S, M) re_ptr_to_usize(&((S *) 0)->M)
#define re_arr_len(ARR) (sizeof(ARR) / sizeof(ARR[0]))

#define U8_MAX    ((u8_t) ~0)
#define U16_MAX   ((u16_t) ~0)
#define U32_MAX   ((u32_t) ~0)
#define U64_MAX   ((u64_t) ~0)
#define USIZE_MAX ((usize_t) ~0)

#define I8_MIN    ((i8_t)    (0 | (i8_t) (1 << 7)))
#define I16_MIN   ((i16_t)   (0 | (i16_t) (1 << 15)))
#define I32_MIN   ((i32_t)   (0 | (i32_t) (1 << 31)))
#define I64_MIN   ((i64_t)   (0 | (i64_t) (1ll << 63)))
#define ISIZE_MIN ((isize_t) (0 | (1l << (sizeof(isize_t) * 8  -1))))

#define I8_MAX    ((i8_t)    ~I8_MIN)
#define I16_MAX   ((i16_t)   ~I16_MIN)
#define I32_MAX   ((i32_t)   ~I32_MIN)
#define I64_MAX   ((i64_t)   ~I64_MIN)
#define ISIZE_MAX ((isize_t) ~ISIZE_MIN)

#define re_bit(N) (1 << (N))
#define re_bit_set(M, N, V) \
    ((V) == 1 ? \
        ((M) |= re_bit(N)) : \
        ((M) &= ~re_bit(N)) \
    )
#define re_bit_get(M, N) \
    (M) >> (N) & 1

RE_API usize_t re_fvn1a_hash(const char *key, usize_t len);

/*=========================*/
// Hash table
/*=========================*/

#define RE_HT_INIT_CAP  8
#define RE_HT_MAX_FILL  0.75f
#define RE_HT_GROW_RATE 2

typedef void *re_ht_t;
#define re_ht_t(K, V) struct { \
    struct { \
        usize_t hash; \
        K key; \
        V value; \
        b8_t alive; \
    } *entries; \
    usize_t count; \
    usize_t capacity; \
    re_allocator_t allocator; \
    usize_t key_size; \
    K temp_key; \
    re_hash_func_t hash_func; \
} *

#define _re_ht_get_entry(ENTRIES, CAP, HASH, OUT_ENTRY) do { \
    usize_t re_macro_var(index) = (HASH) % (CAP); \
    for (;;) { \
        if (!(ENTRIES)[re_macro_var(index)].alive || (ENTRIES)[re_macro_var(index)].hash == (HASH)) { \
            (OUT_ENTRY) = &(ENTRIES)[re_macro_var(index)]; \
            break; \
        } \
        re_macro_var(index)= (re_macro_var(index) + 1) % (CAP); \
    } \
} while (0)

#define _re_ht_resize(HT) do { \
    usize_t re_macro_var(new_cap) = (HT)->capacity * RE_HT_GROW_RATE; \
    usize_t re_macro_var(size) = re_macro_var(new_cap) * sizeof(__typeof__(*(HT)->entries)); \
    __typeof__((HT)->entries) re_macro_var(new_entries) = (HT)->allocator.reserve(re_macro_var(size), (HT)->allocator.ctx); \
    (HT)->allocator.commit(re_macro_var(new_entries), re_macro_var(size), (HT)->allocator.ctx); \
    memset(re_macro_var(new_entries), 0, re_macro_var(size)); \
    for (usize_t i = 0; i < (HT)->capacity; i++) { \
        __typeof__(*(HT)->entries) re_macro_var(old) = (HT)->entries[i]; \
        if (!re_macro_var(old).alive) { \
            continue; \
        } \
        __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL; \
        _re_ht_get_entry(re_macro_var(new_entries), re_macro_var(new_cap), re_macro_var(old).hash, re_macro_var(entry)); \
        *re_macro_var(entry) = re_macro_var(old); \
    } \
    (HT)->allocator.release((HT)->entries, (HT)->capacity * sizeof(__typeof__(*(HT)->entries)), (HT)->allocator.ctx); \
    (HT)->entries = re_macro_var(new_entries); \
    (HT)->capacity = re_macro_var(new_cap); \
} while (0)

#define re_ht_create(HT, ALLOC, HASH_FUNC) do { \
    (HT) = (ALLOC).reserve(sizeof(__typeof__(*(HT))), (ALLOC).ctx); \
    (ALLOC).commit((HT), sizeof(__typeof__(*(HT))), (ALLOC).ctx); \
    (HT)->entries = (ALLOC).reserve(RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (ALLOC).ctx); \
    (ALLOC).commit((HT)->entries, RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (ALLOC).ctx); \
    memset((HT)->entries, 0, RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries))); \
    (HT)->count = 0; \
    (HT)->capacity = RE_HT_INIT_CAP; \
    (HT)->allocator = (ALLOC); \
    (HT)->key_size = sizeof((HT)->temp_key); \
    (HT)->hash_func = (HASH_FUNC); \
} while (0)

#define re_ht_destroy(HT) do { \
    (HT)->allocator.release((HT)->entries, sizeof(__typeof__(*(HT)->entries)) * (HT)->capacity, (HT)->allocator.ctx); \
    (HT)->allocator.release((HT), sizeof(*(HT)), (HT)->allocator.ctx); \
    (HT) = NULL; \
} while (0)

#define re_ht_set(HT, KEY, VALUE) do { \
    __typeof__(KEY) re_macro_var(temp_key) = KEY; \
    if ((HT)->count + 1 > (HT)->capacity * RE_HT_MAX_FILL) { \
        _re_ht_resize(HT); \
    } \
    __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL; \
    usize_t re_macro_var(hash) = (HT)->hash_func(&re_macro_var(temp_key), (HT)->key_size); \
    _re_ht_get_entry((HT)->entries, (HT)->capacity, re_macro_var(hash), re_macro_var(entry)); \
    if (!re_macro_var(entry)->alive) { \
        (HT)->count++; \
    } \
    re_macro_var(entry)->hash = re_macro_var(hash); \
    re_macro_var(entry)->key = re_macro_var(temp_key); \
    re_macro_var(entry)->value = (VALUE); \
    re_macro_var(entry)->alive = true; \
} while (0)

#define re_ht_get(HT, KEY, OUT) do { \
    __typeof__(KEY) re_macro_var(temp_key) = KEY; \
    __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL; \
    usize_t re_macro_var(hash) = (HT)->hash_func(&re_macro_var(temp_key), (HT)->key_size); \
    _re_ht_get_entry((HT)->entries, (HT)->capacity, re_macro_var(hash), re_macro_var(entry)); \
    if (re_macro_var(entry)->alive) {                                                         \
        (OUT) = re_macro_var(entry)->value; \
    } \
} while (0)

#define re_ht_remove(HT, KEY) do { \
    __typeof__(KEY) re_macro_var(temp_key) = (KEY); \
    __typeof__(*(HT)->entries) *re_macro_var(entry) = NULL; \
    usize_t re_macro_var(hash) = (HT)->hash_func(&re_macro_var(temp_key), (HT)->key_size); \
    _re_ht_get_entry((HT)->entries, (HT)->capacity, re_macro_var(hash), re_macro_var(entry)); \
    if (re_macro_var(entry)->alive) { \
        (HT)->count--; \
    } \
    re_macro_var(entry)->alive = false; \
} while (0)

#define re_ht_clear(HT) do { \
    __typeof__((HT)->entries) re_macro_var(new_entries) = (HT)->allocator.reserve(RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (HT)->allocator.ctx); \
    (HT)->allocator.commit(re_macro_var(new_entries), RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries)), (HT)->allocator.ctx); \
    re_memset(re_macro_var(new_entries), 0, RE_HT_INIT_CAP * sizeof(__typeof__(*(HT)->entries))); \
    (HT)->allocator.release((HT)->entries, sizeof(__typeof__(*(HT)->entries)) * (HT)->capacity, (HT)->allocator.ctx); \
    (HT)->entries = re_macro_var(new_entries); \
    (HT)->capacity = RE_HT_INIT_CAP; \
    (HT)->count = 0; \
} while (0)

#define re_ht_count(HT) (HT)->count

typedef usize_t re_ht_iter_t;

RE_API re_ht_iter_t __re_ht_iter_next(usize_t start, void *entries, usize_t entry_count, usize_t alive_offset, usize_t stride);
#define _re_ht_iter_next(HT, START) \
    __re_ht_iter_next( \
        (START), \
        (HT)->entries, \
        (HT)->capacity, \
        re_offsetof(__typeof__(*(HT)->entries), alive), \
        sizeof(__typeof__(*(HT)->entries)) \
    )

#define re_ht_iter_new(HT) _re_ht_iter_next((HT), 0)
#define re_ht_iter_valid(HT, ITER) (ITER) < (HT)->capacity
#define re_ht_iter_advance(HT, ITER) (ITER) = _re_ht_iter_next((HT), (ITER) + 1)

#define re_ht_get_iter(HT, ITER, KEY, VALUE) do { \
    (KEY) = (HT)->entries[(ITER)].key; \
    (VALUE) = (HT)->entries[(ITER)].value; \
} while (0)

/*=========================*/
// Strings
/*=========================*/

typedef struct re_str_t re_str_t;
struct re_str_t {
    usize_t len;
    const char *str;
};

#define re_str_null { 0, NULL }
#define re_str_lit(str) re_str(str, sizeof(str) - 1)
RE_API re_str_t re_str(const char *cstr, usize_t len);
RE_API re_str_t re_str_sub(re_str_t string, usize_t start, usize_t end);
RE_API re_str_t re_str_prefix(re_str_t string, usize_t len);
RE_API re_str_t re_str_suffix(re_str_t string, usize_t len);
RE_API re_str_t re_str_chop(re_str_t string, usize_t len);
RE_API re_str_t re_str_skip(re_str_t string, usize_t len);
RE_API i32_t    re_str_cmp(re_str_t a, re_str_t b);

/*=========================*/
// Linked lists
/*=========================*/

// Singly linked list stack (slls)
#define re_slls_push_n(STACK, NODE, NEXT) ( \
    (NODE)->NEXT = NULL, \
    (STACK) == NULL ? \
        ((STACK) = NODE) : \
        ((NODE)->NEXT = (STACK), (STACK) = (NODE)) \
)
#define re_slls_pop_n(STACK, NODE) ( \
    (STACK) != NULL ? \
        (STACK) = (STACK)->NODE : \
        0 \
)
#define re_slls_push(STACK, NODE) re_slls_push_n(STACK, NODE, next)
#define re_slls_pop(STACK) re_slls_pop_n(STACK, next)

// Singly linked list queue (sllq)
#define re_sllq_push_front_n(FIRST, LAST, NODE, NEXT) ( \
    (FIRST) == NULL ? \
        ((FIRST) = (LAST) = (NODE), (NODE)->NEXT = NULL) : \
        ((NODE)->NEXT = (FIRST), (FIRST) = (NODE)) \
)
#define re_sllq_push_back_n(FIRST, LAST, NODE, NEXT) ( \
    (NODE)->NEXT = NULL, \
    (FIRST) == NULL ? \
        (FIRST) = (LAST) = (NODE) : \
        ((LAST)->NEXT = (NODE), (LAST) = (NODE)) \
)
#define re_sllq_pop_n(FIRST, LAST, NEXT) ( \
    (FIRST) == (LAST) ? \
        (FIRST) = (LAST) = NULL : \
        ((FIRST) = (FIRST)->NEXT) \
)
#define re_sllq_push_front(FIRST, LAST, NODE) \
    re_sllq_push_front_n(FIRST, LAST, NODE, next)
#define re_sllq_push_back(FIRST, LAST, NODE) \
    re_sllq_push_back_n(FIRST, LAST, NODE, next)
#define re_sllq_pop(FIRST, LAST) \
    re_sllq_pop_n(FIRST, LAST, next)

// Doubly linked list (dll)
#define re_dll_push_back_np(FIRST, LAST, NODE, NEXT, PREV) ( \
    (NODE)->NEXT = NULL, \
    (FIRST) == NULL ? \
        ((FIRST) = (LAST) = (NODE), (NODE)->PREV = NULL) : \
        ((LAST)->NEXT = (NODE), ((NODE)->PREV = (LAST), (LAST) = (NODE))) \
)
#define re_dll_push_front_np(FIRST, LAST, NODE, NEXT, PREV) ( \
    (NODE)->PREV = NULL, \
    (FIRST) == NULL ? \
        ((FIRST) = (LAST) = (NODE), (NODE)->NEXT = NULL) : \
        ((FIRST)->PREV = (NODE), ((NODE)->NEXT = (FIRST), (FIRST) = (NODE))) \
)
#define re_dll_remove_np(FIRST, LAST, NODE, NEXT, PREV) ( \
    (NODE) == (FIRST) ? \
        ((FIRST) = (FIRST)->NEXT, (FIRST)->PREV = NULL) : \
        ((NODE) == (LAST) ? \
            ((LAST) = (LAST)->PREV, (LAST)->NEXT = NULL) : \
            ((NODE)->PREV->NEXT = (NODE)->NEXT, (NODE)->NEXT->PREV = (NODE)->PREV)) \
)
#define re_dll_push_back(FIRST, LAST, NODE) \
    re_dll_push_back_np(FIRST, LAST, NODE, next, prev)
#define re_dll_push_front(FIRST, LAST, NODE) \
    re_dll_push_front_np(FIRST, LAST, NODE, next, prev)
#define re_dll_remove(FIRST, LAST, NODE) \
    re_dll_remove_np(FIRST, LAST, NODE, next, prev)

/*=========================*/
// Dynamic array
/*=========================*/

#define re_da_t(T) T *

// Lifetime operators
#define re_da_create(DA, ALLOCATOR) _re_da_create((void **) &(DA), sizeof(*(DA)), (ALLOCATOR))
#define re_da_destroy(DA) _re_da_destroy((void **) &(DA))

// Ordered operators
#define re_da_insert(DA, VALUE, INDEX) do { \
    __typeof__(VALUE) re_macro_var(temp_value) = (VALUE); \
    _re_da_insert_arr((void **) &(DA), &re_macro_var(temp_value), 1, (INDEX)); \
} while (0)
#define re_da_remove(DA, INDEX, OUT) _re_da_remove_arr((void **) &(DA), 1, (INDEX), OUT)

// Fast operators
#define re_da_insert_fast(DA, VALUE, INDEX) do { \
    __typeof__(VALUE) re_macro_var(temp_value) = (VALUE); \
    _re_da_insert_fast((void **) &(DA), &re_macro_var(temp_value), (INDEX)); \
} while (0)
#define re_da_remove_fast(DA, INDEX, OUT) _re_da_remove_fast((void **) &(DA), (INDEX), OUT)

// Stack operators
#define re_da_push(DA, VALUE) re_da_insert_fast(DA, VALUE, re_da_count(DA));
#define re_da_pop(DA, OUT) re_da_remove_fast(DA, re_da_count(DA) - 1, OUT)

// Array operators
#define re_da_insert_arr(DA, ARR, COUNT, INDEX) _re_da_insert_arr((void **) &(DA), (ARR), (COUNT), (INDEX))
#define re_da_remove_arr(DA, COUNT, INDEX, OUT) _re_da_remove_arr((void **) &(DA), (COUNT), (INDEX), (OUT))

// Array stack operators
#define re_da_push_arr(DA, ARR, COUNT) _re_da_insert_arr((void **) &(DA), (ARR), (COUNT), re_da_count(DA));
#define re_da_pop_arr(DA, COUNT, OUT) _re_da_remove_arr((void **) &(DA), (COUNT), re_da_count(DA) - (COUNT), (OUT));

// Helpers
#define re_da_count(DA) _re_da_to_head(DA)->count
#define re_da_last(DA) ((DA)[re_da_count - 1])
#define re_da_iter(DA, I) for (usize_t I = 0; I < re_da_count(DA); I++)

// Private API
#define _RE_DA_INIT_CAP 8

typedef struct _re_da_header_t _re_da_header_t;
struct _re_da_header_t {
    re_allocator_t alloc;
    usize_t size;
    usize_t count;
    usize_t cap;
};

#define _re_da_to_head(DA) ((_re_da_header_t *) ((ptr_t) (DA) - sizeof(_re_da_header_t)))
#define _re_head_to_da(HEAD) ((void *) ((ptr_t) (HEAD) + sizeof(_re_da_header_t)))

RE_API void _re_da_resize(void **da, usize_t count);
RE_API void _re_da_create(void **da, usize_t size, re_allocator_t allocator);
RE_API void _re_da_destroy(void **da);
RE_API void _re_da_insert_fast(void **da, const void *value, usize_t index);
RE_API void _re_da_remove_fast(void **da, usize_t index, void *output);
RE_API void _re_da_insert_arr(void **da, const void *arr, usize_t count, usize_t index);
RE_API void _re_da_remove_arr(void **da, usize_t count, usize_t index, void *output);

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

RE_API re_lib_t     *re_lib_load(const char *path, re_allocator_t allocator);
RE_API void          re_lib_unload(re_lib_t *lib);
RE_API re_func_ptr_t re_lib_func(const re_lib_t *lib, const char *name);

/*=========================*/
// Allocators
/*=========================*/

RE_API void *re_os_reserve(usize_t size);
RE_API void  re_os_commit(void *ptr, usize_t size);
RE_API void  re_os_decommit(void *ptr, usize_t size);
RE_API void  re_os_release(void *ptr, usize_t size);

RE_API void *_re_os_reserve(usize_t size, void *ctx);
RE_API void  _re_os_commit(void *ptr, usize_t size, void *ctx);
RE_API void  _re_os_decommit(void *ptr, usize_t size, void *ctx);
RE_API void  _re_os_release(void *ptr, usize_t size, void *ctx);

static const re_allocator_t re_os_allocator = {
    _re_os_reserve,
    _re_os_commit,
    _re_os_decommit,
    _re_os_release,
    NULL
};

/*=========================*/
// Multithreading
/*=========================*/

// Threads
typedef struct re_thread_t re_thread_t;
struct re_thread_t {
    usize_t handle;
};

typedef void (*re_thread_func_t)(void *arg);

RE_API void       *_re_thread_func(void *arg);
RE_API re_thread_t re_thread_create(re_thread_func_t func, void *arg);
RE_API void        re_thread_destroy(re_thread_t thread);
RE_API void        re_thread_wait(re_thread_t thread);

// Mutexes
typedef struct re_mutex_t re_mutex_t;

RE_API re_mutex_t *re_mutex_create(re_allocator_t alloc);
RE_API void        re_mutex_destroy(re_mutex_t *mutex);
RE_API void        re_mutex_lock(re_mutex_t *mutex);
RE_API void        re_mutex_unlock(re_mutex_t *mutex);

#endif // REBOUND_H
