#ifndef REBOUND_H
#define REBOUND_H

/*=========================*/
// Includes
/*=========================*/

#define _GNU_SOURCE

#include <string.h> // memset
#include <stdio.h>
#include <stdarg.h>

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
#endif

#ifdef __linux__
#define RE_OS_LINUX
#endif

#ifdef __clang__
#define RE_COMPILER_CLANG 1
#endif

#ifdef __GNUC__
#define RE_COMPILER_GCC 1
#endif

#ifdef _MSC_VER
#define RE_COMPILER_MSVC 1
#endif

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

#ifdef RE_OS_LINUX
#define RE_FORMAT_FUNCTION(FORMAT_INDEX, VA_INDEX) __attribute__((format(printf, FORMAT_INDEX, VA_INDEX)))
#else
#define RE_FORMAT_FUNCTION(FORMAT_INDEX, VA_INDEX)
#endif

#if defined(RE_COMPILER_GCC) || defined(RE_COMPILER_CLANG)
#define RE_THREAD_LOCAL __thread
#elif defined(RE_COMPILER_MSVC)
#define RE_THREAD_LOCAL __declspec(thread)
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

typedef u8_t  b8_t;
typedef u32_t b32_t;

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
// Initialization
/*=========================*/

RE_API void re_init(void);
RE_API void re_terminate(void);

/*=========================*/
// Allocators
/*=========================*/

#if defined(RE_MALLOC) && defined(RE_REALLOC) && defined(RE_FREE)
#elif !defined(RE_MALLOC) && !defined(RE_REALLOC) && !defined(RE_FREE)
#else
#error "Must define all or none of RE_MALLOC, RE_REALLOC and RE_FREE."
#endif

#ifndef RE_MALLOC
#include <stdlib.h>
#define RE_MALLOC malloc
#define RE_REALLOC realloc
#define RE_FREE free
#endif

#define OUT_OF_MEMORY "Out of memory"

RE_API void *re_malloc(usize_t size);
RE_API void *re_realloc(void *ptr, usize_t size);
RE_API void  re_free(void *ptr);

/*=========================*/
// Arena
/*=========================*/

typedef struct re_arena_t re_arena_t;

RE_API re_arena_t *re_arena_create(u64_t capacity);
RE_API void re_arena_destroy(re_arena_t **arena);

RE_API void *re_arena_push(re_arena_t *arena, u64_t size);
RE_API void *re_arena_push_zero(re_arena_t *arena, u64_t size);

RE_API void re_arena_pop(re_arena_t *arena, u64_t size);
RE_API void re_arena_clear(re_arena_t *arena);

RE_API u64_t re_arena_get_pos(re_arena_t *arena);
RE_API void *re_arena_get_index(u64_t index, re_arena_t *arena);

// Temp
typedef struct re_arena_temp_t re_arena_temp_t;
struct re_arena_temp_t {
    re_arena_t *arena;
    u64_t position;
};

RE_API re_arena_temp_t re_arena_temp_start(re_arena_t *arena);
RE_API void re_arena_temp_end(re_arena_temp_t *arena);

// Scratch
RE_API re_arena_temp_t re_arena_scratch_get(re_arena_t **conflicts, u32_t conflict_count);
#define re_arena_scratch_release(scratch) re_arena_temp_end(scratch)

RE_API void _re_arena_scratch_destroy(void);

/*=========================*/
// Utils
/*=========================*/

// Aborts the program and logs the reason.
// Both format string and format arguments should be supplied in the variatic arguments.
#define RE_ABORT(...) do { \
    char buffer[1024]; \
    re_format_string(buffer, __VA_ARGS__); \
    re_log_fatal("ABORT: %s", buffer); \
    abort(); \
} while (0)

// Ensures a condition is met. If it isn't met, program will abort.
#define RE_ENSURE(cond, ...) do { \
    if (!(cond)) { \
        re_log_error("Condition '%s' not met.", #cond); \
        RE_ABORT(__VA_ARGS__); \
    } \
} while (0)

// Ensures a condition is met while debugging. Does nothing if not in debug mode.
#ifdef RE_DEBUG
#define RE_ASSERT(cond, ...) RE_ENSURE(cond, __VA_ARGS__)
#else
#define RE_ASSERT(cond, ...)
#endif

typedef u64_t (*re_hash_func_t)(const void *data, u64_t size);
typedef b8_t (*re_equal_func_t)(const void *a, const void *b, u32_t size);

// Concatinates A and B into an identifier.
#define re_concat(A, B) _re_concat(A, B)
// Creates a unique variable name within a macro to avoid name collisions.
#define re_macro_var(NAME) re_concat(re_concat(UNIQUE_MACRO_ID, __LINE__), NAME)
// Second concatination function needed for macro expansion.
#define _re_concat(A, B) A##B

