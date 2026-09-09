/*
 * object.h - The Universal Object System
 * 
 * Philosophy: "Everything is an Object."
 * 
 * Every value in Skizm—integers, booleans, strings, players, 
 * game loops—is represented by this single type. Objects receive messages.
 * 
 * Implementation Strategy:
 * - Tagged Pointers: Small integers (up to 62 bits) are encoded directly
 *   in the pointer itself. No allocation needed for most game numbers.
 * - Heap Objects: Larger values get allocated in the Arena.
 * 
 * Tagged Pointer Scheme (64-bit):
 *   If bit 0 is 1: It's a small integer. Shift right 1 to get the value.
 *   If bit 0 is 0: It's a pointer to a heap-allocated Object.
 */

#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include "arena.h"

/* Forward declaration */
typedef struct Object Object;

/*
 * TypeID - Identifies what kind of object this is.
 * The dispatcher uses this to find the right method.
 */
typedef enum TypeID {
    TYPE_NIL = 0,
    TYPE_BOOL,
    TYPE_INT,        /* For heap-allocated big integers if needed */
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_OBJECT,     /* User-defined class instance */
    TYPE_CLASS,      /* Class object itself (for reflection) */
    TYPE_METHOD,     /* Bound method / closure */
    TYPE_MAX
} TypeID;

/*
 * ClassID - Identifies which user-defined class an object belongs to.
 * Built-in types use CLASS_BUILTIN.
 * User classes get IDs starting from CLASS_USER.
 */
typedef uint32_t ClassID;
#define CLASS_BUILTIN  0
#define CLASS_USER     1000  /* First user-defined class ID */

/*
 * Object - The "God Struct"
 * 
 * Every heap-allocated value is this struct. The 'data' union holds
 * the actual payload depending on 'type'.
 */
struct Object {
    TypeID   type;       /* What kind of data is in the union */
    ClassID  class_id;   /* Which class (for TYPE_OBJECT) */
    uint32_t flags;      /* Reserved for future use (immutability, etc.) */
    
    union {
        bool         as_bool;
        int64_t      as_int;
        double       as_float;
        
        struct {             /* String data */
            char    *chars;
            uint32_t length;
        } as_string;
        
        struct {             /* Array data */
            Object **items;
            uint32_t length;
            uint32_t capacity;
        } as_array;
        
        struct {             /* User object instance */
            Object **fields;     /* Array of field values */
            uint32_t field_count;
        } as_instance;
        
        struct {             /* Class definition */
            const char *name;
            uint32_t    field_count;
            /* Method table pointer will go here */
        } as_class;
        
    } data;
};


/* =========================================================================
 * Tagged Pointer API - The Magic That Makes Small Ints Free
 * ========================================================================= */

/* 
 * We use the lowest bit as a tag:
 *   Bit 0 = 1: This is a small integer (shift right 1 to get value)
 *   Bit 0 = 0: This is a real pointer to Object
 * 
 * This means small integers from -2^62 to 2^62-1 need zero allocation.
 * Perfect for HP, damage, gold, coordinates, etc.
 */

/* Check if a value is a tagged small integer */
static inline bool is_small_int(Object *obj) {
    return ((uintptr_t)obj & 1) == 1;
}

/* Check if a value is a heap object */
static inline bool is_heap_object(Object *obj) {
    return obj != NULL && ((uintptr_t)obj & 1) == 0;
}

/* Create a tagged small integer (no allocation!) */
static inline Object *make_small_int(int64_t value) {
    /* Shift left 1 and set the tag bit */
    return (Object *)(((uintptr_t)value << 1) | 1);
}

/* Extract the integer from a tagged pointer */
static inline int64_t get_small_int(Object *obj) {
    /* Arithmetic shift right to preserve sign */
    return ((intptr_t)obj) >> 1;
}


/* =========================================================================
 * Object Creation API - These Allocate From the Arena
 * ========================================================================= */

/* The global arena (set this at startup) */
extern Arena *g_arena;

/* Create nil singleton */
Object *obj_nil(void);

/* Create boolean (we use singletons for true/false) */
Object *obj_bool(bool value);

/* Create integer - uses tagged pointer if possible, heap otherwise */
Object *obj_int(int64_t value);

/* Create float */
Object *obj_float(double value);

/* Create string (copies the chars into the arena) */
Object *obj_string(const char *chars, uint32_t length);

/* Create array with initial capacity */
Object *obj_array(uint32_t capacity);

/* Create instance of a user-defined class */
Object *obj_instance(ClassID class_id, uint32_t field_count);


/* =========================================================================
 * Type Checking API
 * ========================================================================= */

TypeID obj_type(Object *obj);
bool obj_is_nil(Object *obj);
bool obj_is_bool(Object *obj);
bool obj_is_int(Object *obj);
bool obj_is_float(Object *obj);
bool obj_is_truthy(Object *obj);  /* For conditionals: false and nil are falsy */


/* =========================================================================
 * Value Extraction API
 * ========================================================================= */

bool    obj_as_bool(Object *obj);
int64_t obj_as_int(Object *obj);
double  obj_as_float(Object *obj);
const char *obj_as_string(Object *obj);


/* =========================================================================
 * Array Operations
 * ========================================================================= */

void    obj_array_push(Object *array, Object *value);
Object *obj_array_get(Object *array, uint32_t index);
void    obj_array_set(Object *array, uint32_t index, Object *value);
uint32_t obj_array_length(Object *array);


/* =========================================================================
 * Instance Field Access
 * ========================================================================= */

Object *obj_get_field(Object *instance, uint32_t field_index);
void    obj_set_field(Object *instance, uint32_t field_index, Object *value);


/* =========================================================================
 * Debug / Inspection
 * ========================================================================= */

/* Print an object for debugging */
void obj_print(Object *obj);

/* Get a string representation (allocates in arena) */
Object *obj_to_string(Object *obj);

#endif /* OBJECT_H */
