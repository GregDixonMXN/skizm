/*
 * object.c - The Universal Object System Implementation
 */

#include "object.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Global State
 * ========================================================================= */

Arena *g_arena = NULL;

/* Singletons for nil, true, false - never allocate these twice */
static Object nil_singleton  = { .type = TYPE_NIL,  .class_id = CLASS_BUILTIN };
static Object true_singleton = { .type = TYPE_BOOL, .class_id = CLASS_BUILTIN, .data.as_bool = true };
static Object false_singleton= { .type = TYPE_BOOL, .class_id = CLASS_BUILTIN, .data.as_bool = false };


/* =========================================================================
 * Object Creation
 * ========================================================================= */

Object *obj_nil(void) {
    return &nil_singleton;
}

Object *obj_bool(bool value) {
    return value ? &true_singleton : &false_singleton;
}

Object *obj_int(int64_t value) {
    /* 
     * Check if we can use a tagged pointer.
     * We need the value to fit in 63 bits (leaving 1 for the tag).
     * Safe range: roughly -4.6 quintillion to +4.6 quintillion.
     * More than enough for any game value.
     */
    const int64_t MIN_SMALL = -(1LL << 62);
    const int64_t MAX_SMALL = (1LL << 62) - 1;
    
    if (value >= MIN_SMALL && value <= MAX_SMALL) {
        return make_small_int(value);
    }
    
    /* Fallback: heap allocate (rare in games) */
    Object *obj = (Object *)arena_alloc(g_arena, sizeof(Object));
    if (!obj) return obj_nil();
    
    obj->type = TYPE_INT;
    obj->class_id = CLASS_BUILTIN;
    obj->data.as_int = value;
    return obj;
}

Object *obj_float(double value) {
    Object *obj = (Object *)arena_alloc(g_arena, sizeof(Object));
    if (!obj) return obj_nil();
    
    obj->type = TYPE_FLOAT;
    obj->class_id = CLASS_BUILTIN;
    obj->data.as_float = value;
    return obj;
}

Object *obj_string(const char *chars, uint32_t length) {
    Object *obj = (Object *)arena_alloc(g_arena, sizeof(Object));
    if (!obj) return obj_nil();
    
    /* Allocate space for the string content + null terminator */
    char *copy = (char *)arena_alloc(g_arena, length + 1);
    if (!copy) return obj_nil();
    
    memcpy(copy, chars, length);
    copy[length] = '\0';
    
    obj->type = TYPE_STRING;
    obj->class_id = CLASS_BUILTIN;
    obj->data.as_string.chars = copy;
    obj->data.as_string.length = length;
    return obj;
}

Object *obj_array(uint32_t capacity) {
    Object *obj = (Object *)arena_alloc(g_arena, sizeof(Object));
    if (!obj) return obj_nil();
    
    Object **items = NULL;
    if (capacity > 0) {
        items = (Object **)arena_alloc(g_arena, sizeof(Object *) * capacity);
        if (!items) return obj_nil();
    }
    
    obj->type = TYPE_ARRAY;
    obj->class_id = CLASS_BUILTIN;
    obj->data.as_array.items = items;
    obj->data.as_array.length = 0;
    obj->data.as_array.capacity = capacity;
    return obj;
}

Object *obj_instance(ClassID class_id, uint32_t field_count) {
    Object *obj = (Object *)arena_alloc(g_arena, sizeof(Object));
    if (!obj) return obj_nil();
    
    Object **fields = NULL;
    if (field_count > 0) {
        fields = (Object **)arena_alloc(g_arena, sizeof(Object *) * field_count);
        if (!fields) return obj_nil();
        
        /* Initialize all fields to nil */
        for (uint32_t i = 0; i < field_count; i++) {
            fields[i] = obj_nil();
        }
    }
    
    obj->type = TYPE_OBJECT;
    obj->class_id = class_id;
    obj->data.as_instance.fields = fields;
    obj->data.as_instance.field_count = field_count;
    return obj;
}


/* =========================================================================
 * Type Checking
 * ========================================================================= */

TypeID obj_type(Object *obj) {
    if (obj == NULL) return TYPE_NIL;
    if (is_small_int(obj)) return TYPE_INT;
    return obj->type;
}

bool obj_is_nil(Object *obj) {
    return obj == NULL || obj == &nil_singleton;
}

bool obj_is_bool(Object *obj) {
    return obj == &true_singleton || obj == &false_singleton;
}

bool obj_is_int(Object *obj) {
    if (is_small_int(obj)) return true;
    if (is_heap_object(obj)) return obj->type == TYPE_INT;
    return false;
}

bool obj_is_float(Object *obj) {
    return is_heap_object(obj) && obj->type == TYPE_FLOAT;
}

bool obj_is_truthy(Object *obj) {
    /* Only nil and false are falsy - everything else is truthy */
    if (obj_is_nil(obj)) return false;
    if (obj == &false_singleton) return false;
    return true;
}


/* =========================================================================
 * Value Extraction
 * ========================================================================= */