// Clamps V between MIN and MAX.
#define re_clamp(V, MIN, MAX) (V) > (MAX) ? (MAX) : (V) < (MIN) ? (MIN) : (V)
// Clamps V maximum value to MAX.
#define re_clamp_max(V, MAX) (V) > (MAX) ? (MAX) : (V)
// Clamps V minimum value to MIN.
#define re_clamp_min(V, MIN) (V) < (MIN) ? (MIN) : (V)
// Returns the biggest value of A and B.
#define re_max(A, B) (A) > (B) ? (A) : (B)
// Returns the smallest value of A and B.
#define re_min(A, B) (A) < (B) ? (A) : (B)

// Converts a pointer to an integer.
#define re_ptr_to_usize(PTR) ((usize_t) ((u8_t *) (PTR) - (u8_t) 0))
// Converts an integer to a pointer.
#define re_usize_to_ptr(N) ((void *) ((u8_t *) + N))
// Calculates the byte offset of the member M in struct S.
#define re_offsetof(S, M) re_ptr_to_usize(&((S *) 0)->M)
// Calculates the length of ARR.
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

#define KB(n) (n << 10)
#define MB(n) (n << 20)
#define GB(n) ((u64_t) n << 30)
#define TB(n) ((u64_t) n << 40)

#define re_bit(N) (1 << (N))
#define re_bit_set(M, N, V) \
    ((V) == 1 ? \
        ((M) |= re_bit(N)) : \
        ((M) &= ~re_bit(N)) \
    )
#define re_bit_get(M, N) \
    (M) >> (N) & 1

// Hashes data using the fvn1a algorithm.
RE_API u64_t re_fvn1a_hash(const void *data, u64_t size);
// Formats the fmt string into the provided buffer.
RE_API void re_format_string(char buffer[1024], const char *fmt, ...) RE_FORMAT_FUNCTION(2, 3);

/*=========================*/
// Strings
/*=========================*/

typedef struct re_str_t re_str_t;
struct re_str_t {
    u8_t *str;
    usize_t len;
};

#define re_str_null ((re_str_t) {NULL, 0})
#define re_str_lit(str) re_str((u8_t *) (str), sizeof(str) - 1)
#define re_str_cstr(str) re_str((u8_t *) (str), strlen(str))
RE_API re_str_t re_str(u8_t *cstr, usize_t len);
RE_API re_str_t re_str_sub(re_str_t string, usize_t start, usize_t end);
RE_API re_str_t re_str_prefix(re_str_t string, usize_t len);
RE_API re_str_t re_str_suffix(re_str_t string, usize_t len);
RE_API re_str_t re_str_chop(re_str_t string, usize_t len);
RE_API re_str_t re_str_skip(re_str_t string, usize_t len);
RE_API i32_t    re_str_cmp(re_str_t a, re_str_t b);
RE_API re_str_t re_str_pushf(const char *fmt, va_list args, re_arena_t *arena);
RE_API re_str_t re_str_push_copy(re_str_t str, re_arena_t *arena);

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

#define re_dyn_arr_t(T) T *

#define re_dyn_arr_new(ARR, SIZE) \
    _re_dyn_arr_new_impl((void **) &(ARR), (SIZE))

#define re_dyn_arr_free(ARR) \
    _re_dyn_arr_free_impl((void **) &(ARR))

u32_t re_dyn_arr_count(void *arr);

#define re_dyn_arr_last(ARR) ((ARR)[re_dyn_arr_count(ARR) - 1])

// Iterations
#define re_dyn_arr_push(ARR, VALUE) \
    re_dyn_arr_insert_fast((ARR), (VALUE), re_dyn_arr_count(ARR))

#define re_dyn_arr_insert(ARR, VALUE, INDEX) ({ \
        re_dyn_arr_new((ARR), sizeof(*(ARR))); \
        __typeof__(VALUE) temp_value = (VALUE); \
        _re_dyn_arr_insert_arr_impl((void **) &(ARR), &temp_value, 1, (INDEX)); \
    })

#define re_dyn_arr_insert_fast(ARR, VALUE, INDEX) ({ \
        re_dyn_arr_new((ARR), sizeof(*(ARR))); \
        __typeof__(VALUE) temp_value = (VALUE); \
        _re_dyn_arr_insert_fast_impl((void **) &(ARR), &temp_value, (INDEX)); \
    })

#define re_dyn_arr_push_arr(ARR, VALUE_ARR, COUNT) ({ \
        re_dyn_arr_new((ARR), sizeof(*(ARR))); \
        _re_dyn_arr_insert_arr_impl((void **) &(ARR), (VALUE_ARR), (COUNT), re_dyn_arr_count(ARR)); \
    })


#define re_dyn_arr_insert_arr(ARR, VALUE_ARR, COUNT, INDEX) ({ \
        re_dyn_arr_new((ARR), sizeof(*(ARR))); \
        _re_dyn_arr_insert_arr_impl((void **) &(ARR), (VALUE_ARR), (COUNT), (INDEX)); \
    })

#define re_dyn_arr_reserve(ARR, COUNT) \
    re_dyn_arr_push_arr((ARR), NULL, (COUNT))

// Removal
#define re_dyn_arr_pop(ARR) \
    re_dyn_arr_remove_fast((ARR), re_dyn_arr_count(ARR) - 1)

