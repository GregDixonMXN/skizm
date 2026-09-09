/*
 * arena.h - Arena-Based Memory Allocator
 * 
 * Philosophy: No garbage collector. Allocate fast, wipe everything at once.
 * Perfect for games: allocate during a frame, reset at frame end.
 */

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct Arena {
    uint8_t *buffer;      /* The raw memory block */
    size_t   capacity;    /* Total size in bytes */
    size_t   offset;      /* Current allocation position */
} Arena;

/* Create an arena with the given capacity (bytes) */
Arena arena_create(size_t capacity);

/* Allocate 'size' bytes, aligned to 8-byte boundary. Returns NULL if full. */
void *arena_alloc(Arena *arena, size_t size);

/* Reset the arena - all allocations become invalid, memory reused instantly */
void arena_reset(Arena *arena);

/* Free the arena's underlying buffer */
void arena_destroy(Arena *arena);

/* Query how much space remains */
size_t arena_remaining(Arena *arena);

#endif /* ARENA_H */
