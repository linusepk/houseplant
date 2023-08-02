#include "rebound.h"

#ifdef RE_OS_LINUX
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef RE_OS_WINDOWS
#include <windows.h>
#endif

#include <math.h>
#include <stdlib.h>
#include <time.h>

//  ____                   _
// | __ )  __ _ ___  ___  | |    __ _ _   _  ___ _ __
// |  _ \ / _` / __|/ _ \ | |   / _` | | | |/ _ \ '__|
// | |_) | (_| \__ \  __/ | |__| (_| | |_| |  __/ |
// |____/ \__,_|___/\___| |_____\__,_|\__, |\___|_|
//                                    |___/
// Base layer

/*=========================*/
// Global state
/*=========================*/

/* typedef struct _rebound_state_t _rebound_state_t; */
/* struct _rebound_state_t { */
/* }; */
/* static _rebound_state_t _re_state = {0}; */

/*=========================*/
// Initialization
/*=========================*/

void re_init(void) {
    re_os_init();
}

void re_terminate(void) {
    _re_arena_scratch_destroy();
    re_os_terminate();
}

/*=========================*/
// Allocators
/*=========================*/

void *re_malloc(usize_t size) {
    void *ptr = RE_MALLOC(size);
    RE_ENSURE(ptr != NULL, OUT_OF_MEMORY);
    return ptr;
}

void *re_realloc(void *ptr, usize_t size) {
    void *new_ptr = RE_REALLOC(ptr, size);
    RE_ENSURE(new_ptr != NULL, OUT_OF_MEMORY);
    return new_ptr;
}

void re_free(void *ptr) { RE_FREE(ptr); }

/*=========================*/
// Arena
/*=========================*/

struct re_arena_t {
    u64_t capacity;
    u64_t position;
    u64_t commited;
    ptr_t pool;
};

re_arena_t *re_arena_create(u64_t capacity) {
    u64_t actual_capacity = ((sizeof(re_arena_t) + capacity) + re_os_get_page_size() - 1) & (~re_os_get_page_size() - 1);
    re_arena_t *arena = re_os_mem_reserve(actual_capacity);
    re_os_mem_commit(arena, re_os_get_page_size());

    arena->capacity = actual_capacity;
    arena->position = 0;
    arena->commited = re_os_get_page_size();
    arena->pool = (ptr_t) arena + sizeof(re_arena_t);

    return arena;
}

void re_arena_destroy(re_arena_t **arena) {
    re_os_mem_release(*arena, (*arena)->capacity);
    *arena = NULL;
}

void *re_arena_push(re_arena_t *arena, u64_t size) {
    while (arena->position + size >= arena->commited) {
        re_os_mem_commit((ptr_t) arena + arena->commited, re_os_get_page_size());
        arena->commited += re_os_get_page_size();
    }

    void *result = arena->pool + arena->position;
    arena->position += size;
    return result;
}

void *re_arena_push_zero(re_arena_t *arena, u64_t size) {
    void *result = re_arena_push(arena, size);

    ptr_t iter = result;
    for (u64_t i = 0; i < size; i++) {
        iter[i] = 0;
    }

    return result;
}

void re_arena_pop(re_arena_t *arena, u64_t size) {
    while (arena->position <= arena->commited - re_os_get_page_size()) {
        arena->commited -= re_os_get_page_size();
        re_os_mem_decommit((ptr_t) arena + arena->commited, re_os_get_page_size());
    }
    arena->position -= size;
}

void re_arena_clear(re_arena_t *arena) {
    re_os_mem_decommit((ptr_t) arena + re_os_get_page_size(), arena->commited - re_os_get_page_size());
    arena->commited = re_os_get_page_size();
    arena->position = 0;
}

u64_t re_arena_get_pos(re_arena_t *arena) {
    return arena->position;
}

void *re_arena_get_index(u64_t index, re_arena_t *arena) {
    return arena->pool + index;
}

re_arena_temp_t re_arena_temp_start(re_arena_t *arena) {
    return (re_arena_temp_t) {
        .arena = arena,
        .position = arena->position
    };
}

void re_arena_temp_end(re_arena_temp_t *arena) {
    arena->arena->position = arena->position;
}

/*=========================*/
// Scratch arena
/*=========================*/

#define RE_SCRATCH_POOL_SIZE 2

RE_THREAD_LOCAL re_arena_t *_re_scratch_pool[RE_SCRATCH_POOL_SIZE] = {0};