#define re_dyn_arr_remove(ARR, INDEX) ({ \
        __typeof__(*(ARR)) result; \
        _re_dyn_arr_remove_arr_impl((void **) &(ARR), (INDEX), 1, &result); \
        result; \
    })

#define re_dyn_arr_remove_fast(ARR, INDEX) ({ \
        __typeof__(*(ARR)) result; \
        _re_dyn_arr_remove_fast_impl((void **) &(ARR), (INDEX), &result); \
        result; \
    })

#define re_dyn_arr_pop_arr(ARR, COUNT, OUT) \
    _re_dyn_arr_remove_arr_impl((void **) &(ARR), (COUNT), re_dyn_arr_count(ARR) - (COUNT), (OUT))

#define re_dyn_arr_remove_arr(ARR, COUNT, INDEX, OUT) \
    _re_dyn_arr_remove_arr_impl((void **) &(ARR), (COUNT), (INDEX), (OUT))


// Private API
RE_API void _re_dyn_arr_new_impl(void **arr, u32_t size);
RE_API void _re_dyn_arr_free_impl(void **arr);
RE_API void _re_dyn_arr_insert_fast_impl(void **arr, const void *value, u32_t index);
RE_API void _re_dyn_arr_insert_arr_impl(void **arr, const void *value_arr, u32_t count, u32_t index);
RE_API void _re_dyn_arr_remove_fast_impl(void **arr, u32_t index, void *result);
RE_API void _re_dyn_arr_remove_arr_impl(void **arr, u32_t count, u32_t index, void *out);

/*=========================*/
// Hash map
/*=========================*/

#define re_hash_map_t(KEY, VALUE) struct { \
    struct { \
        KEY key; \
        VALUE value; \
        u64_t hash; \
        bucket_state_t state; \
    } *buckets; \
    u32_t count; \
    u32_t tombstone_count; \
    void *null_value; \
    void *null_key; \
    re_hash_func_t hash_func; \
    re_equal_func_t equal_func; \
} *

#define re_hash_map_init(MAP, NULL_KEY, NULL_VALUE, HASH_FUNC, EQUAL_FUNC) ({ \
        if ((MAP) == NULL) { \
            (MAP) = re_malloc(sizeof(*(MAP))); \
            *(MAP) = (__typeof__(*(MAP))) {0}; \
            re_dyn_arr_reserve((MAP)->buckets, 8); \
            (MAP)->count = 0; \
            \
            __typeof__(NULL_KEY) temp_null_key = (NULL_KEY); \
            (MAP)->null_key = re_malloc(sizeof(__typeof__((MAP)->buckets->key))); \
            memcpy((MAP)->null_key, &temp_null_key, sizeof(__typeof__((MAP)->buckets->key))); \
            \
            __typeof__(NULL_VALUE) temp_null_value = (NULL_VALUE); \
            (MAP)->null_value = re_malloc(sizeof(__typeof__((MAP)->buckets->value))); \
            memcpy((MAP)->null_value, &temp_null_value, sizeof(__typeof__((MAP)->buckets->value))); \
            \
            (MAP)->hash_func = (HASH_FUNC); \
            (MAP)->equal_func = (EQUAL_FUNC); \
        } \
    })

#define re_hash_map_init_default(MAP) \
    re_hash_map_init((MAP), ((__typeof__((MAP)->buckets->key)) {0}), ((__typeof__((MAP)->buckets->value)) {0}), (re_hash_func_t) (void *) re_fvn1a_hash, _re_hash_map_default_equal_func)

#define re_hash_map_free(MAP) ({ \
        re_dyn_arr_free((MAP)->buckets); \
        re_free((MAP)->null_key); \
        re_free((MAP)->null_value); \
        re_free(MAP); \
        (MAP) = NULL; \
    })

#define re_hash_map_count(MAP) ((MAP)->count)

#define re_hash_map_set(MAP, KEY, VALUE) ({ \
        re_hash_map_init_default(MAP); \
        /* Resize if needed. */ \
        if ((MAP)->count >= re_dyn_arr_count((MAP)->buckets) * _RE_HASH_MAP_MAX_LOAD) { \
            __typeof__((MAP)->buckets) new_buckets = NULL; \
            re_dyn_arr_reserve(new_buckets, re_dyn_arr_count((MAP)->buckets) * _RE_HASH_MAP_GROW_FACTOR); \
            for (u32_t i = 0; i < re_dyn_arr_count((MAP)->buckets); i++) { \
                if ((MAP)->buckets[i].state == BUCKET_STATE_IN_USE) { \
                    b8_t _temp; (void) _temp; \
                    u32_t index = _re_hash_map_find_bucket(new_buckets, &(MAP)->buckets[i].key, (MAP)->buckets[i].hash, (MAP)->equal_func, _temp); \
                    new_buckets[index] = (MAP)->buckets[i]; \
                } \
            } \
            re_dyn_arr_free((MAP)->buckets); \
            (MAP)->buckets = new_buckets; \
        } \
        \
        __typeof__((MAP)->buckets->key) temp_key = (KEY); \
        u64_t hash = (MAP)->hash_func(&temp_key, sizeof((MAP)->buckets->key)); \
        b8_t new_entry; \
        u32_t index = _re_hash_map_find_bucket((MAP)->buckets, &temp_key, hash, (MAP)->equal_func, new_entry); \
        (MAP)->buckets[index].key = temp_key; \
        (MAP)->buckets[index].value = (VALUE); \
        (MAP)->buckets[index].hash = hash; \
        (MAP)->buckets[index].state = BUCKET_STATE_IN_USE; \
        if (new_entry) { \
            (MAP)->count++; \
        } \
    })

