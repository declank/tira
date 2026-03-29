

#include "string.h"

size_t strlen(const char* str) {
    const char *s = str;
    while (*s) ++s;
    return (size_t)(s - str);
}

int string_compare(String lhs, String rhs) {
    for (size_t i = 0; *lhs.data == *rhs.data && (i< (lhs.len-1) && (i<rhs.len-1)); lhs.data++, rhs.data++, i++);
    uint8_t ret_val = (uint8_t)*lhs.data - (uint8_t)*rhs.data; 
    return ret_val;
}

int strcmp(const char *lhs, const char *rhs) {
    for (; *lhs == *rhs && *lhs; lhs++, rhs++);
    return *(uint8_t*)lhs - *(uint8_t*)rhs;
}

StringBuilder sb_create_temp(Arena arena) {
    StringBuilder sb = {0};
    if ((sb.buffer.data = arena_push(&arena, kilobytes(128))) != NULL) { sb.capacity = kilobytes(128); sb.buffer.len = 0; }
    return sb;
}

static inline size_t sb_remaining(const StringBuilder *sb) {
    return sb->capacity - sb->buffer.len;
}

void sb_newline(StringBuilder *sb) {
    //sb_build_char(sb, '\r');
    sb_build_char(sb, '\n');
}

void sb_build_string(StringBuilder *sb, String s) {
    size_t remaining = sb_remaining(sb);
    if (s.len > remaining) return;

    if (memcpy(sb->buffer.data + sb->buffer.len, s.data, s.len) != NULL) {
        sb->buffer.len += s.len;
    }    
}

void sb_build_char(StringBuilder *sb, char c) {
    if (sb->buffer.len >= sb->capacity - 1) return;

    sb->buffer.data[sb->buffer.len++] = c;
}

void sb_build_int(StringBuilder *sb, int i) {
    size_t remaining = sb_remaining(sb);

    if (i == 0 && remaining > 0) {
        sb->buffer.data[sb->buffer.len++] = '0';
        return;
    }

    uint64_t u = i;
    if (i < 0) {
        if (remaining <= 1) return;
        sb->buffer.data[sb->buffer.len++] = '-';
        remaining--;

        // Handle INT64_MIN
        u = (uint64_t)(-(i + 1)) + 1;
    }

    sb_build_u64(sb, u);
}



void sb_build_u64(StringBuilder *sb, uint64_t u) {
    size_t remaining = sb_remaining(sb);

    if (u == 0 && remaining > 0) {
        sb->buffer.data[sb->buffer.len++] = '0';
        return;
    }

    char temp[16];
    int ti = 0;

    while (u != 0) {
        temp[ti++] = (u % 10) + '0';
        u /= 10;
    }

    if (ti >= remaining) return;
    while (ti--) { sb->buffer.data[sb->buffer.len++] = temp[ti]; }
}

bool string_to_double(String str, double *out) {
    if (!str.len) return false; // TODO check that this is unsigned

    double result = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    
    bool negative = false;
    bool in_fraction = false;
    size_t i = 0;

    if (str.data[i] == '-') { negative = true; i++; }
    else if (str.data[i] == '+') { i++; }

    for (; i < str.len; i++) {
        char c = str.data[i];
        if (c == '.') {
            if (in_fraction) return false; // Invalid to have more than one '.'
            in_fraction = true;
            continue;
        }

        if (c < '0' || c > '9') return false; // TODO: implement scientific notation
        if (in_fraction) {
            divisor *= 10.0;
            fraction += (c - '0') / divisor;
        } else {
            result = result * 10.0 + (c - '0');
        }
    }

    *out = negative ? -(result + fraction) : (result + fraction);
    return true;
    
}

bool string_to_int64(String str, int64_t *out) {
    if (!str.len) return false; // TODO check that this is unsigned

    int64_t result = 0;

    bool negative = false;
    size_t i = 0;

    if (str.data[i] == '-') { negative = true; i++; }
    else if (str.data[i] == '+') { i++; }

    // TODO test entering -0x0F
    // Hex number case
    char c;
    if (str.len > 2 && str.data[0] == '0' && (str.data[1] == 'x' || str.data[1] == 'X')) {
        i = 2;
        for (; i < str.len; i++) {
            c = str.data[i];
            int64_t digit;

            // is this more optimal with LUT?
            if(0);
            else if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else return false;

            // Overflow check
            if (result > (INT64_MAX - digit) / 16) return false;
            result = result * 16 + digit;
        }

        *out = result;
        return true;
    }

    // Decimal case
    if (i == str.len) return false;
    for (; i < str.len; i++) {
        c = str.data[i];
        if (c < '0' || c > '9') return false;

        // Overflow check
        if (result > (INT64_MAX - (c - '0')) / 10) return false;
        result = result * 10 + (c - '0');
    }

    *out = negative ? -result : result;
    return true;
}

