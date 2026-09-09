/*
 * runtime.c - Runtime Initialization and Management
 */

#include "runtime.h"
#include <stdio.h>

static Arena main_arena;

void runtime_init(size_t arena_size) {
    /* Create the main arena */
    main_arena = arena_create(arena_size);
    if (main_arena.buffer == NULL) {
        fprintf(stderr, "Failed to allocate %zu bytes for arena!\n", arena_size);
        return;
    }
    
    /* Set the global arena pointer */
    g_arena = &main_arena;
    
    /* Initialize the dispatch system */
    dispatch_init();
    
    /* Register built-in types and methods */
    register_builtins();
    
    printf("Runtime initialized with %zu KB arena\n", arena_size / 1024);
}

void runtime_shutdown(void) {
    arena_destroy(&main_arena);
    g_arena = NULL;
    printf("Runtime shut down.\n");
}

void runtime_reset(void) {
    arena_reset(&main_arena);
}

size_t runtime_memory_used(void) {
    return main_arena.offset;
}

size_t runtime_memory_remaining(void) {
    return arena_remaining(&main_arena);
}