#define re_hash_map_get(MAP, KEY) ({ \
        re_hash_map_init_default(MAP); \
        __typeof__((MAP)->buckets->value) result; \
        __typeof__((MAP)->buckets->key) temp_key = (KEY); \
        u64_t hash = (MAP)->hash_func(&temp_key, sizeof(temp_key)); \
        b8_t temp; (void) temp; \
        i32_t index = _re_hash_map_find_bucket((MAP)->buckets, &temp_key, hash, (MAP)->equal_func, temp); \
        if ((MAP)->buckets[index].state == BUCKET_STATE_IN_USE) { \
            result = (MAP)->buckets[index].value; \
        } else { \
            memcpy(&result, (MAP)->null_value, sizeof(__typeof__((MAP)->buckets->value))); \
        } \
        result; \
    })


#define re_hash_map_get_index_key(MAP, INDEX) ({ \
        re_hash_map_init_default(MAP); \
        __typeof__((MAP)->buckets->key) result; \
        if ((MAP)->buckets[(INDEX)].state == BUCKET_STATE_IN_USE) { \
            result = (MAP)->buckets[(INDEX)].key; \
        } else { \
            memcpy(&result, (MAP)->null_key, sizeof(__typeof__((MAP)->buckets->key))); \
        } \
        result; \
    })

#define re_hash_map_get_index_value(MAP, INDEX) ({ \
        re_hash_map_init_default(MAP); \
        __typeof__((MAP)->buckets->value) result; \
        if ((MAP)->buckets[(INDEX)].state == BUCKET_STATE_IN_USE) { \
            result = (MAP)->buckets[(INDEX)].value; \
        } else { \
            memcpy(&result, (MAP)->null_value, sizeof(__typeof__((MAP)->buckets->value))); \
        } \
        result; \
    })

#define re_hash_map_has(MAP, KEY) ({ \
        re_hash_map_init_default(MAP); \
        __typeof__((MAP)->buckets->key) temp_key = (KEY); \
        u64_t hash = (MAP)->hash_func(&temp_key, sizeof(temp_key)); \
        b8_t temp; (void) temp; \
        i32_t index = _re_hash_map_find_bucket((MAP)->buckets, &temp_key, hash, (MAP)->equal_func, temp); \
        (MAP)->buckets[index].state == BUCKET_STATE_IN_USE; \
    })

#define re_hash_map_remove(MAP, KEY) ({ \
        re_hash_map_init_default(MAP); \
        __typeof__((MAP)->buckets->value) result; \
        __typeof__((MAP)->buckets->key) temp_key = (KEY); \
        u64_t hash = (MAP)->hash_func(&temp_key, sizeof(temp_key)); \
        b8_t temp; (void) temp; \
        i32_t index = _re_hash_map_find_bucket((MAP)->buckets, &temp_key, hash, (MAP)->equal_func, temp); \
        if ((MAP)->buckets[index].state == BUCKET_STATE_IN_USE) { \
            result = (MAP)->buckets[index].value; \
            (MAP)->buckets[index].state = BUCKET_STATE_TOMBSTONE; \
            (MAP)->count--; \
            (MAP)->tombstone_count++; \
        } else { \
            memcpy(&result, (MAP)->null_value, sizeof((MAP)->buckets->value)); \
        } \
        /* Redo bucket array to get rid of tombstones. */ \
        if ((MAP)->tombstone_count >= re_dyn_arr_count((MAP)->buckets) * _RE_HASH_MAP_MAX_TOMBSTONE_LOAD) { \
            __typeof__((MAP)->buckets) new_buckets = NULL; \
            re_dyn_arr_reserve(new_buckets, re_dyn_arr_count((MAP)->buckets)); \
            for (u32_t i = 0; i < re_dyn_arr_count((MAP)->buckets); i++) { \
                if ((MAP)->buckets[i].state == BUCKET_STATE_IN_USE) { \
                    b8_t _temp; (void) _temp; \
                    u32_t index = _re_hash_map_find_bucket(new_buckets, &(MAP)->buckets[i].key, (MAP)->buckets[i].hash, (MAP)->equal_func, _temp); \
                    new_buckets[index] = (MAP)->buckets[i]; \
                } \
            } \
            re_dyn_arr_free((MAP)->buckets); \
            (MAP)->buckets = new_buckets; \
            (MAP)->tombstone_count = 0; \
        } \
        result; \
    })

