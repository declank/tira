#pragma once

#include "string.h"

typedef String FileBuf;

typedef struct {
    int64_t seconds;
    int64_t ns;
} Time;

Time clock_time_monotonic_raw(void);

int console_error(const char* error, usize length);
int console_out(const char *output, usize length);
int console_read(char *buffer, usize bufsz);

void *mem_alloc(usize size);
void *mem_alloc_code(usize size);
int mem_dont_need(void *addr, usize size);
int platform_read_entire_file(FileBuf *buf, String path, Arena *arena);

int open_file(const char *filename);
int write_to_file(int fd, char *buf, usize buf_size);
int close_file(int fd);

void print(String s);
void print_char(char c);
void print_cstr(const char *str);

int getchar(void);
void exit(int status);

int tira_error(const char *fmt, ...);

int os_get_logical_cores(void);

typedef struct {
    int    tid;       
    void  *stack;     // so we can munmap later
} Thread;

//typedef int (*thread_fn)(void *);
typedef void (*thread_fn)(void *);

int thread_spawn(Thread *t, thread_fn fn, void *arg);
void thread_join(Thread *t);

static int threads_spawn(Thread *threads, usize count, thread_fn fn, void *arg) {
    for each_count(i, count) {
        int ret = thread_spawn(&threads[i], fn, arg);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static void threads_join(Thread *threads, usize count) {
    for each_count(i, count) {
        thread_join(&threads[i]);
    }
}