re_arena_temp_t re_arena_scratch_get(re_arena_t **conflicts, u32_t conflict_count) {
    if (_re_scratch_pool[0] == NULL) {
        for (u32_t i = 0; i < RE_SCRATCH_POOL_SIZE; i++) {
            _re_scratch_pool[i] = re_arena_create(GB(8));
        }
    }

    re_arena_temp_t result = {0};
    for (u32_t i = 0; i < RE_SCRATCH_POOL_SIZE; i++) {
        b8_t conflict = false;
        for (u32_t j = 0; j < conflict_count; j++) {
            if (conflicts[i] == _re_scratch_pool[i]) {
                conflict = true;
                break;
            }
        }
        if (!conflict) {
            result = re_arena_temp_start(_re_scratch_pool[i]);
            break;
        }
    }

    return result;
}

void _re_arena_scratch_destroy(void) {
    if (_re_scratch_pool[0] != NULL) {
        for (u32_t i = 0; i < RE_SCRATCH_POOL_SIZE; i++) {
            re_arena_destroy(&_re_scratch_pool[i]);
        }
    }
}

/*=========================*/
// Utils
/*=========================*/

usize_t re_fvn1a_hash(const char *key, usize_t len) {
    u32_t hash = 2166136261u;
    for (u32_t i = 0; i < len; i++) {
        hash ^= (u8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

void re_format_string(char buffer[1024], const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);
}

/*=========================*/
// Hash table
/*=========================*/

re_ht_iter_t __re_ht_iter_next(usize_t start, void *entries,
        usize_t entry_count, usize_t alive_offset,
        usize_t stride) {
    for (usize_t i = start; i < entry_count; i++) {
        u8_t *entry = (u8_t *)entries + i * stride;
        b8_t alive = *(entry + alive_offset);

        if (alive) {
            return i;
        }
    }

    return entry_count;
}

/*=========================*/
// Strings
/*=========================*/

re_str_t re_str(u8_t *cstr, usize_t len) { return (re_str_t) {cstr, len}; }

re_str_t re_str_sub(re_str_t string, usize_t start, usize_t end) {
    usize_t len = end - start + 1;
    len = re_clamp_max(len, string.len);
    return (re_str_t) {&string.str[start], len};
}

re_str_t re_str_prefix(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(len, string.len);
    return (re_str_t) {string.str, clamped_len};
}

re_str_t re_str_suffix(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(len, string.len);
    return (re_str_t) {&string.str[string.len - clamped_len], clamped_len};
}

re_str_t re_str_chop(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(len, string.len);
    return (re_str_t){string.str, string.len - clamped_len};
}

re_str_t re_str_skip(re_str_t string, usize_t len) {
    usize_t clamped_len = re_clamp_max(string.len - len, string.len);
    return (re_str_t){string.str + len, clamped_len};
}

i32_t re_str_cmp(re_str_t a, re_str_t b) {
    if (a.str == b.str) {
        return 0;
    } else if (a.str == NULL) {
        return -1;
    } else if (b.str == NULL) {
        return 1;
    } else if (a.len != b.len) {
        return a.str[0] > b.str[0] ? 1 : -1;
    }

    for (usize_t i = 0; i < a.len; i++) {
        if (a.str[i] != b.str[i]) {
            return a.str[i] > b.str[i] ? 1 : -1;
        }
    }

    return 0;
}

/*=========================*/
// Dynamic array
/*=========================*/

#define _RE_DA_INIT_CAP 8

typedef struct _re_da_header_t _re_da_header_t;
struct _re_da_header_t {
    usize_t size;
    usize_t count;
    usize_t cap;
};

#define _re_da_to_head(DA) ((_re_da_header_t *) ((ptr_t) (DA) - sizeof(_re_da_header_t)))
#define _re_head_to_da(HEAD) ((void *) ((ptr_t) (HEAD) + sizeof(_re_da_header_t)))

void _re_da_resize(void **da, usize_t count) {
    _re_da_header_t *head = _re_da_to_head(*da);
    b8_t resize_needed = false;
    if (head->count + count > head->cap) {
        head->cap *= 2;
        resize_needed = true;
    }

    if (!resize_needed) {
        return;
    }

    void *new_head =
        re_realloc(head, sizeof(_re_da_header_t) + head->size * head->cap);
    if (!new_head) {
        *da = NULL;
        return;
    }
    *da = _re_head_to_da(new_head);
}

void _re_da_create(void **da, usize_t size) {
    usize_t total_size = sizeof(_re_da_header_t) + size * _RE_DA_INIT_CAP;
    _re_da_header_t *head = re_malloc(total_size);
    if (!head) {
        *da = NULL;
        return;
    }
    head->size = size;
    head->count = 0;
    head->cap = _RE_DA_INIT_CAP;

    *da = _re_head_to_da(head);
}

void _re_da_destroy(void **da) {
    _re_da_header_t *head = _re_da_to_head(*da);
    re_free(head);
    *da = NULL;
}

void _re_da_insert_fast(void **da, const void *value, usize_t index) {
    _re_da_resize(da, 1);

    _re_da_header_t *head = _re_da_to_head(*da);
    index = re_clamp_max(index, head->count);

    ptr_t dest = (ptr_t) *da + head->count * head->size;
    ptr_t source = (ptr_t) *da + index * head->size;

    memmove(dest, source, head->size);
    memcpy(source, value, head->size);

    head->count++;
}

void _re_da_remove_fast(void **da, usize_t index, void *output) {
    _re_da_header_t *head = _re_da_to_head(*da);
    index = re_clamp_max(index, head->count - 1);

    ptr_t dest = (ptr_t) *da + index * head->size;
    ptr_t source = (ptr_t) *da + (head->count - 1) * head->size;

    if (output != NULL) {
        memcpy(output, dest, head->size);
    }
    memmove(dest, source, head->size);

    head->count--;
}

void _re_da_insert_arr(void **da, const void *arr, usize_t count, usize_t index) {
    _re_da_resize(da, count);

    _re_da_header_t *head = _re_da_to_head(*da);
    index = re_clamp_max(index, head->count);

    ptr_t dest = (ptr_t) *da + (index + count) * head->size;
    ptr_t source = (ptr_t) *da + index * head->size;

    memmove(dest, source, (head->count - index) * head->size);
    if (arr == NULL) {
        memset(source, 0, count * head->size);
    } else {
        memcpy(source, arr, count * head->size);
    }

    head->count += count;
}

void _re_da_remove_arr(void **da, usize_t count, usize_t index, void *output) {
    _re_da_header_t *head = _re_da_to_head(*da);
    index = re_clamp_max(index, head->count - count);

    ptr_t dest = (ptr_t) *da + index * head->size;
    ptr_t source = (ptr_t) *da + (index + count) * head->size;

    if (output != NULL) {
        memcpy(output, dest, count * head->size);
    }
    memmove(dest, source, (head->count - index - count) * head->size);

    head->count -= count;
}

usize_t _re_da_count(void *da) {
    return _re_da_to_head(da)->count;
}

/*=========================*/
// Logger
/*=========================*/

typedef struct _re_logger_callback_t _re_logger_callback_t;
struct _re_logger_callback_t {
    re_log_callback_t func;
    re_log_level_t level;
    void *user_data;
};

typedef struct _re_logger_t _re_logger_t;
struct _re_logger_t {
    b8_t silent;
    _re_logger_callback_t callbacks[32];
    u32_t callback_i;
    re_log_level_t level;
};

static _re_logger_t _re_logger = {.level = RE_LOG_LEVEL_TRACE};
static const char *_re_log_level_string[RE_LOG_LEVEL_COUNT] = {
    "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

static void _re_log_stdout_callback(re_log_event_t *const event) {
    FILE *fp = event->level < RE_LOG_LEVEL_WARN ? stdout : stderr;

#ifndef RE_LOG_NO_COLOR
    static const char *level_color[RE_LOG_LEVEL_COUNT] = {
        "\033[1;101m", "\033[1;91m", "\033[0;93m",
        "\033[0;92m",  "\033[0;94m", "\033[0;95m"};

    u32_t i = 0;
    while (event->message[i] != '\0') {
        fprintf(fp,
                "\033[2;37m%.2d:%.2d:%.2d\033[0m %s%-5s\033[0m \033[2;37m%s:%d: "
                "\033[0m",
                event->time.hour, event->time.min, event->time.sec,
                level_color[event->level], _re_log_level_string[event->level],
                event->file, event->line);

        u32_t start = i;
        while (event->message[i] != '\n' && event->message[i] != '\0') {
            i++;
        }
        u32_t end = i;

        fprintf(fp, "%.*s\n", end - start, event->message + start);
        i++;
    }
#else
    u32_t i = 0;
    while (event->message[i] != '\0') {
        fprintf(fp, "%.2d:%.2d:%.2d %-5s %s:%d: ", event->time.hour,
                event->time.min, event->time.sec,
                _re_log_level_string[event->level], event->file, event->line);

        u32_t start = i;
        while (event->message[i] != '\n' && event->message[i] != '\0') {
            i++;
        }
        u32_t end = i;

        fprintf(fp, "%.*s\n", end - start, event->message + start);
        i++;
    }
#endif
}

static void _re_log_file_callback(re_log_event_t *const event) {
    u32_t i = 0;
    while (event->message[i] != '\0') {
        fprintf(event->user_data, "%.2d:%.2d:%.2d %-5s %s:%d: ", event->time.hour,
                event->time.min, event->time.sec,
                _re_log_level_string[event->level], event->file, event->line);

        u32_t start = i;
        while (event->message[i] != '\n' && event->message[i] != '\0') {
            i++;
        }
        u32_t end = i;

        fprintf(event->user_data, "%.*s\n", end - start, event->message + start);
        i++;
    }

    fflush(event->user_data);
}

void _re_log(const char *file, i32_t line, re_log_level_t level,
        const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    re_log_event_t event = {.file = file,
        .line = line,
        .level = level,
        .time = {
            .hour = tm->tm_hour,
            .min = tm->tm_min,
            .sec = tm->tm_sec,
        }};

    va_list args;
    va_start(args, fmt);
    vsnprintf(event.message, RE_LOG_MESSAGE_MAX_LENGTH, fmt, args);
    va_end(args);
    event.message_length = strlen(event.message);

    if (!_re_logger.silent && level <= _re_logger.level) {
        _re_log_stdout_callback(&event);
    }

    for (u32_t i = 0; i < _re_logger.callback_i; i++) {
        if (level <= _re_logger.callbacks[i].level) {
            event.user_data = _re_logger.callbacks[i].user_data;
            _re_logger.callbacks[i].func(&event);
        }
    }
}

void re_logger_add_callback(re_log_callback_t callback, re_log_level_t level,
        void *user_data) {
    RE_ASSERT(_re_logger.callback_i < RE_LOGGER_CALLBACK_MAX,
            "Can't surpace %d logger callbacks", RE_LOGGER_CALLBACK_MAX);

    _re_logger.callbacks[_re_logger.callback_i++] = (_re_logger_callback_t){
        .func = callback, .level = level, .user_data = user_data};
}

void re_logger_add_fp(FILE *fp, re_log_level_t level) {
    re_logger_add_callback(_re_log_file_callback, level, fp);
}

void re_logger_set_silent(b8_t silent) { _re_logger.silent = silent; }

void re_logger_set_level(re_log_level_t level) { _re_logger.level = level; }

/*=========================*/
// Math
/*=========================*/

// 2D vector.
re_vec2_t re_vec2(f32_t x, f32_t y) { return (re_vec2_t){x, y}; }
re_vec2_t re_vec2s(f32_t scaler) { return re_vec2(scaler, scaler); }

re_vec2_t re_vec2_mul(re_vec2_t a, re_vec2_t b) {
    return re_vec2(a.x * b.x, a.y * b.y);
}
re_vec2_t re_vec2_div(re_vec2_t a, re_vec2_t b) {
    return re_vec2(a.x / b.x, a.y / b.y);
}
re_vec2_t re_vec2_add(re_vec2_t a, re_vec2_t b) {
    return re_vec2(a.x + b.x, a.y + b.y);
}
re_vec2_t re_vec2_sub(re_vec2_t a, re_vec2_t b) {
    return re_vec2(a.x - b.x, a.y - b.y);
}

re_vec2_t re_vec2_muls(re_vec2_t vec, f32_t scaler) {
    return re_vec2(vec.x * scaler, vec.y * scaler);
}
re_vec2_t re_vec2_divs(re_vec2_t vec, f32_t scaler) {
    return re_vec2(vec.x / scaler, vec.y / scaler);
}
re_vec2_t re_vec2_adds(re_vec2_t vec, f32_t scaler) {
    return re_vec2(vec.x + scaler, vec.y + scaler);
}
re_vec2_t re_vec2_subs(re_vec2_t vec, f32_t scaler) {
    return re_vec2(vec.x - scaler, vec.y - scaler);
}

re_vec2_t re_vec2_rotate(re_vec2_t vec, f32_t degrees) {
    f32_t theta = RAD(degrees);
    return re_vec2(vec.x * cosf(theta) - vec.y * sinf(theta),
            vec.x * sinf(theta) + vec.y * cosf(theta));
}

re_vec2_t re_vec2_normalize(re_vec2_t vec) {
    return re_vec2_muls(vec, 1.0f / re_vec2_magnitude(vec));
}
f32_t re_vec2_magnitude(re_vec2_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y);
}
f32_t re_vec2_cross(re_vec2_t a, re_vec2_t b) { return a.x * b.y - a.y * b.x; }
f32_t re_vec2_dot(re_vec2_t a, re_vec2_t b) { return a.x * b.x + a.y * b.y; }

b8_t re_vec2_equal(re_vec2_t a, re_vec2_t b) {
    return a.x == b.x && a.y == b.y;
}

// 2D integer vector.
re_ivec2_t re_ivec2(i32_t x, i32_t y) { return (re_ivec2_t){x, y}; }
re_ivec2_t re_ivec2s(i32_t scaler) { return re_ivec2(scaler, scaler); }

re_ivec2_t re_ivec2_mul(re_ivec2_t a, re_ivec2_t b) {
    return re_ivec2(a.x * b.x, a.y * b.y);
}
re_ivec2_t re_ivec2_div(re_ivec2_t a, re_ivec2_t b) {
    return re_ivec2(a.x / b.x, a.y / b.y);
}
re_ivec2_t re_ivec2_add(re_ivec2_t a, re_ivec2_t b) {
    return re_ivec2(a.x + b.x, a.y + b.y);
}
re_ivec2_t re_ivec2_sub(re_ivec2_t a, re_ivec2_t b) {
    return re_ivec2(a.x - b.x, a.y - b.y);
}

re_ivec2_t re_ivec2_muls(re_ivec2_t vec, i32_t scaler) {
    return re_ivec2(vec.x * scaler, vec.y * scaler);
}
re_ivec2_t re_ivec2_divs(re_ivec2_t vec, i32_t scaler) {
    return re_ivec2(vec.x / scaler, vec.y / scaler);
}
re_ivec2_t re_ivec2_adds(re_ivec2_t vec, i32_t scaler) {
    return re_ivec2(vec.x + scaler, vec.y + scaler);
}
re_ivec2_t re_ivec2_subs(re_ivec2_t vec, i32_t scaler) {
    return re_ivec2(vec.x - scaler, vec.y - scaler);
}

re_ivec2_t re_ivec2_rotate(re_ivec2_t vec, f32_t degrees) {
    f32_t theta = RAD(degrees);
    return re_ivec2(vec.x * cosf(theta) - vec.y * sinf(theta),
            vec.x * sinf(theta) + vec.y * cosf(theta));
}

re_ivec2_t re_ivec2_normalize(re_ivec2_t vec) {
    return re_ivec2_muls(vec, 1.0f / re_ivec2_magnitude(vec));
}
f32_t re_ivec2_magnitude(re_ivec2_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y);
}
i32_t re_ivec2_cross(re_ivec2_t a, re_ivec2_t b) {
    return a.x * b.y - a.y * b.x;
}
i32_t re_ivec2_dot(re_ivec2_t a, re_ivec2_t b) { return a.x * b.x + a.y * b.y; }

b8_t re_ivec2_equal(re_ivec2_t a, re_ivec2_t b) {
    return a.x == b.x && a.y == b.y;
}

// Conversion.
re_ivec2_t re_vec2_to_ivec2(re_vec2_t vec) {
    return re_ivec2(vec.x, vec.y);
}

re_vec2_t re_ivec2_to_vec2(re_ivec2_t vec) {
    return re_vec2(vec.x, vec.y);
}

// 3D vector.
re_vec3_t re_vec3(f32_t x, f32_t y, f32_t z) { return (re_vec3_t){x, y, z}; }
re_vec3_t re_vec3s(f32_t scaler) { return re_vec3(scaler, scaler, scaler); }

re_vec3_t re_vec3_mul(re_vec3_t a, re_vec3_t b) {
    return re_vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}
re_vec3_t re_vec3_div(re_vec3_t a, re_vec3_t b) {
    return re_vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}
re_vec3_t re_vec3_add(re_vec3_t a, re_vec3_t b) {
    return re_vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
re_vec3_t re_vec3_sub(re_vec3_t a, re_vec3_t b) {
    return re_vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

re_vec3_t re_vec3_muls(re_vec3_t vec, f32_t scaler) {
    return re_vec3(vec.x * scaler, vec.y * scaler, vec.z * scaler);
}
re_vec3_t re_vec3_divs(re_vec3_t vec, f32_t scaler) {
    return re_vec3(vec.x / scaler, vec.y / scaler, vec.z / scaler);
}
re_vec3_t re_vec3_adds(re_vec3_t vec, f32_t scaler) {
    return re_vec3(vec.x + scaler, vec.y + scaler, vec.z + scaler);
}
re_vec3_t re_vec3_subs(re_vec3_t vec, f32_t scaler) {
    return re_vec3(vec.x - scaler, vec.y - scaler, vec.z - scaler);
}

re_vec3_t re_vec3_normalize(re_vec3_t vec) {
    return re_vec3_muls(vec, 1.0f / re_vec3_magnitude(vec));
}
f32_t re_vec3_magnitude(re_vec3_t vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}
re_vec3_t re_vec3_cross(re_vec3_t a, re_vec3_t b) {
    return re_vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
}
f32_t re_vec3_dot(re_vec3_t a, re_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

b8_t re_vec3_equal(re_vec3_t a, re_vec3_t b) {
    return a.x == b.x && a.y == b.y && a.z == b.y;
}


// 3D integer vector.
re_ivec3_t re_ivec3(i32_t x, i32_t y, i32_t z) { return (re_ivec3_t){x, y, z}; }
re_ivec3_t re_ivec3s(i32_t scaler) { return re_ivec3(scaler, scaler, scaler); }

b8_t re_ivec3_equal(re_ivec3_t a, re_ivec3_t b) {
    return a.x == b.x && a.y == b.y && a.z == b.y;
}

// 4D vector.
re_vec4_t re_vec4(f32_t x, f32_t y, f32_t z, f32_t w) { return (re_vec4_t) {x, y, z, w}; }
re_vec4_t re_vec4s(f32_t scaler) { return (re_vec4_t) {scaler, scaler, scaler, scaler}; }

re_vec4_t re_vec4_mul(re_vec4_t a, re_vec4_t b) {
    return re_vec4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);
}

re_vec4_t re_vec4_div(re_vec4_t a, re_vec4_t b) {
    return re_vec4(a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w);
}

re_vec4_t re_vec4_add(re_vec4_t a, re_vec4_t b) {
    return re_vec4(a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w);
}

re_vec4_t re_vec4_sub(re_vec4_t a, re_vec4_t b) {
    return re_vec4(a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w);
}

re_vec4_t re_vec4_muls(re_vec4_t vec, f32_t scaler) {
    return re_vec4(vec.x*scaler, vec.y*scaler, vec.z*scaler, vec.w*scaler);
}

re_vec4_t re_vec4_divs(re_vec4_t vec, f32_t scaler) {
    return re_vec4(vec.x/scaler, vec.y/scaler, vec.z/scaler, vec.w/scaler);
}

re_vec4_t re_vec4_adds(re_vec4_t vec, f32_t scaler) {
    return re_vec4(vec.x+scaler, vec.y+scaler, vec.z+scaler, vec.w+scaler);
}

re_vec4_t re_vec4_subs(re_vec4_t vec, f32_t scaler) {
    return re_vec4(vec.x-scaler, vec.y-scaler, vec.z-scaler, vec.w-scaler);
}

re_vec4_t re_vec4_normalize(re_vec4_t vec) {
    return re_vec4_muls(vec, 1.0f / re_vec4_magnitude(vec));
}

f32_t re_vec4_magnitude(re_vec4_t vec) {
    return sqrtf(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z + vec.w*vec.w);
}

b8_t re_vec4_equal(re_vec4_t a, re_vec4_t b) {
    return a.x == b.x && a.y == b.y && a.z == b.y && a.w == b.w;
}

// 4x4 matrix.
re_mat4_t re_mat4_identity(void) {
    return (re_mat4_t){{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
}

re_mat4_t re_mat4_orthographic_projection(f32_t left, f32_t right, f32_t top,
        f32_t bottom, f32_t near, f32_t far) {
    // http://learnwebgl.brown37.net/08_projections/projections_ortho.html
    f32_t rl =  1.0f / (right - left);
    f32_t tb =  1.0f / (top   - bottom);
    f32_t fn = -1.0f / (far   - near);

    return (re_mat4_t){
        {2.0f * rl, 0, 0, -(right+left) * rl},
        {0, 2.0f * tb, 0, -(top+bottom) * tb},
        {0, 0, -fn, near * fn},
        {0, 0, 0, 1},
    };
}

/*=========================*/
// Pool
/*=========================*/

typedef struct _re_pool_node_t _re_pool_node_t;
struct _re_pool_node_t {
    _re_pool_node_t *next;
    _re_pool_node_t *prev;
    u32_t index;
    u32_t generation;
};

struct re_pool_t {
    re_arena_t *arena;
    u32_t size;
    _re_pool_node_t *free_node;
    _re_pool_node_t *used_nodes;
    u32_t count;
};

re_pool_t *re_pool_create(u32_t object_size, re_arena_t *arena) {
    re_pool_t *pool = re_arena_push(arena, sizeof(re_pool_t));

    pool->arena = arena;
    pool->size = object_size;
    pool->free_node = NULL;
    pool->used_nodes = NULL;

    return pool;
}

u32_t re_pool_get_count(const re_pool_t *pool) {
    return pool->count;
}

re_pool_handle_t re_pool_new(re_pool_t *pool) {
    _re_pool_node_t *node = pool->free_node;
    if (node != NULL) {
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }
        pool->free_node = node->next;
    } else {
        node = re_arena_push_zero(pool->arena, sizeof(_re_pool_node_t) + pool->size);
        node->index = re_arena_get_pos(pool->arena) - pool->size;
    }

    if (pool->used_nodes != NULL) {
        pool->used_nodes->prev = node;
    }
    node->next = pool->used_nodes;
    pool->used_nodes = node;

    pool->count++;
    return (re_pool_handle_t) {
        .pool = pool,
        .index = node->index,
        .generation = node->generation
    };
}

void re_pool_delete(re_pool_handle_t handle) {
    _re_pool_node_t *node = (_re_pool_node_t *) ((ptr_t) re_pool_get_ptr(handle) - sizeof(_re_pool_node_t));
    if (handle.pool->free_node != NULL) {
        handle.pool->free_node->prev = node;
    }

    if (node->next != NULL) {  
        node->next->prev = node->prev;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (handle.pool->used_nodes == node) {
        handle.pool->used_nodes = NULL;
    }

    node->next = handle.pool->free_node;
    handle.pool->free_node = node;
    node->generation++;
    handle.pool->count--;
}

b8_t re_pool_handle_valid(re_pool_handle_t handle) {
    if (handle.pool == NULL || (handle.index == U32_MAX && handle.generation == U32_MAX)) {
        return false;
    }

    _re_pool_node_t *node = (_re_pool_node_t *) ((ptr_t) re_pool_get_ptr(handle) - sizeof(_re_pool_node_t));
    return node->generation == handle.generation;
}

void *re_pool_get_ptr(re_pool_handle_t handle) {
    return re_arena_get_index(handle.index, handle.pool->arena);
}

re_pool_iter_t re_pool_iter_new(re_pool_t *pool) {
    if (pool->used_nodes == NULL) {
        return (re_pool_iter_t) {NULL, U32_MAX};
    }

    return (re_pool_iter_t) {
        .pool = pool,
        .index = pool->used_nodes->index
    };
}

b8_t re_pool_iter_valid(re_pool_iter_t iter) {
    return iter.index != U32_MAX || iter.pool != NULL;
}

void re_pool_iter_next(re_pool_iter_t *iter) {
    _re_pool_node_t *node = (_re_pool_node_t *) ((ptr_t) re_arena_get_index(iter->index, iter->pool->arena) - sizeof(_re_pool_node_t));
    if (node->next == NULL) {
        iter->index = U32_MAX;
        iter->pool = NULL;
        return;
    }

    iter->index = node->next->index;
}

re_pool_handle_t re_pool_iter_get(re_pool_iter_t iter) {
    if (!re_pool_iter_valid(iter)) {
        return RE_POOL_INVALID_HANDLE;
    }

    _re_pool_node_t *node = (_re_pool_node_t *) ((ptr_t) re_arena_get_index(iter.index, iter.pool->arena) - sizeof(_re_pool_node_t));
    return (re_pool_handle_t) {
        .pool = iter.pool,
        .index = node->index,
        .generation = node->generation
    };
}

/*=========================*/
// Error handling
/*=========================*/

#define RE_ERROR_STACK_SIZE 16

static re_error_t _re_error_stack[RE_ERROR_STACK_SIZE] = {0};
static u32_t _re_error_stack_count = 0;
static re_error_callback_t _re_error_callback = NULL;
static re_error_level_t _re_error_level = RE_ERROR_LEVEL_WARN;

re_error_t re_error_pop(void) {
    re_error_t error = _re_error_stack[0];

    _re_error_stack[_re_error_stack_count] = (re_error_t) {0};
    memmove(&_re_error_stack[0], &_re_error_stack[1], sizeof(_re_error_stack) - sizeof(re_error_t));
    if (_re_error_stack_count > RE_ERROR_STACK_SIZE) {
        _re_error_stack_count--;
    }

    return error;
}

void _re_error(re_error_level_t level, const char *file, i32_t line, const char *fmt, ...) {
    if (level > _re_error_level) {
        return;
    }

    re_error_t error = {
        .message = {0},
        .level = level,
        .file = file,
        .line = line
    };
    va_list args;
    va_start(args, fmt);
    vsnprintf(error.message, sizeof(error.message), fmt, args);
    va_end(args);

    memmove(&_re_error_stack[1], &_re_error_stack[0], sizeof(_re_error_stack) - sizeof(re_error_t));
    if (_re_error_stack_count < RE_ERROR_STACK_SIZE) {
        _re_error_stack_count++;
    }
    _re_error_stack[0] = error;

    if (_re_error_callback != NULL) {
        _re_error_callback(error);
    }
}

void re_error_set_callback(re_error_callback_t callback) { _re_error_callback = callback; }
void re_error_set_level(re_error_level_t level) { _re_error_level = level; }
void re_error_log_callback(re_error_t error) {
    _re_log(error.file, error.line, (re_log_level_t) error.level, "%s", error.message);
}

/*=========================*/
// File handling
/*=========================*/

re_str_t re_file_read(const char *filepath, re_arena_t *arena) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        return re_str_null;
    }

    fseek(fp, 0, SEEK_END);
    u32_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    u8_t *buffer = re_arena_push(arena, len);
    fread(buffer, 1, len, fp);
    return re_str(buffer, len);
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
// Initialization
/*=========================*/

typedef struct _re_os_state_t _re_os_state_t;
struct _re_os_state_t {
    f32_t start_time;
    u32_t processor_count;
    u32_t page_size;
};

static _re_os_state_t _re_os_state = {0};

void re_os_init(void) {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    _re_os_state.start_time = (f32_t)tp.tv_sec + (f32_t)tp.tv_nsec * 1e-9;

    _re_os_state.processor_count = sysconf(_SC_NPROCESSORS_ONLN);
    _re_os_state.page_size = getpagesize();
}

void re_os_terminate(void) {
}

/*=========================*/
// Platform specific structs
/*=========================*/

struct re_lib_t {
    void *handle;
};

struct re_mutex_t {
    pthread_mutex_t handle;
};

/*=========================*/
// Dynamic library loading
/*=========================*/

re_lib_t *re_lib_load(const char *path, re_lib_mode_t mode) {
    u32_t posix_mode = 0;
    switch (mode) {
        case RE_LIB_MODE_LOCAL:
            posix_mode = RTLD_LOCAL;
            break;
        case RE_LIB_MODE_GLOBAL:
            posix_mode = RTLD_GLOBAL;
            break;
    }

    re_lib_t *lib = re_malloc(sizeof(re_lib_t));
    *lib = (re_lib_t){0};
    lib->handle = dlopen(path, RTLD_LAZY | posix_mode);
    if (lib->handle == NULL) {
        re_free(lib);
        return NULL;
    }
    return lib;
}

void re_lib_unload(re_lib_t *lib) {
    if (lib == NULL) {
        return;
    }

    dlclose(lib->handle);
    lib->handle = NULL;
    re_free(lib);
}

re_func_ptr_t re_lib_func(const re_lib_t *lib, const char *name) {
    RE_ASSERT(lib != NULL, "Loading function '%s' from NULL library.", name);

    re_func_ptr_t ptr;
    *((void **)&ptr) = dlsym(lib->handle, name);

    return ptr;
}

/*=========================*/
// Multithreading
/*=========================*/

// Threads
typedef struct _re_thread_context_t _re_thread_context_t;
struct _re_thread_context_t {
    re_thread_func_t func;
    void *arg;
};

static void *_re_thread_func_cleanup(void *arg) {
    _re_thread_context_t ctx = *(_re_thread_context_t *) arg;
    ctx.func(ctx.arg);
    _re_arena_scratch_destroy();
    return NULL;
}

re_thread_t re_thread_create(re_thread_func_t func, void *arg) {
    re_thread_t thread = {0};

    re_arena_temp_t scratch = re_arena_scratch_get(NULL, 0);
    _re_thread_context_t *ctx = re_arena_push(scratch.arena, sizeof(_re_thread_context_t));
    *ctx = (_re_thread_context_t) {
        .func = func,
        .arg = arg
    };
    pthread_create(
            &thread.handle,
            NULL,
            _re_thread_func_cleanup,
            ctx);
    re_arena_scratch_release(&scratch);

    return thread;
}

void re_thread_destroy(re_thread_t thread) { (void)thread; }

void re_thread_wait(re_thread_t thread) { pthread_join(thread.handle, NULL); }

// Mutexes
re_mutex_t *re_mutex_create(void) {
    re_mutex_t *mutex = re_malloc(sizeof(re_mutex_t));
    mutex->handle = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    return mutex;
}

void re_mutex_destroy(re_mutex_t *mutex) {
    pthread_mutex_destroy(&mutex->handle);
    re_free(mutex);
}

void re_mutex_lock(re_mutex_t *mutex) { pthread_mutex_lock(&mutex->handle); }
void re_mutex_unlock(re_mutex_t *mutex) {
    pthread_mutex_unlock(&mutex->handle);
}

/*=========================*/
// System info
/*=========================*/

f32_t re_os_get_time(void) {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return ((f32_t)tp.tv_sec + (f32_t)tp.tv_nsec * 1e-9) - _re_os_state.start_time;
}

u32_t re_os_get_processor_count(void) {
    return _re_os_state.processor_count;
}

u32_t re_os_get_page_size(void) {
    return _re_os_state.page_size;
}

/*=========================*/
// Memory
/*=========================*/

void *re_os_mem_reserve(usize_t size) {
    return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
}

void re_os_mem_commit(void *ptr, usize_t size) {
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

void re_os_mem_decommit(void *ptr, usize_t size) {
    mprotect(ptr, size, PROT_NONE);
}

void re_os_mem_release(void *ptr, usize_t size) {
    munmap(ptr, size);
}

#endif // RE_OS_LINUX

#ifdef RE_OS_WINDOWS
#endif
