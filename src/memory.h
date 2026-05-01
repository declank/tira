#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct Arena Arena;
struct Arena {
    uint8_t* base;
    usize size;
    usize used;
    Arena *prev;
};

typedef struct {
    Arena *arena;
    void *start_pos;
} TempArena;

typedef struct {
    void *data;
    usize count;
    usize cap;
} DynArray;

#define array_push(arena, arr, type) \
    (dyn_array_maybe_grow((arena), (arr), sizeof(type), __alignof(type)), \
    &((type*)(arr)->data[(arr)->count++]))

static void dyn_array_maybe_grow(Arena *a, DynArray *arr, usize elem_size, usize align);

Arena arena_create_ex(usize size, void* (*alloc_fn)(usize));

static usize arena_default_reserve_size = megabytes(64);
static usize arena_default_commit_size = kilobytes(64);

void *mem_alloc(usize size);
void *mem_alloc_code(usize size);

#define new(arena, type, count) (type *)alloc((arena), sizeof(type), __alignof(type), count)
#define realloc_array(arena, base, type, old_count, new_count) \
    (type *)realloc_array_((arena), (base), sizeof(type), __alignof(type), old_count, new_count)
#define mem_zero(ptr) __builtin_memset((ptr), 0, sizeof(*(ptr)))

Arena arena_create(usize size);
Arena arena_create_code(usize size);
void arena_reset(Arena* arena);

void *arena_push(Arena* arena, usize count);
void arena_pop_to(Arena *arena, void *pos);
void arena_pop_bytes(Arena *arena, usize num_bytes);

TempArena temp_begin(Arena *arena);
void temp_end(TempArena temp);

void *alloc(Arena *a, usize size, usize align, usize count);
void *realloc_array_(Arena *a, void *base, usize elem_size, usize align, usize old_count, usize new_count);


//inline void *memset(void* dest, int ch, usize count);
void *memcpy(void * restrict dest, const void *restrict src, usize count);
void *memzero(void *ptr, usize count);
void *memset(void* dest, int ch, usize count);
int memcmp(const void* lhs, const void* rhs, usize count);
void *memchr(const void *ptr, int ch, usize count);

static inline usize next_capacity(usize count);


