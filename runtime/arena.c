/*
 * arena.c - Arena-Based Memory Allocator Implementation
 */

#include "arena.h"
#include <stdlib.h>
#include <string.h>

/* Align size up to 8-byte boundary for safe struct access */
static size_t align_up(size_t size) {
    return (size + 7) & ~((size_t)7);
}

Arena arena_create(size_t capacity) {
    Arena arena;
    arena.buffer = (uint8_t *)malloc(capacity);
    arena.capacity = arena.buffer ? capacity : 0;
    arena.offset = 0;
    return arena;
}

void *arena_alloc(Arena *arena, size_t size) {
    size_t aligned = align_up(size);
    
    if (arena->offset + aligned > arena->capacity) {
        return NULL;  /* Out of memory - caller decides what to do */
    }
    
    void *ptr = arena->buffer + arena->offset;
    arena->offset += aligned;
    
    /* Zero the memory for safety */
    memset(ptr, 0, aligned);
    
    return ptr;
}

void arena_reset(Arena *arena) {
    arena->offset = 0;
    /* No freeing, no walking, just instant reset */
}

void arena_destroy(Arena *arena) {
    free(arena->buffer);
    arena->buffer = NULL;
    arena->capacity = 0;
    arena->offset = 0;
}

size_t arena_remaining(Arena *arena) {
    return arena->capacity - arena->offset;
}
