
/*
 * dispatch.c - Message Dispatch System Implementation
 */

#define _POSIX_C_SOURCE 200809L  

#include "dispatch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SELECTORS 1024

static struct {
    const char *names[MAX_SELECTORS];
    uint32_t count;
} g_selectors = { .count = 0 };

SelectorID selector_intern(const char *name) {
    for (uint32_t i = 0; i < g_selectors.count; i++) {
        if (strcmp(g_selectors.names[i], name) == 0) return i;
    }
    if (g_selectors.count >= MAX_SELECTORS) {
        fprintf(stderr, "Too many selectors!\n");
        return 0;
    }
    char *copy = strdup(name);
    g_selectors.names[g_selectors.count] = copy;
    return g_selectors.count++;
}

const char *selector_name(SelectorID sel) {
    if (sel >= g_selectors.count) return "<invalid>";
    return g_selectors.names[sel];
}

SelectorID SEL_NEW, SEL_INIT, SEL_DO, SEL_ELSE, SEL_PLUS, SEL_MINUS, SEL_TIMES, SEL_DIVIDE;
SelectorID SEL_EQ, SEL_LT, SEL_GT, SEL_LE, SEL_GE, SEL_TO_STRING, SEL_PRINT;
SelectorID SEL_INPUT, SEL_CLEAR, SEL_RANDOM, SEL_TO_INT;

#define MAX_CLASSES 256
#define MAX_METHODS_PER_CLASS 64
#define MAX_FIELDS_PER_CLASS 128

typedef struct {
    const char *name;
    uint32_t field_count;
    const char *field_names[MAX_FIELDS_PER_CLASS];
    struct {
        SelectorID selector;
        MethodFunc method;
        int arity;
    } methods[MAX_METHODS_PER_CLASS];
    uint32_t method_count;
} ClassDef;

static struct {
    ClassDef classes[MAX_CLASSES];
    uint32_t count;
} g_classes = { .count = 0 };

static ClassID CLASS_INT_ID, CLASS_BOOL_ID, CLASS_STRING_ID, CLASS_ARRAY_ID, CLASS_SYSTEM_ID;

ClassID class_register(const char *name, uint32_t field_count) {
    if (g_classes.count >= MAX_CLASSES) return CLASS_BUILTIN;
    ClassID id = g_classes.count++;
    ClassDef *def = &g_classes.classes[id];
    def->name = strdup(name);
    def->field_count = field_count;
    for (uint32_t i = 0; i < MAX_FIELDS_PER_CLASS; i++) {
        def->field_names[i] = NULL;
    }
    def->method_count = 0;
    return id;
}

void class_add_method(ClassID class_id, SelectorID selector, MethodFunc method) {
    class_add_method_arity(class_id, selector, method, -1);
}

void class_add_method_arity(ClassID class_id, SelectorID selector, MethodFunc method, int arity) {
    if (class_id >= g_classes.count) return;
    ClassDef *def = &g_classes.classes[class_id];
    if (def->method_count >= MAX_METHODS_PER_CLASS) return;
    def->methods[def->method_count].selector = selector;
    def->methods[def->method_count].method = method;
    def->methods[def->method_count].arity = arity;
    def->method_count++;
}

void class_add_field(ClassID class_id, uint32_t field_index, const char *name) {
    if (class_id >= g_classes.count) return;
    if (field_index >= MAX_FIELDS_PER_CLASS) return;
    g_classes.classes[class_id].field_names[field_index] = strdup(name);
}

