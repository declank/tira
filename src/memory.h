#pragma once

#include <stddef.h>
#include <stdint.h>
#include "common.h"

typedef struct Arena Arena;
struct Arena {
    uint8_t* base;
    size_t size;
    size_t used;
    Arena *prev;
};

typedef struct {
    Arena *arena;
    void *start_pos;
} TempArena;

Arena arena_create_ex(size_t size, void* (*alloc_fn)(size_t));

#define kilobytes(x) ((size_t)(x) * 1024ULL)
#define megabytes(x) (kilobytes(x) * 1024ULL)
#define gigabytes(x) (megabytes(x) * 1024ULL)

static size_t arena_default_reserve_size = megabytes(64);
static size_t arena_default_commit_size = kilobytes(64);

void *mem_alloc(size_t size);
void *mem_alloc_code(size_t size);

#define new(arena, type, count) (type *)alloc((arena), sizeof(type), _Alignof(type), count)
#define realloc_array(arena, base, type, count) (type *)realloc_array_((arena), (base), sizeof(type), _Alignof(type), count)
#define mem_zero(ptr) __builtin_memset((ptr), 0, sizeof(*(ptr)))

Arena arena_create(size_t size);
Arena arena_create_code(size_t size);
void arena_reset(Arena* arena);

void *arena_push(Arena* arena, size_t count);
void arena_pop_to(Arena *arena, void *pos);
void arena_pop_bytes(Arena *arena, size_t num_bytes);

TempArena temp_begin(Arena *arena);
void temp_end(TempArena temp);

void *alloc(Arena *a, size_t size, size_t align, size_t count);
void *realloc_array_(Arena *a, void *base, size_t elem_size, size_t align, size_t count);


//inline void *memset(void* dest, int ch, size_t count);
void *memcpy(void *restrict dest, const void *restrict src, size_t count);
void *memzero(void *ptr, size_t count);
void *memset(void* dest, int ch, size_t count);
int memcmp(const void* lhs, const void* rhs, size_t count);
void *memchr(const void *ptr, int ch, size_t count);