#define _re_hash_map_find_bucket(BUCKETS, KEY, HASH, EQUAL_FUNC, NEW_ENTRY) ({ \
        u32_t index = (HASH) % re_dyn_arr_count((BUCKETS)); \
        (NEW_ENTRY) = true; \
        while (true) { \
            if ((BUCKETS)[index].state == BUCKET_STATE_INACTIVE) { \
                break; \
            } else if ((BUCKETS)[index].hash == (HASH) && \
                       (EQUAL_FUNC)((KEY), &(BUCKETS)[index].key, sizeof((BUCKETS)->key))) { \
                (NEW_ENTRY) = false; \
                break; \
           } \
           index = (index + 1) % re_dyn_arr_count((BUCKETS)); \
        } \
        index; \
    })

// Iteration
typedef u32_t re_hash_map_iter_t;

#define re_hash_map_iter_get(MAP) \
    re_hash_map_iter_next(MAP, -1) \

#define re_hash_map_iter_valid(ITER) (ITER != U32_MAX)

#define re_hash_map_iter_next(MAP, ITER) ({ \
        re_hash_map_init_default(MAP); \
        u32_t result = U32_MAX; \
        for (u32_t i = (ITER) + 1; i < re_dyn_arr_count((MAP)->buckets); i++) { \
            if ((MAP)->buckets[i].state == BUCKET_STATE_IN_USE) { \
                result = i; \
                break; \
            } \
        } \
        result; \
    })


#define _RE_HASH_MAP_MAX_LOAD 0.75f
#define _RE_HASH_MAP_GROW_FACTOR 2
#define _RE_HASH_MAP_MAX_TOMBSTONE_LOAD 0.25f

typedef enum {
    BUCKET_STATE_INACTIVE,
    BUCKET_STATE_IN_USE,
    BUCKET_STATE_TOMBSTONE
} bucket_state_t;

RE_API b8_t _re_hash_map_default_equal_func(const void *a, const void *b, u32_t size);

/*=========================*/
// Logger
/*=========================*/

#ifndef RE_LOG_MESSAGE_MAX_LENGTH
#define RE_LOG_MESSAGE_MAX_LENGTH 1024
#endif

#ifndef RE_LOGGER_CALLBACK_MAX
#define RE_LOGGER_CALLBACK_MAX 32
#endif

typedef enum {
    RE_LOG_LEVEL_FATAL,
    RE_LOG_LEVEL_ERROR,
    RE_LOG_LEVEL_WARN,
    RE_LOG_LEVEL_INFO,
    RE_LOG_LEVEL_DEBUG,
    RE_LOG_LEVEL_TRACE,

    RE_LOG_LEVEL_COUNT
} re_log_level_t;

typedef struct re_log_event_t re_log_event_t;
struct re_log_event_t {
    char message[RE_LOG_MESSAGE_MAX_LENGTH];
    u32_t message_length;
    const char *file;
    i32_t line;
    re_log_level_t level;
    void *user_data;
    struct {
        u8_t hour;
        u8_t min;
        u8_t sec;
    } time;
};

typedef void (*re_log_callback_t)(re_log_event_t *const event);

// Adds a callback to be called every time a logging function happens.
// Callback will only be called if log level is equal or more urgent than 'level'.
// 'user_data' will be passed to the callback via the log event.
RE_API void re_logger_add_callback(
        re_log_callback_t callback,
        re_log_level_t level,
        void *user_data);
// Adds an output file to write logs to.
// Only logs with 'level' or higher urgency will be written.
RE_API void re_logger_add_fp(FILE *fp, re_log_level_t level);
// Silences the logger from outputting to standard output and standard error.
// Callbacks will still be called.
RE_API void re_logger_set_silent(b8_t silent);
// Set lowest level of logging urgency for standard output and standard error.
// Silencing will override this option.
// Default level is RE_LOG_LEVEL_TRACE.
RE_API void re_logger_set_level(re_log_level_t level);

// Log a fatal error.
#define re_log_fatal(...) \
    _re_log(__FILE__, __LINE__, RE_LOG_LEVEL_FATAL, __VA_ARGS__)
// Log an error.
#define re_log_error(...) \
    _re_log(__FILE__, __LINE__, RE_LOG_LEVEL_ERROR, __VA_ARGS__)
// Log a warning.
#define re_log_warn(...) \
    _re_log(__FILE__, __LINE__, RE_LOG_LEVEL_WARN, __VA_ARGS__)
// Log some information.
#define re_log_info(...) \
    _re_log(__FILE__, __LINE__, RE_LOG_LEVEL_INFO, __VA_ARGS__)
// Log some debug information.
#define re_log_debug(...) \
    _re_log(__FILE__, __LINE__, RE_LOG_LEVEL_DEBUG, __VA_ARGS__)
// Log some trace information.
#define re_log_trace(...) \
    _re_log(__FILE__, __LINE__, RE_LOG_LEVEL_TRACE, __VA_ARGS__)

