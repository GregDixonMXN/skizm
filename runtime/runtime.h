#ifndef RUNTIME_H
#define RUNTIME_H

#include "arena.h"
#include "object.h"
#include "dispatch.h"

void runtime_init(size_t arena_size);
void runtime_shutdown(void);
void runtime_reset(void);
size_t runtime_memory_used(void);
size_t runtime_memory_remaining(void);

#define System obj_nil()

/* ADD THIS: Bridges your 'Array new' code to the C engine */
static inline Object *Array_new(Object *self, Object **args, int argc) {
    (void)self; (void)args; (void)argc;
    /* Create an array with a default capacity of 128 units */
    return obj_array(128); 
}

#endif /* RUNTIME_H */