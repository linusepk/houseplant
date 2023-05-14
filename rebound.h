#ifndef REBOUND_H
#define REBOUND_H

/*=========================*/
// Context cracking
/*=========================*/

#if defined(_WIN32)
#define RE_OS_WINDOWS
#endif // _WIN32

#if defined(__linux__)
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
#if !defined(true)
#define true 1
#endif // true
#if !defined(false)
#define false 0
#endif // false

#if !defined(NULL)
#define NULL ((void *) 0)
#endif // NULL

/*=========================*/
// Allocator API
/*=========================*/

typedef struct re_allocator_t re_allocator_t;
struct re_allocator_t {
    void *user_data;
    void *(*alloc)(re_allocator_t *allocator, usize_t size);
    void *(*free)(re_allocator_t *allocator, void *ptr);
};

#endif // REBOUND_H
