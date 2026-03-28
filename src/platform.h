#pragma once

#include "string.h"

typedef String FileBuf;

int console_error(const char* error, size_t length);
int console_out(const char *output, size_t length);
int console_read(char *buffer, size_t bufsz);

void *mem_alloc(size_t size);
void *mem_alloc_code(size_t size);
FileBuf platform_read_entire_file(String path, Arena *arena);

void print(String s);
void print_char(char c);
void print_cstr(const char *str);

int getchar(void);
void exit(int status);

