#ifndef UTIL_H_
#define UTIL_H_

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef size_t usize;
typedef ptrdiff_t isize;
typedef i8 b8;
typedef i32 b32;

#if (defined(__GNUC__) || defined(__clang__))
#  define LIKELY(EXPR) __builtin_expect((EXPR), 1)
#  define UNLIKELY(EXPR) __builtin_expect((EXPR), 0)
#else
#  define LIKELY(EXPR) (EXPR)
#  define UNLIKELY(EXPR) (EXPR)
#endif // (defined(__GNUC__) || defined(__clang__))

// TODO: MSVC version of notreached
#if (defined(__GNUC__) || defined(__clang__))
#  define NOTREACHED __builtin_unreachable()
#else
#  define NOTREACHED ((void)0)
#endif

// TODO: MSVC version of noinline
#if (defined(__GNUC__) || defined(__clang__))
#  define NOINLINE __attribute__((noinline))
#elif (defined(_MSC_VER))
#  define NOINLINE __declspec(noinline)
#else
#  define NOINLINE
#endif

#if (defined(__GNUC__) || defined(__clang__))
#  define PRINTF_FORMAT_FUNC(N, M) __attribute__((__format__ (__printf__, N, M)))
#else
#  define PRINTF_FORMAT_FUNC(N, M)
#endif

// TODO: For msvc: __declspec(noreturn)
#if (__STDC_VERSION__ >= 201112L)
#  define NORETURN _Noreturn
#elif (defined(__GNUC__) || defined(__clang__))
#  define NORETURN __attribute__((noreturn))
#else
#  define NORETURN
#endif

#define ARRAY_COUNT(ARR) sizeof(ARR) / sizeof((ARR)[0])

PRINTF_FORMAT_FUNC(4, 5) NOINLINE static void
cwarn(char const* fname, int row, int col, char const* fmt, ...)
{
    va_list args;

    fflush(stdout);
    fprintf(stderr, "%s:%d:%d: warning: ", fname, row, col);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

PRINTF_FORMAT_FUNC(4, 5) NORETURN NOINLINE static void
cerror(char const* fname, int row, int col, char const* fmt, ...)
{
    va_list args;

    fflush(stdout);
    fprintf(stderr, "%s:%d:%d: error: ", fname, row, col);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(1);
}

PRINTF_FORMAT_FUNC(1, 2) NORETURN NOINLINE static void
fatal(char* fmt, ...)
{
    va_list args;

    fflush(stdout);
    fprintf(stderr, "fatal: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");

    exit(2);
}

// TODO: Proper debug/release assert
#define ASSERT assert

#endif // UTIL_H_
