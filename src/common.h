#pragma once

#include <stdint.h>
#include <stdbool.h>

#define kilobytes(x) ((size_t)(x) * 1024ULL)
#define megabytes(x) (kilobytes(x) * 1024ULL)
#define gigabytes(x) (megabytes(x) * 1024ULL)

typedef int8_t b8;
typedef int32_t b32;

#define countof(x) (size_t)(sizeof(x) / sizeof(*(x)))
#define lengthof(x) (countof(x) - 1)

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
