/*
 * dispatch.h - Message Dispatch System
 * 
 * Philosophy: Objects receive messages. When you write:
 *     hero take_damage: 10
 * 
 * This becomes a "send" operation:
 *     send(hero, "take_damage:", arg_int(10))
 * 
 * The dispatcher looks up which method to call based on:
 *   1. The receiver's class (hero's ClassID)
 *   2. The message selector ("take_damage:")
 * 
 * Implementation:
 * - Each class has a method table (array of selector -> function pointer)
 * - Dispatch is a simple table lookup, not a hash table
 * - Selectors are interned strings (compared by pointer, not strcmp)
 */

#ifndef DISPATCH_H
#define DISPATCH_H

#include "object.h"

/* =========================================================================
 * Selectors - Interned Message Names
 * ========================================================================= */

/*
 * A Selector is a unique ID for a message name.
 * "take_damage:" always maps to the same SelectorID.
 * This makes dispatch a simple array index lookup.
 */
typedef uint32_t SelectorID;

/* Intern a message name and get its unique ID */
SelectorID selector_intern(const char *name);

/* Get the name back from a selector (for debugging) */
const char *selector_name(SelectorID sel);


/* =========================================================================
 * Method Signature
 * ========================================================================= */

/*
 * All methods have this signature:
 *   - receiver: The object receiving the message (self)
 *   - args: Array of argument objects
 *   - argc: Number of arguments
 *   Returns: The result object
 */
typedef Object *(*MethodFunc)(Object *receiver, Object **args, int argc);


/* =========================================================================
 * Class Registration
 * ========================================================================= */

/* 
 * Register a new user-defined class.
 * Returns the ClassID to use when creating instances.
 */
ClassID class_register(const char *name, uint32_t field_count);

/* Add a method to a class */
void class_add_method(ClassID class_id, SelectorID selector, MethodFunc method);

/* Add a method with a known arity, used for diagnostics. Use -1 for dynamic arity. */
void class_add_method_arity(ClassID class_id, SelectorID selector, MethodFunc method, int arity);

/* Add a field name to a class at a compiler-assigned index */
void class_add_field(ClassID class_id, uint32_t field_index, const char *name);

/* Find a field index by name. Returns -1 if the field does not exist. */
int class_find_field(ClassID class_id, const char *name);

/* Get the field count for a class */
uint32_t class_field_count(ClassID class_id);

/* Get the class name */
const char *class_name(ClassID class_id);


/* =========================================================================
 * Message Sending - The Core Dispatch
 * ========================================================================= */

/*
 * Send a message to an object.
 * This is the heart of the language runtime.
 * 
 * Example: send(hero, sel_take_damage, args, 1)
 */
Object *send(Object *receiver, SelectorID selector, Object **args, int argc);

/* Convenience: send with no arguments */
Object *send0(Object *receiver, SelectorID selector);

/* Convenience: send with one argument */
Object *send1(Object *receiver, SelectorID selector, Object *arg);

/* Convenience: send with two arguments */
Object *send2(Object *receiver, SelectorID selector, Object *arg1, Object *arg2);

/* Runtime field lookup for cross-object access when the compiler does not know the class. */
Object *obj_get_named_field(Object *instance, const char *field_name);
void obj_set_named_field(Object *instance, const char *field_name, Object *value);


/* =========================================================================
 * Built-in Selectors - Pre-interned for Performance
 * ========================================================================= */

/* Initialize the dispatch system (call once at startup) */
void dispatch_init(void);

/* Common selectors - these are pre-interned */
extern SelectorID SEL_NEW;           /* new */
extern SelectorID SEL_INIT;          /* init */
extern SelectorID SEL_DO;            /* do (for booleans: conditional execution) */
extern SelectorID SEL_ELSE;          /* else */
extern SelectorID SEL_PLUS;          /* + */
extern SelectorID SEL_MINUS;         /* - */
extern SelectorID SEL_TIMES;         /* * */
extern SelectorID SEL_DIVIDE;        /* / */
extern SelectorID SEL_EQ;            /* = */
extern SelectorID SEL_LT;            /* < */
extern SelectorID SEL_GT;            /* > */
extern SelectorID SEL_LE;            /* <= */
extern SelectorID SEL_GE;            /* >= */
extern SelectorID SEL_TO_STRING;     /* to_string */
extern SelectorID SEL_PRINT;         /* print */


/* =========================================================================
 * Built-in Methods for Primitive Types
 * ========================================================================= */

/* Register all built-in methods (Integer, Boolean, String, etc.) */
void register_builtins(void);

#endif /* DISPATCH_H */