// Private API
RE_API void _re_log(
        const char *file,
        i32_t line,
        re_log_level_t level,
        const char *fmt,
        ...) RE_FORMAT_FUNCTION(4, 5);

/*=========================*/
// Math
/*=========================*/

#define RAD(DEGS) (DEGS * 0.0174532925)
#define DEG(RADS) (RADS * 57.2957795)

// 2D vector.
typedef union re_vec2_t re_vec2_t;
union re_vec2_t {
    struct { f32_t x, y; };
    struct { f32_t u, v; };
    f32_t f[2];
};

RE_API re_vec2_t re_vec2(f32_t x, f32_t y);
RE_API re_vec2_t re_vec2s(f32_t scaler);

RE_API re_vec2_t re_vec2_mul(re_vec2_t a, re_vec2_t b);
RE_API re_vec2_t re_vec2_div(re_vec2_t a, re_vec2_t b);
RE_API re_vec2_t re_vec2_add(re_vec2_t a, re_vec2_t b);
RE_API re_vec2_t re_vec2_sub(re_vec2_t a, re_vec2_t b);

RE_API re_vec2_t re_vec2_muls(re_vec2_t vec, f32_t scaler);
RE_API re_vec2_t re_vec2_divs(re_vec2_t vec, f32_t scaler);
RE_API re_vec2_t re_vec2_adds(re_vec2_t vec, f32_t scaler);
RE_API re_vec2_t re_vec2_subs(re_vec2_t vec, f32_t scaler);

RE_API re_vec2_t re_vec2_rotate(re_vec2_t vec, f32_t degrees);

RE_API re_vec2_t re_vec2_normalize(re_vec2_t vec);
RE_API f32_t re_vec2_magnitude(re_vec2_t vec);
RE_API f32_t re_vec2_cross(re_vec2_t a, re_vec2_t b);
RE_API f32_t re_vec2_dot(re_vec2_t a, re_vec2_t b);

RE_API b8_t re_vec2_equal(re_vec2_t a, re_vec2_t b);

// 2D integer vector.
typedef union re_ivec2_t re_ivec2_t;
union re_ivec2_t {
    struct { i32_t x, y; };
    struct { i32_t u, v; };
    i32_t f[2];
};

RE_API re_ivec2_t re_ivec2(i32_t x, i32_t y);
RE_API re_ivec2_t re_ivec2s(i32_t scaler);

RE_API re_ivec2_t re_ivec2_mul(re_ivec2_t a, re_ivec2_t b);
RE_API re_ivec2_t re_ivec2_div(re_ivec2_t a, re_ivec2_t b);
RE_API re_ivec2_t re_ivec2_add(re_ivec2_t a, re_ivec2_t b);
RE_API re_ivec2_t re_ivec2_sub(re_ivec2_t a, re_ivec2_t b);

RE_API re_ivec2_t re_ivec2_muls(re_ivec2_t vec, i32_t scaler);
RE_API re_ivec2_t re_ivec2_divs(re_ivec2_t vec, i32_t scaler);
RE_API re_ivec2_t re_ivec2_adds(re_ivec2_t vec, i32_t scaler);
RE_API re_ivec2_t re_ivec2_subs(re_ivec2_t vec, i32_t scaler);

RE_API re_ivec2_t re_ivec2_rotate(re_ivec2_t vec, f32_t degrees);

RE_API re_ivec2_t re_ivec2_normalize(re_ivec2_t vec);
RE_API f32_t re_ivec2_magnitude(re_ivec2_t vec);
RE_API i32_t re_ivec2_cross(re_ivec2_t a, re_ivec2_t b);
RE_API i32_t re_ivec2_dot(re_ivec2_t a, re_ivec2_t b);

RE_API b8_t re_ivec2_equal(re_ivec2_t a, re_ivec2_t b);

// Conversion.
RE_API re_ivec2_t re_vec2_to_ivec2(re_vec2_t vec);
RE_API re_vec2_t re_ivec2_to_vec2(re_ivec2_t vec);

// 3D vector.
typedef union re_vec3_t re_vec3_t;
union re_vec3_t {
    struct { f32_t x, y, z; };
    struct { f32_t r, g, b; };
    f32_t f[3];
};

RE_API re_vec3_t re_vec3(f32_t x, f32_t y, f32_t z);
RE_API re_vec3_t re_vec3s(f32_t scaler);
RE_API re_vec3_t re_vec3_hex1(u32_t hex);
RE_API re_vec3_t re_vec3_hex255(u32_t hex);

RE_API re_vec3_t re_vec3_mul(re_vec3_t a, re_vec3_t b);
RE_API re_vec3_t re_vec3_div(re_vec3_t a, re_vec3_t b);
RE_API re_vec3_t re_vec3_add(re_vec3_t a, re_vec3_t b);
RE_API re_vec3_t re_vec3_sub(re_vec3_t a, re_vec3_t b);

