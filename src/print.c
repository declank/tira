
#include <stdarg.h>

#include "platform.h"


int vsnprintf(char *buffer, size_t bufsz,  const char *format, va_list vlist);
int error(const char *fmt, ...);

int printf(const char* restrict format, ...);

#define VSNPRINTF_PUTC          do { if (rem > 1) { (*buffer++) = *format++; rem--; } } while(0)
#define VSNPRINTF_DIV_OUT       
#define VSNPRINTF_OUT_DIGIT     

/* int vsnprintf_putc(char *buffer, const char **format, size_t rem, char c) {
    if (rem > 1) { *buffer++ = *(*format++); rem--; }
}*/

// TODO add back in restrict on buffer and format
int vsnprintf(char *buffer, size_t bufsz,
              const char *format, va_list vlist) {
    char *start = buffer;   
    if (bufsz == 0) buffer = NULL;
    size_t rem = bufsz;
    bool enforce_sign = false; 

    while (*format) {
        //char c = *format;
        if (*format != '%') {
            if (rem > 1) { *buffer++ = *format++; rem--; }
            //format++;
            continue;
        }

        format++; // skip the %

        if (*format == '%') {
            if (rem > 1) { *buffer++ = *format++; rem--; }
            continue;
        }

        if (*format == '+') {
            enforce_sign = true;
            format++;
        }

        if (*format == 's') {
            const char *s = va_arg(vlist, const char *);
            //if (!s) s = "(null)"; // what LOL?
            if (!s) return -1;
            while (*s) { // TODO where be strcpy
                if (rem > 1) { (*buffer++) = *s++; rem--; }
            }
            format++; continue;
        }

        if (*format == 'S') {
            String s = va_arg(vlist, String);
            if (!s.data) return -1;
            while (s.len--) {
                if (rem > 1) { (*buffer++) = *s.data++; rem--; }
            }

        }

        // TODO factor out sizing
        if (*format == 'z' && format[1] == 'u') {
            size_t v = va_arg(vlist, size_t);

            if (enforce_sign) { *buffer++ = '+'; rem--; }
            
            char tmp[32];
            if (v == 0) { 
                if (rem > 1) { *buffer++ = '0'; rem--; }
                format += 2; continue;
            }

            size_t i = 0;
            while (v > 0) { tmp[i++] = '0' + v % 10; v /= 10; }

            if (rem > i + 1) {
                rem -= i;
                while (i--) { *buffer++ = tmp[i]; }
            }

            format += 2;
            continue;
        }

        if (*format == 'd') {
            intptr_t v = va_arg(vlist, int); // TODO @Cleanup Note value is extended here (would need to then ensure INT64_MIN works correctly)
            bool negative = false;
            char tmp[16];
            size_t i = 0;

            if (v == 0) {
                if (enforce_sign && rem > 2) { *buffer++ = '+'; rem--; }

                if (rem > 1) { *buffer++ = '0'; rem--; }
                format++; continue;
            }

            if (v < 0) {
                negative = true;
                v = -v; // TODO Check does this work with INT_MIN
            }

            while (v > 0) { tmp[i++] = '0' + v % 10; v /= 10; }
            if (negative) { tmp[i++] = '-'; }
            else if (enforce_sign) { tmp[i++] = '+'; }
            if (rem > i + 1) {
                rem -= i;
                while (i--) { *buffer++ = tmp[i]; }
            }

            format++;
            continue;
        }

        // TODO handle outputing other cases e.g. %a or erroring for unhandled
        // ...
        format++;
    }

    /* if (bufsz > 0) {
        if (rem > 0) {
            *buffer = '\0';
        } else { 
            start[bufsz - 1] = '\0';
        }
    } */

    size_t written = 0;
    if (bufsz > 0) {
        written = bufsz - rem;

        if (written < bufsz) start[written] = '\0';
        else start[bufsz - 1] = '\0'; // null terminate in the case where buffer is full
    }

    return written;
}

int tira_error(const char *fmt, ...) {
    char buf[1024];

    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        console_error(buf, len);
    }

    return len;
}

int printf(const char* restrict format, ...) {
    char buf[1024];

    va_list ap;
    va_start(ap, format);
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if (len > 0) {
        console_out(buf, len);
    }

    return len;
}

