#pragma once

#include <stdarg.h>

#include "platform.h"

#define VSNPRINTF_PUTC do { if (rem > 1) { (*buffer++) = *format++; rem--; } } while(0)

int vsnprintf(char *buffer, usize bufsz,  const char *format, va_list vlist);
int error(const char *fmt, ...);

int printf(const char* restrict format, ...);

