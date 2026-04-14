#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"

typedef struct {
    char *data;
    size_t len;
} String;

typedef struct StringNode StringNode;
struct StringNode {
    StringNode *next;
    String s;
};

typedef struct {
    StringNode *first;
    StringNode *last;
    size_t count;
    size_t cap;
} StringList;

//#define S(x) (String) { .data = (x), .len = (sizeof(x) / sizeof(*(x)))-1 }

#define S(x) \
    (String) { .data = (char*)(x), .len = sizeof(x)-1 }

typedef struct {
    String buffer;
    size_t capacity;
} StringBuilder;

#define sb_build(builder, x) _Generic((x), String: sb_build_string, \
				           char:        sb_build_char, \
                           uint64_t:    sb_build_u64, \
				           int:         sb_build_int)(builder, x)
#define sb_char(builder, c) sb_build_char((builder), (c))

size_t strlen(const char* str);
int strcmp(const char *lhs, const char *rhs);
StringBuilder sb_create_temp(Arena arena);
void sb_newline(StringBuilder *sb);
void sb_build_string(StringBuilder *sb, String s);
void sb_build_char(StringBuilder *sb, char c);
void sb_build_u64(StringBuilder *sb, uint64_t u);
void sb_build_int(StringBuilder *sb, int i);
void print_sb(StringBuilder sb);

bool string_to_double(String str, double *out);
bool string_to_int64(String str, int64_t *out);

int string_compare(String lhs, String rhs);
bool string_equal(String lhs, String rhs);

