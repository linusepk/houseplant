#include "rebound.h"

#ifdef RE_OS_LINUX
#include <dlfcn.h>
#include <sys/mman.h>
#include <pthread.h>
#endif

#include <stdlib.h>

//  ____                   _
// | __ )  __ _ ___  ___  | |    __ _ _   _  ___ _ __
// |  _ \ / _` / __|/ _ \ | |   / _` | | | |/ _ \ '__|
// | |_) | (_| \__ \  __/ | |__| (_| | |_| |  __/ |
// |____/ \__,_|___/\___| |_____\__,_|\__, |\___|_|
//                                    |___/
// Base layer

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

void re_free(void *ptr) {
    RE_FREE(ptr);
}

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

void re_format_string(char buffer[1024], const char *fmt, ...)  {
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);
}

/*=========================*/
// Hash table
/*=========================*/

re_ht_iter_t __re_ht_iter_next(usize_t start, void *entries, usize_t entry_count, usize_t alive_offset, usize_t stride) {
    for (usize_t i = start; i < entry_count; i++) {
        u8_t *entry = (u8_t *) entries + i * stride;
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

    void *new_head = re_realloc(head, sizeof(_re_da_header_t) + head->size * head->cap);
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
        memcpy(dest, output, head->size);
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

static _re_logger_t _re_logger = { .level = RE_LOG_LEVEL_TRACE };
static const char *_re_log_level_string[RE_LOG_LEVEL_COUNT] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};


static void _re_log_stdout_callback(re_log_event_t *const event) {
    FILE *fp = event->level < RE_LOG_LEVEL_WARN ? stdout : stderr;

#ifndef RE_LOG_NO_COLOR
    static const char *level_color[RE_LOG_LEVEL_COUNT] = {
        "\033[1;101m",
        "\033[1;91m",
        "\033[0;93m",
        "\033[0;92m",
        "\033[0;94m",
        "\033[0;95m"
    };

    u32_t i = 0;
    while (event->message[i] != '\0') {
        fprintf(fp,
                "\033[2;37m%.2d:%.2d:%.2d\033[0m %s%-5s\033[0m \033[2;37m%s:%d: \033[0m",
                event->time.hour, event->time.min, event->time.sec,
                level_color[event->level],
                _re_log_level_string[event->level],
                event->file,
                event->line);

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
        fprintf(fp,
                "%.2d:%.2d:%.2d %-5s %s:%d: ",
                event->time.hour, event->time.min, event->time.sec,
                _re_log_level_string[event->level],
                event->file,
                event->line);

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
        fprintf(event->user_data,
                "%.2d:%.2d:%.2d %-5s %s:%d: ",
                event->time.hour, event->time.min, event->time.sec,
                _re_log_level_string[event->level],
                event->file,
                event->line);

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


void _re_log(
        const char *file,
        i32_t line,
        re_log_level_t level,
        const char *fmt,
        ...) {
    isize_t t = time(NULL);
    struct tm *tm = localtime(&t);

    re_log_event_t event = {
        .file = file,
        .line = line,
        .level = level,
        .time = {
            .hour = tm->tm_hour,
            .min = tm->tm_min,
            .sec = tm->tm_sec,
        }
    };

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

void re_logger_add_callback(
        re_log_callback_t callback,
        re_log_level_t level,
        void *user_data) {
    RE_ASSERT(_re_logger.callback_i < RE_LOGGER_CALLBACK_MAX,
            "Can't surpace %d logger callbacks", RE_LOGGER_CALLBACK_MAX);

    _re_logger.callbacks[_re_logger.callback_i++] = (_re_logger_callback_t) {
        .func = callback,
        .level = level,
        .user_data = user_data
    };
}

void re_logger_add_fp(FILE *fp, re_log_level_t level) {
    re_logger_add_callback(_re_log_file_callback, level, fp);
}

void re_logger_set_silent(b8_t silent) {
    _re_logger.silent = silent;
}

void re_logger_set_level(re_log_level_t level) {
    _re_logger.level = level;
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
    b8_t valid;
};

struct re_mutex_t {
    pthread_mutex_t handle;
};

/*=========================*/
// Dynamic library loading
/*=========================*/

re_lib_t *re_lib_load(const char *path) {
    re_lib_t *lib = re_malloc(sizeof(re_lib_t));
    *lib = (re_lib_t) {0};
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
    re_free(lib);
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
re_mutex_t *re_mutex_create(void) {
    re_mutex_t *mutex = re_malloc(sizeof(re_mutex_t));
    mutex->handle = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    return mutex;
}

void re_mutex_destroy(re_mutex_t *mutex) {
    pthread_mutex_destroy(&mutex->handle);
    re_free(mutex);
}

void re_mutex_lock(re_mutex_t *mutex)   { pthread_mutex_lock(&mutex->handle); }
void re_mutex_unlock(re_mutex_t *mutex) { pthread_mutex_unlock(&mutex->handle); }

#endif // RE_OS_LINUX
