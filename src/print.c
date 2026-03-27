



int vsnprintf(char *buffer, size_t bufsz,  const char *format, va_list vlist);
int error(const char *fmt, ...);

int printf(const char* restrict format, ...);

#define VSNPRINTF_PUTC do { if (rem > 1) { (*buffer++) = *format++; rem--; } } while(0)


// TODO add back in restrict on buffer and format
int vsnprintf(char *buffer, size_t bufsz,
              const char *format, va_list vlist) {
    char *start = buffer;   
    if (bufsz == 0) buffer = NULL;
    size_t rem = bufsz;
    

    while (*format) {
        //char c = *format;
        if (*format != '%') {
            if (rem > 1) { *buffer++ = *format++; rem--; }
            continue;
        }

        format++; // skip the %

        if (*format == '%') {
            if (rem > 1) { *buffer++ = *format++; rem--; }
            continue;
        }

        if (*format == 's') {
            const char *s = va_arg(vlist, const char *);
            if (!s) s = "(null)";
            while (*s) {
                if (rem > 1) { (*buffer++) = *s++; rem--; }
            }
            format++; continue;
        }

        if (*format == 'z' && format[1] == 'u') {
            size_t v = va_arg(vlist, size_t);
            
            char tmp[32];
            size_t i = 0;
            if (v == 0) { 
                if (rem > 1) { *buffer++ = '0'; rem--; }
                format += 2; continue;
            }

            while (v > 0) { tmp[i++] = '0' + v % 10; v /= 10; }
            if (rem > i + 1) {
                rem -= i;
                while (i--) { *buffer++ = tmp[i]; }
            }

            format += 2;
            continue;
        }

        // TODO handle outputing other cases e.g. %a or erroring for unhandled
        // ...
        format++;
    }

    if (bufsz > 0) {
        if (rem > 0) {
            *buffer = '\0';
        } else { 
            start[bufsz - 1] = '\0';
        }
    }

    return bufsz - rem;
}

int error(const char *fmt, ...) {
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
