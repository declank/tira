

#include "memory.h"
#include "platform.h"
#include <stdint.h>

Arena arena_create(size_t size) {
    return arena_create_ex(size, mem_alloc);
}

Arena arena_create_code(size_t size) {
    return arena_create_ex(size, mem_alloc_code);
}

void *arena_push(Arena* arena, size_t count) {
    if (arena->used + count > arena->size) return NULL;

    void *ptr = arena->base + arena->used;
    arena->used += count;

    return ptr;
}

Arena arena_create_ex(size_t size, void* (*alloc_fn)(size_t)) {    
    Arena a = {0};
    void *mem = alloc_fn(size);
    if (!mem) {
        //error("Memory allocation failed.");
        return a;
    }

    a.base = (uint8_t*) mem;
    a.size = size;
    a.used = 0;
    return a;
}


void *alloc(Arena *a, size_t size, size_t align, size_t count) {
    if (!a || !a->base || size == 0 || align == 0 || count == 0) return NULL;

    size_t total = size * count;
    uintptr_t base = (uintptr_t)a->base + a->used;
    uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t new_used = (aligned - (uintptr_t)a->base) + total;

    if (new_used > a->size) { // Arena out of memory
        error("Arena out of memory\n");
        //__asm__ volatile("int3");
        exit(1);
        return NULL;
    }
    a->used = new_used;
    return (void *) aligned;
}

static inline size_t next_capacity(size_t count) {
    //return count < 8 ? 8 : count * 2;
    return count ? count * 2 : 1;
}

#if 0
void *realloc_array_(Arena *a, void *base, size_t elem_size, size_t align, size_t count) {
    uint8_t *arena_top = a->base + a->used;
    uint8_t *array_end = (uint8_t *)base + elem_size * count;
    b8 at_top = base && array_end == arena_top;

    if (at_top) {
        size_t current = (array_end - (uint8_t*)base) / elem_size;
        if (count > current) alloc(a, elem_size, align, next_capacity(current) - current);
        return base;
    }

    size_t capacity = next_capacity(count);
    void *new_ptr = alloc(a, elem_size, align, capacity);
    if (base != NULL) memcpy(new_ptr, base, elem_size * count);
    return new_ptr;
}
#endif

static inline uintptr_t align_up_pow2(uintptr_t val, uintptr_t align) {
    return (val + align - 1) & ~(align - 1);
}

static inline uintptr_t align_down_pow2(uintptr_t val, uintptr_t align) {
    return val & ~(align - 1);
}

// TODO remove this performance counter
static uint64_t realloc_moves = 0;

void *realloc_array_(Arena *a, void *base, size_t elem_size, size_t align, size_t old_count, size_t new_count) {
    uint8_t *arena_top = a->base + a->used;
    uint8_t *array_end = (uint8_t *)base + elem_size * old_count;
    b8 at_top = (base != NULL) && (array_end == arena_top);

    if (at_top) {
        size_t current = (array_end - (uint8_t*)base) / elem_size;
        if (new_count > current) alloc(a, elem_size, align, new_count - current);
        return base;
    }

    realloc_moves++; // Increment the counter
    void *new_ptr = alloc(a, elem_size, align, new_count);

    // Initialize these for debugging convienience
    uintptr_t page_start = 0;
    uintptr_t page_end   = 0;

    if (base != NULL) {
        memcpy(new_ptr, base, elem_size * old_count);

        // Release pages from old allocation
        page_start = align_up_pow2((uintptr_t) base, 4096);
        page_end   = align_down_pow2((uintptr_t) array_end, 4096);

        if (page_start < page_end)
            mem_dont_need((void *)page_start, page_end - page_start);
    }

    return new_ptr;
}

static void dyn_array_maybe_grow(Arena *a, DynArray *arr, size_t elem_size, size_t align) {
    if (arr->count < arr->cap) return;

    uint8_t *arena_top = a->base + a->used;
    uint8_t *array_end = (uint8_t *)arr->ptr + elem_size * arr->cap;
    b8 at_top = arr->ptr && array_end == arena_top;

    size_t new_cap = next_capacity(arr->cap);

    if (at_top) {
        alloc(a, elem_size, align, new_cap - arr->cap);
        arr->cap = new_cap;
        return;
    }

    void *new_ptr = alloc(a, elem_size, align, new_cap);
    if (arr->ptr != NULL) {
        memcpy(new_ptr, arr->ptr, elem_size * arr->cap);

        uintptr_t page_start = align_up_pow2((uintptr_t)arr->ptr, 4096);
        uintptr_t page_end   = align_down_pow2((uintptr_t)array_end, 4096);

        if (page_start < page_end)
            mem_dont_need((void *)page_start, page_end - page_start);
    }

    arr->ptr = new_ptr;
    arr->cap = new_cap;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (count--) *d++ = *s++;
    return dest;
}

void* memzero(void *ptr, size_t count) {
  return memset(ptr, 0, count);
}

void *memset(void* dest, int ch, size_t count) {
    // TODO Alignment/SIMD optimisation or import from CRT
    uint8_t* ptr = dest;
    for (size_t i = 0; i < count; i++) {
        ptr[i] = (uint8_t)ch;
    }
    return ptr;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
    // TODO Alignment/SIMD optimisation or import from CRT
    const unsigned char* a = (const unsigned char*)lhs;
    const unsigned char* b = (const unsigned char*)rhs;

    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return (int)(a[i] - b[i]);
        }
    }

    return 0;
}

void *memchr(const void *ptr, int ch, size_t count) {
    const uint8_t *p = ptr;

    for (size_t i = 0; i < count; i++) {
        if (p[i] == (uint8_t)ch) {
            return (void *)(p + i);
        }
    }

    return NULL;
}