RE_API re_vec3_t re_vec3_muls(re_vec3_t vec, f32_t scaler);
RE_API re_vec3_t re_vec3_divs(re_vec3_t vec, f32_t scaler);
RE_API re_vec3_t re_vec3_adds(re_vec3_t vec, f32_t scaler);
RE_API re_vec3_t re_vec3_subs(re_vec3_t vec, f32_t scaler);

RE_API re_vec3_t re_vec3_normalize(re_vec3_t vec);
RE_API f32_t re_vec3_magnitude(re_vec3_t vec);
RE_API re_vec3_t re_vec3_cross(re_vec3_t a, re_vec3_t b);
RE_API f32_t re_vec3_dot(re_vec3_t a, re_vec3_t b);

RE_API b8_t re_vec3_equal(re_vec3_t a, re_vec3_t b);

// 3D integer vector.
typedef union re_ivec3_t re_ivec3_t;
union re_ivec3_t {
    struct { i32_t x, y, z; };
    struct { i32_t r, g, b; };
    i32_t f[3];
};

#define re_ivec3_to_ivec3v(VEC) (*(re_ivec3v_t *) &(VEC))
#define re_ivec3v_to_ivec3(VEC) (*(re_ivec3_t *) &(VEC))

RE_API re_ivec3_t re_ivec3(i32_t x, i32_t y, i32_t z);
RE_API re_ivec3_t re_ivec3s(i32_t scaler);
RE_API re_ivec3_t re_ivec3_hex1(u32_t hex);
RE_API re_ivec3_t re_ivec3_hex255(u32_t hex);

RE_API b8_t re_ivec3_equal(re_ivec3_t a, re_ivec3_t b);

// 4D vector.
typedef union re_vec4_t re_vec4_t;
union re_vec4_t {
    struct { f32_t x, y, z, w; };
    struct { f32_t r, g, b, a; };
    f32_t f[4];
};

RE_API re_vec4_t re_vec4(f32_t x, f32_t y, f32_t z, f32_t w);
RE_API re_vec4_t re_vec4s(f32_t scaler);
RE_API re_vec4_t re_vec4_hex1(u32_t hex);
RE_API re_vec4_t re_vec4_hex255(u32_t hex);

RE_API re_vec4_t re_vec4_mul(re_vec4_t a, re_vec4_t b);
RE_API re_vec4_t re_vec4_div(re_vec4_t a, re_vec4_t b);
RE_API re_vec4_t re_vec4_add(re_vec4_t a, re_vec4_t b);
RE_API re_vec4_t re_vec4_sub(re_vec4_t a, re_vec4_t b);

RE_API re_vec4_t re_vec4_muls(re_vec4_t vec, f32_t scaler);
RE_API re_vec4_t re_vec4_divs(re_vec4_t vec, f32_t scaler);
RE_API re_vec4_t re_vec4_adds(re_vec4_t vec, f32_t scaler);
RE_API re_vec4_t re_vec4_subs(re_vec4_t vec, f32_t scaler);

RE_API re_vec4_t re_vec4_normalize(re_vec4_t vec);
RE_API f32_t re_vec4_magnitude(re_vec4_t vec);

RE_API b8_t re_vec4_equal(re_vec4_t a, re_vec4_t b);

// 4x4 matrix.
typedef union re_mat4_t re_mat4_t;
union re_mat4_t {
    struct { re_vec4_t i, j, k, l; };
    re_vec4_t v[4];
    f32_t f1[16];
    f32_t f2[4][4];
};
typedef union re_mat4v_t re_mat4v_t;

// Creates a 4x4 identity matrix.
RE_API re_mat4_t re_mat4_identity(void);
// Calculates a 4x4 ortographic projection matrix.
RE_API re_mat4_t re_mat4_orthographic_projection(f32_t left, f32_t right, f32_t top, f32_t bottom, f32_t near, f32_t far);

/*=========================*/
// Pool
/*=========================*/

typedef struct re_pool_t re_pool_t;

typedef struct re_pool_handle_t re_pool_handle_t;
struct re_pool_handle_t {
    re_pool_t *pool;
    u32_t index;
    u32_t generation;
};
#define RE_POOL_INVALID_HANDLE (re_pool_handle_t) {NULL, U32_MAX, U32_MAX};

// Create a pool.
RE_API re_pool_t *re_pool_create(u32_t object_size, re_arena_t *arena);
RE_API u32_t re_pool_get_count(const re_pool_t *pool);

// Retrieve a new handle.
RE_API re_pool_handle_t re_pool_new(re_pool_t *pool);
// Give handle back, making it invalid.
RE_API void re_pool_delete(re_pool_handle_t handle);
// Retrieve data from handle.
RE_API void *re_pool_get_ptr(re_pool_handle_t handle);
// Check if handle if still valid.
RE_API b8_t re_pool_handle_valid(re_pool_handle_t handle);


typedef struct re_pool_iter_t re_pool_iter_t;
struct re_pool_iter_t {
    re_pool_t *pool;
    u32_t index;
};

