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