bool obj_as_bool(Object *obj) {
    if (obj == &true_singleton) return true;
    if (obj == &false_singleton) return false;
    /* Fallback: use truthiness */
    return obj_is_truthy(obj);
}

int64_t obj_as_int(Object *obj) {
    if (is_small_int(obj)) {
        return get_small_int(obj);
    }
    if (is_heap_object(obj) && obj->type == TYPE_INT) {
        return obj->data.as_int;
    }
    if (is_heap_object(obj) && obj->type == TYPE_FLOAT) {
        return (int64_t)obj->data.as_float;
    }
    return 0;
}

double obj_as_float(Object *obj) {
    if (is_small_int(obj)) {
        return (double)get_small_int(obj);
    }
    if (is_heap_object(obj) && obj->type == TYPE_FLOAT) {
        return obj->data.as_float;
    }
    if (is_heap_object(obj) && obj->type == TYPE_INT) {
        return (double)obj->data.as_int;
    }
    return 0.0;
}

const char *obj_as_string(Object *obj) {
    if (is_heap_object(obj) && obj->type == TYPE_STRING) {
        return obj->data.as_string.chars;
    }
    return "";
}


/* =========================================================================
 * Array Operations
 * ========================================================================= */

void obj_array_push(Object *array, Object *value) {
    if (!is_heap_object(array) || array->type != TYPE_ARRAY) return;
    
    if (array->data.as_array.length >= array->data.as_array.capacity) {
        /* 
         * Arena limitation: we can't resize in place.
         * In a real game, you'd pre-allocate enough capacity.
         * For now, we just refuse to grow.
         */
        fprintf(stderr, "Array full: cannot push (arena doesn't support realloc)\n");
        return;
    }
    
    array->data.as_array.items[array->data.as_array.length++] = value;
}

Object *obj_array_get(Object *array, uint32_t index) {
    if (!is_heap_object(array) || array->type != TYPE_ARRAY) return obj_nil();
    if (index >= array->data.as_array.length) return obj_nil();
    return array->data.as_array.items[index];
}

void obj_array_set(Object *array, uint32_t index, Object *value) {
    if (!is_heap_object(array) || array->type != TYPE_ARRAY) return;
    if (index >= array->data.as_array.length) return;
    array->data.as_array.items[index] = value;
}

uint32_t obj_array_length(Object *array) {
    if (!is_heap_object(array) || array->type != TYPE_ARRAY) return 0;
    return array->data.as_array.length;
}


/* =========================================================================
 * Instance Field Access
 * ========================================================================= */

Object *obj_get_field(Object *instance, uint32_t field_index) {
    if (!is_heap_object(instance) || instance->type != TYPE_OBJECT) return obj_nil();
    if (field_index >= instance->data.as_instance.field_count) return obj_nil();
    return instance->data.as_instance.fields[field_index];
}

void obj_set_field(Object *instance, uint32_t field_index, Object *value) {
    if (!is_heap_object(instance) || instance->type != TYPE_OBJECT) return;
    if (field_index >= instance->data.as_instance.field_count) return;
    instance->data.as_instance.fields[field_index] = value;
}


/* =========================================================================
 * Debug / Inspection
 * ========================================================================= */

void obj_print(Object *obj) {
    if (obj_is_nil(obj)) {
        printf("nil");
        return;
    }
    
    if (is_small_int(obj)) {
        printf("%lld", (long long)get_small_int(obj));
        return;
    }
    
    switch (obj->type) {
        case TYPE_BOOL:
            printf("%s", obj->data.as_bool ? "true" : "false");
            break;
        case TYPE_INT:
            printf("%lld", (long long)obj->data.as_int);
            break;
        case TYPE_FLOAT:
            printf("%g", obj->data.as_float);
            break;
        case TYPE_STRING:
            printf("\"%s\"", obj->data.as_string.chars);
            break;
        case TYPE_ARRAY:
            printf("[Array: %u items]", obj->data.as_array.length);
            break;
        case TYPE_OBJECT:
            printf("<Instance of class %u>", obj->class_id);
            break;
        default:
            printf("<Unknown>");
    }
}

Object *obj_to_string(Object *obj) {
    char buffer[256];
    int len = 0;
    
    if (obj_is_nil(obj)) {
        len = snprintf(buffer, sizeof(buffer), "nil");
    } else if (is_small_int(obj)) {
        len = snprintf(buffer, sizeof(buffer), "%lld", (long long)get_small_int(obj));
    } else {
        switch (obj->type) {
            case TYPE_BOOL:
                len = snprintf(buffer, sizeof(buffer), "%s", 
                              obj->data.as_bool ? "true" : "false");
                break;
            case TYPE_INT:
                len = snprintf(buffer, sizeof(buffer), "%lld", 
                              (long long)obj->data.as_int);
                break;
            case TYPE_FLOAT:
                len = snprintf(buffer, sizeof(buffer), "%g", obj->data.as_float);
                break;
            case TYPE_STRING:
                return obj;  /* Already a string */
            default:
                len = snprintf(buffer, sizeof(buffer), "<Object>");
        }
    }
    
    return obj_string(buffer, (uint32_t)len);
}