RE_API re_pool_iter_t re_pool_iter_new(re_pool_t *pool);
RE_API b8_t re_pool_iter_valid(re_pool_iter_t iter);
RE_API void re_pool_iter_next(re_pool_iter_t *iter);
RE_API re_pool_handle_t re_pool_iter_get(re_pool_iter_t iter);

/*=========================*/
// Error handling
/*=========================*/

typedef enum {
    RE_ERROR_LEVEL_FATAL,
    RE_ERROR_LEVEL_ERROR,
    RE_ERROR_LEVEL_WARN,
    RE_ERROR_LEVEL_DEBUG = RE_LOG_LEVEL_DEBUG
} re_error_level_t;

typedef struct re_error_t re_error_t;
struct re_error_t {
    char message[256];
    re_error_level_t level;
    const char *file;
    i32_t line;
};

typedef void (*re_error_callback_t)(re_error_t error);

// Signals an error.
#define re_error(level, ...) _re_error(level, __FILE__, __LINE__, __VA_ARGS__)
// Retrieves the latest error off the stack.
// There can be up to 16 errors on the stack at once.
RE_API re_error_t re_error_pop(void);
// Sets an error callback to be called every time an error is signaled.
RE_API void re_error_set_callback(re_error_callback_t callback);
// Sets the minimum error level that will be sent to the stack and callback.
RE_API void re_error_set_level(re_error_level_t level);
// Premade callback that logs all errors.
RE_API void re_error_log_callback(re_error_t error);

RE_API void _re_error(re_error_level_t level, const char *file, i32_t line, const char *fmt, ...) RE_FORMAT_FUNCTION(4, 5);

/*=========================*/
// File handling
/*=========================*/

RE_API re_str_t re_file_read(const char *filepath, re_arena_t *arena);

//  ____  _       _    __                        _
// |  _ \| | __ _| |_ / _| ___  _ __ _ __ ___   | |    __ _ _   _  ___ _ __
// | |_) | |/ _` | __| |_ / _ \| '__| '_ ` _ \  | |   / _` | | | |/ _ \ '__|
// |  __/| | (_| | |_|  _| (_) | |  | | | | | | | |__| (_| | |_| |  __/ |
// |_|   |_|\__,_|\__|_|  \___/|_|  |_| |_| |_| |_____\__,_|\__, |\___|_|
//                                                          |___/
// Platform layer

/*=========================*/
// Initialization
/*=========================*/

RE_API void re_os_init(void);
RE_API void re_os_terminate(void);

/*=========================*/
// Dynamic library loading
/*=========================*/

typedef enum {
    RE_LIB_MODE_GLOBAL,
    RE_LIB_MODE_LOCAL
} re_lib_mode_t;

typedef struct re_lib_t re_lib_t;

typedef void (*re_func_ptr_t)(void);

// Loads a dynamic library.
// Returns NULL if it fails.
RE_API re_lib_t *re_lib_load(const char *filepath, re_lib_mode_t mode);
// Unloads a dynamic library.
RE_API void re_lib_unload(re_lib_t *lib);
// Retrieves a function using 'name' from dynamic library.
RE_API re_func_ptr_t re_lib_func(const re_lib_t *lib, const char *name);

/*=========================*/
// Multithreading
/*=========================*/

// Threads
typedef struct re_thread_t re_thread_t;
struct re_thread_t {
    usize_t handle;
};

typedef void (*re_thread_func_t)(void *arg);

// Creates a new thread and executes 'func' passing 'arg' to it.
RE_API re_thread_t re_thread_create(re_thread_func_t func, void *arg);
// Frees all memory and handles to thread.
RE_API void re_thread_destroy(re_thread_t thread);
// Pauses current thread until 'thread' is finished.
RE_API void re_thread_wait(re_thread_t thread);

// Mutexes
typedef struct re_mutex_t re_mutex_t;

// Creates a mutex.
RE_API re_mutex_t *re_mutex_create(void);
// Frees all memory and handles to mutex.
RE_API void re_mutex_destroy(re_mutex_t *mutex);
// Locks mutex.
RE_API void re_mutex_lock(re_mutex_t *mutex);
// Unlocks mutex.
RE_API void re_mutex_unlock(re_mutex_t *mutex);

/*=========================*/
// System info
/*=========================*/

// Gets time since last re_os_get_time call.
RE_API f32_t re_os_get_time(void);
// Gets number of usable cores.
RE_API u32_t re_os_get_processor_count(void);
// Gets size of a memory page.
RE_API u32_t re_os_get_page_size(void);

/*=========================*/
// Memory
/*=========================*/

RE_API void *re_os_mem_reserve(usize_t size);
RE_API void re_os_mem_commit(void *ptr, usize_t size);
RE_API void re_os_mem_decommit(void *ptr, usize_t size);
RE_API void re_os_mem_release(void *ptr, usize_t size);

#ifdef RE_UNIT_TESTS

/*=========================*/
// Unit test
/*=========================*/

RE_API void re_dyn_arr_unit_test(void);
RE_API void re_hash_map_unit_test(void);

#endif // RE_UNIT_TESTS

#endif // REBOUND_H
