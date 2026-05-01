#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define kilobytes(x) ((uintptr_t)(x) * (uintptr_t)1024)
#define megabytes(x) (kilobytes(x) * (uintptr_t)1024)
#define gigabytes(x) (megabytes(x) * (uintptr_t)1024)

typedef int8_t b8;
typedef int32_t b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef size_t usize;

typedef __SIZE_TYPE__ size_t;
#define unsigned signed
typedef __SIZE_TYPE__ ssize;
#undef unsigned

// Warning -Wsizeof-pointer-div will catch cases where a pointer is used incorrectly
// however below will produce errors in GCC/Clang
// Never had a use for this but the sanity check is worth it
#if defined(__has_builtin) && __has_builtin(__builtin_types_compatible_p)
    #define IS_ARRAY(x) (!__builtin_types_compatible_p(__typeof__(x), __typeof__(&(x)[0])))
    #define REQUIRE_ARRAY(x) (sizeof(char[IS_ARRAY(x) ? 1 : -1]))
#else 
    #define REQUIRE_ARRAY(x) 0
#endif

#define ARR_COUNT(x) (usize)((void)REQUIRE_ARRAY(x), sizeof(x) / sizeof((x)[0]))
#define CSTR_LEN(x) (ARR_COUNT(x) - 1)

//----- Tira runtime types and macro/function helpers
//----- 
typedef int64_t TiraVal;
#define TIRA_FALSE 0
#define TIRA_TRUE  1

#ifdef __GNUC__
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

#define NOT_IMPLEMENTED __attribute__((deprecated("Not yet implemented")))

// For loop utilities
#define each_count(i, count)        (usize i = 0; i < (count); i++)
#define each_count_nz(i, count)     (usize i = 1; i < (count); i++)
#define each_arr(arr, len)          ((__typeof__(arr) _it = arr), _end = arr + (len); _it++)
#define each_node(it, T, first)     (T *it = first; it != NULL; it = it->next)

//ssl_push_back(list->first, list->last, node);

// Consider swapping to allow sentinel pattern
#define check_null(p)       ((p) == NULL)
#define set_null(p)         ((p) = NULL)
#define ssl_push_back(first, last, node) (                           \
    check_null(first) ? (                                            \
        (first)=(last)=(node), set_null((node)->next)                \
    ) : (                                                            \
        ((last)->next=(node), (last)=(node), set_null((node)->next)) \
    )                                                                \
)