int class_find_field(ClassID class_id, const char *name) {
    if (class_id >= g_classes.count) return -1;
    ClassDef *def = &g_classes.classes[class_id];
    for (uint32_t i = 0; i < def->field_count && i < MAX_FIELDS_PER_CLASS; i++) {
        if (def->field_names[i] && strcmp(def->field_names[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

uint32_t class_field_count(ClassID class_id) {
    if (class_id >= g_classes.count) return 0;
    return g_classes.classes[class_id].field_count;
}

const char *class_name(ClassID class_id) {
    if (class_id >= g_classes.count) return "<unknown>";
    return g_classes.classes[class_id].name;
}

static MethodFunc find_method(ClassID class_id, SelectorID selector) {
    if (class_id >= g_classes.count) return NULL;
    ClassDef *def = &g_classes.classes[class_id];
    for (uint32_t i = 0; i < def->method_count; i++) {
        if (def->methods[i].selector == selector) return def->methods[i].method;
    }
    return NULL;
}

int class_responds_to(ClassID class_id, SelectorID selector) {
    return find_method(class_id, selector) != NULL;
}

static int find_method_arity(ClassID class_id, SelectorID selector) {
    if (class_id >= g_classes.count) return -1;
    ClassDef *def = &g_classes.classes[class_id];
    for (uint32_t i = 0; i < def->method_count; i++) {
        if (def->methods[i].selector == selector) return def->methods[i].arity;
    }
    return -1;
}

static int selector_arity(const char *selector) {
    int count = 0;
    for (const char *c = selector; *c; c++) {
        if (*c == ':') count++;
    }
    return count;
}

static int selector_base_matches(const char *actual, const char *wanted) {
    while (*actual && *actual != ':' && *wanted && *wanted != ':') {
        if (*actual != *wanted) return 0;
        actual++;
        wanted++;
    }
    return (*actual == '\0' || *actual == ':') && (*wanted == '\0' || *wanted == ':');
}

static const char *find_similar_selector(ClassID class_id, const char *wanted) {
    if (class_id >= g_classes.count) return NULL;
    ClassDef *def = &g_classes.classes[class_id];
    for (uint32_t i = 0; i < def->method_count; i++) {
        const char *actual = selector_name(def->methods[i].selector);
        if (selector_base_matches(actual, wanted)) return actual;
    }
    return NULL;
}

static ClassID get_class_id(Object *obj) {
    if (obj_is_nil(obj)) return CLASS_BUILTIN;
    if (is_small_int(obj)) return CLASS_INT_ID;
    switch (obj->type) {
        case TYPE_INT:    return CLASS_INT_ID;
        case TYPE_BOOL:   return CLASS_BOOL_ID;
        case TYPE_STRING: return CLASS_STRING_ID;
        case TYPE_ARRAY:  return CLASS_ARRAY_ID;
        case TYPE_OBJECT: return obj->class_id;
        default:          return CLASS_BUILTIN;
    }
}

Object *send(Object *receiver, SelectorID selector, Object **args, int argc) {
    ClassID class_id = get_class_id(receiver);
    if (selector == SEL_INPUT || selector == SEL_CLEAR || selector == SEL_RANDOM) {
        class_id = CLASS_SYSTEM_ID;
    }
    MethodFunc method = find_method(class_id, selector);
    if (method == NULL && obj_is_nil(receiver)) {
        MethodFunc system_method = find_method(CLASS_SYSTEM_ID, selector);
        const char *system_similar = find_similar_selector(CLASS_SYSTEM_ID, selector_name(selector));
        if (system_method != NULL || system_similar != NULL) {
            class_id = CLASS_SYSTEM_ID;
            method = system_method;
        }
    }
    if (method == NULL) {
        const char *wanted = selector_name(selector);
        fprintf(stderr, "No method '%s' (%d arg%s) for class '%s'",
                wanted, argc, argc == 1 ? "" : "s", class_name(class_id));
        const char *similar = find_similar_selector(class_id, wanted);
        if (similar) {
            fprintf(stderr, ". Did you mean '%s' (%d arg%s)?",
                    similar, selector_arity(similar), selector_arity(similar) == 1 ? "" : "s");
        }
        fprintf(stderr, "\n");
        return obj_nil();
    }
    int arity = find_method_arity(class_id, selector);
    if (arity >= 0 && arity != argc) {
        fprintf(stderr, "Method '%s' for class '%s' expected %d arg%s, got %d\n",
                selector_name(selector), class_name(class_id), arity, arity == 1 ? "" : "s", argc);
        return obj_nil();
    }
    return method(receiver, args, argc);
}

Object *send0(Object *receiver, SelectorID selector) { return send(receiver, selector, NULL, 0); }
Object *send1(Object *receiver, SelectorID selector, Object *arg) { Object *args[1] = { arg }; return send(receiver, selector, args, 1); }
Object *send2(Object *receiver, SelectorID selector, Object *arg1, Object *arg2) { Object *args[2] = { arg1, arg2 }; return send(receiver, selector, args, 2); }

static Object *int_plus(Object *self, Object **args, int argc) { if (argc < 1) return self; return obj_int(obj_as_int(self) + obj_as_int(args[0])); }
static Object *int_minus(Object *self, Object **args, int argc) { if (argc < 1) return self; return obj_int(obj_as_int(self) - obj_as_int(args[0])); }
static Object *int_times(Object *self, Object **args, int argc) { if (argc < 1) return self; return obj_int(obj_as_int(self) * obj_as_int(args[0])); }
static Object *int_divide(Object *self, Object **args, int argc) { if (argc < 1) return self; int64_t b = obj_as_int(args[0]); if (b == 0) return obj_int(0); return obj_int(obj_as_int(self) / b); }
static Object *int_eq(Object *self, Object **args, int argc) { if (argc < 1) return obj_bool(false); return obj_bool(obj_as_int(self) == obj_as_int(args[0])); }
static Object *int_lt(Object *self, Object **args, int argc) { if (argc < 1) return obj_bool(false); return obj_bool(obj_as_int(self) < obj_as_int(args[0])); }
static Object *int_gt(Object *self, Object **args, int argc) { if (argc < 1) return obj_bool(false); return obj_bool(obj_as_int(self) > obj_as_int(args[0])); }
static Object *int_le(Object *self, Object **args, int argc) { if (argc < 1) return obj_bool(false); return obj_bool(obj_as_int(self) <= obj_as_int(args[0])); }
static Object *int_ge(Object *self, Object **args, int argc) { if (argc < 1) return obj_bool(false); return obj_bool(obj_as_int(self) >= obj_as_int(args[0])); }
static Object *int_to_string(Object *self, Object **args, int argc) { (void)args; (void)argc; return obj_to_string(self); }
static Object *int_print(Object *self, Object **args, int argc) { (void)args; (void)argc; obj_print(self); printf("\n"); return self; }
static Object *nil_print(Object *self, Object **args, int argc) { (void)self; (void)args; (void)argc; printf("nil\n"); return obj_nil(); }

static Object *bool_do(Object *self, Object **args, int argc) { (void)args; (void)argc; return obj_bool(obj_as_bool(self)); }
static Object *bool_else(Object *self, Object **args, int argc) { (void)args; (void)argc; return obj_bool(!obj_as_bool(self)); }
static Object *bool_to_string(Object *self, Object **args, int argc) { (void)args; (void)argc; return obj_to_string(self); }
static Object *bool_print(Object *self, Object **args, int argc) { (void)args; (void)argc; obj_print(self); printf("\n"); return self; }

static Object *string_eq(Object *self, Object **args, int argc) { if (argc < 1) return obj_bool(false); Object *other = args[0]; if (!is_heap_object(other) || other->type != TYPE_STRING) return obj_bool(false); return obj_bool(strcmp(obj_as_string(self), obj_as_string(other)) == 0); }
static Object *string_plus(Object *self, Object **args, int argc) { if (argc < 1 || !is_heap_object(self)) return self; const char *a = obj_as_string(self); Object *other_str = obj_to_string(args[0]); const char *b = obj_as_string(other_str); uint32_t len_a = self->data.as_string.length; uint32_t len_b = is_heap_object(other_str) ? other_str->data.as_string.length : 0; char *buffer = (char *)arena_alloc(g_arena, len_a + len_b + 1); if (!buffer) return obj_nil(); memcpy(buffer, a, len_a); memcpy(buffer + len_a, b, len_b); buffer[len_a + len_b] = '\0'; return obj_string(buffer, len_a + len_b); }
static Object *string_to_string(Object *self, Object **args, int argc) { (void)args; (void)argc; return self; }
static Object *string_print(Object *self, Object **args, int argc) { (void)args; (void)argc; printf("%s\n", obj_as_string(self)); return self; }
static Object *string_to_int(Object *self, Object **args, int argc) { (void)args; (void)argc; if (!is_heap_object(self) || self->type != TYPE_STRING) return obj_int(0); return obj_int(atoll(self->data.as_string.chars)); }

static Object *array_push(Object *self, Object **args, int argc) { if (argc < 1) return self; obj_array_push(self, args[0]); return self; }
static Object *array_at(Object *self, Object **args, int argc) { if (argc < 1) return obj_nil(); return obj_array_get(self, (uint32_t)obj_as_int(args[0])); }

static Object *system_input(Object *self, Object **args, int argc) { (void)self; (void)args; (void)argc; char buffer[1024]; printf("> "); fflush(stdout); if (fgets(buffer, sizeof(buffer), stdin)) { size_t len = strlen(buffer); while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) { buffer[len - 1] = '\0'; len--; } return obj_string(buffer, (uint32_t)len); } return obj_nil(); }
static Object *system_clear(Object *self, Object **args, int argc) { (void)self; (void)args; (void)argc; printf("\033[H\033[2J"); return obj_nil(); }
static Object *system_random(Object *self, Object **args, int argc) { (void)self; if (argc < 2) return obj_int(0); int64_t min = obj_as_int(args[0]); int64_t max = obj_as_int(args[1]); if (max <= min) return obj_int(min); return obj_int(min + (rand() % (max - min + 1))); }
static Object *system_time(Object *self, Object **args, int argc) { (void)self; (void)args; (void)argc; return obj_int((int64_t)time(NULL)); }

void dispatch_init(void) {
    SEL_NEW = selector_intern("new"); SEL_INIT = selector_intern("init"); SEL_DO = selector_intern("do"); SEL_ELSE = selector_intern("else");
    SEL_PLUS = selector_intern("+"); SEL_MINUS = selector_intern("-"); SEL_TIMES = selector_intern("*"); SEL_DIVIDE = selector_intern("/");
    SEL_EQ = selector_intern("="); SEL_LT = selector_intern("<"); SEL_GT = selector_intern(">"); SEL_LE = selector_intern("<="); SEL_GE = selector_intern(">=");
    SEL_TO_STRING = selector_intern("to_string"); SEL_PRINT = selector_intern("print");
    SEL_INPUT = selector_intern("input"); SEL_CLEAR = selector_intern("clear"); SEL_RANDOM = selector_intern("random:max:"); SEL_TO_INT = selector_intern("to_int");
    srand((unsigned int)time(NULL));
}

void register_builtins(void) {
    /* FIX: Force 'Nil' to take ID 0 so integers aren't accidentally treated as nil */
    ClassID nil_id = class_register("Nil", 0);
    class_add_method(nil_id, SEL_PRINT, nil_print);
    
    CLASS_INT_ID = class_register("Integer", 0);
    CLASS_BOOL_ID = class_register("Boolean", 0);
    CLASS_STRING_ID = class_register("String", 0);
    CLASS_ARRAY_ID = class_register("Array", 0);
    CLASS_SYSTEM_ID = class_register("System", 0);
    
    class_add_method(CLASS_INT_ID, SEL_PLUS, int_plus); class_add_method(CLASS_INT_ID, SEL_MINUS, int_minus); class_add_method(CLASS_INT_ID, SEL_TIMES, int_times); class_add_method(CLASS_INT_ID, SEL_DIVIDE, int_divide); class_add_method(CLASS_INT_ID, SEL_EQ, int_eq); class_add_method(CLASS_INT_ID, SEL_LT, int_lt); class_add_method(CLASS_INT_ID, SEL_GT, int_gt); class_add_method(CLASS_INT_ID, SEL_LE, int_le); class_add_method(CLASS_INT_ID, SEL_GE, int_ge); class_add_method(CLASS_INT_ID, SEL_TO_STRING, int_to_string); class_add_method(CLASS_INT_ID, SEL_PRINT, int_print);
    class_add_method(CLASS_BOOL_ID, SEL_DO, bool_do); class_add_method(CLASS_BOOL_ID, SEL_ELSE, bool_else); class_add_method(CLASS_BOOL_ID, SEL_TO_STRING, bool_to_string); class_add_method(CLASS_BOOL_ID, SEL_PRINT, bool_print);
    class_add_method(CLASS_STRING_ID, SEL_PLUS, string_plus); class_add_method(CLASS_STRING_ID, SEL_PRINT, string_print); class_add_method(CLASS_STRING_ID, SEL_EQ, string_eq); class_add_method(CLASS_STRING_ID, SEL_TO_STRING, string_to_string); class_add_method(CLASS_STRING_ID, SEL_TO_INT, string_to_int);
    
    /* FIX: Added colons to match parser syntax */
    class_add_method(CLASS_ARRAY_ID, selector_intern("push:"), array_push);
    class_add_method(CLASS_ARRAY_ID, selector_intern("at:"), array_at);
    
    class_add_method(CLASS_SYSTEM_ID, SEL_INPUT, system_input); class_add_method(CLASS_SYSTEM_ID, SEL_CLEAR, system_clear); class_add_method(CLASS_SYSTEM_ID, SEL_RANDOM, system_random);
    class_add_method(CLASS_SYSTEM_ID, selector_intern("time"), system_time);
}
