#pragma once

#include "string.h"

typedef String FileBuf;

typedef struct {
    int64_t seconds;
    int64_t ns;
} Time;

Time clock_time_monotonic_raw(void);

int console_error(const char* error, size_t length);
int console_out(const char *output, size_t length);
int console_read(char *buffer, size_t bufsz);

void *mem_alloc(size_t size);
void *mem_alloc_code(size_t size);
int mem_dont_need(void *addr, size_t size);
int platform_read_entire_file(FileBuf *buf, String path, Arena *arena);

int open_file(const char *filename);
int write_to_file(int fd, char *buf, size_t buf_size);
int close_file(int fd);

void print(String s);
void print_char(char c);
void print_cstr(const char *str);

int getchar(void);
void exit(int status);

int tira_error(const char *fmt, ...);

