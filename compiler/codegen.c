/*
 * codegen.c - C Code Generator Implementation
 * 
 * Transforms the AST into C code that uses the runtime.
 */

#define _POSIX_C_SOURCE 200809L

#include "codegen.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* =========================================================================
 * Code Generation Context
 * ========================================================================= */

typedef struct {
    FILE *out;
    int indent;
    ClassDef *current_class;
    MethodDef *current_method;
    char **locals;
    int local_count;
    int local_capacity;
} CodeGen;

static void gen_expr(CodeGen *g, Expr *e);
static void gen_stmt(CodeGen *g, Stmt *s);

/* =========================================================================
 * Helpers
 * ========================================================================= */

static void emit_indent(CodeGen *g) {
    for (int i = 0; i < g->indent; i++) {
        fprintf(g->out, "    ");
    }
}

static void emit(CodeGen *g, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(g->out, fmt, args);
    va_end(args);
}

static char *to_c_name(const char *name) {
    size_t len = strlen(name);
    char *result = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        result[i] = (name[i] == ':' || name[i] == ' ') ? '_' : name[i];
    }
    result[len] = '\0';
    return result;
}

static char *to_upper(const char *name) {
    size_t len = strlen(name);
    char *result = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        result[i] = (c == ':' || c == ' ') ? '_' : toupper((unsigned char)c);
    }
    result[len] = '\0';
    return result;
}

static int find_field(CodeGen *g, const char *name) {
    if (!g->current_class) return -1;
    for (int i = 0; i < g->current_class->field_count; i++) {
        if (strcmp(g->current_class->fields[i], name) == 0) return i;
    }
    return -1;
}

static int is_local(CodeGen *g, const char *name) {
    for (int i = 0; i < g->local_count; i++) {
        if (strcmp(g->locals[i], name) == 0) return 1;
    }
    return 0;
}

static void add_local(CodeGen *g, const char *name) {
    if (g->local_count >= g->local_capacity) {
        g->local_capacity = g->local_capacity == 0 ? 16 : g->local_capacity * 2;
        g->locals = realloc(g->locals, sizeof(char*) * g->local_capacity);
    }
    g->locals[g->local_count++] = strdup(name);
}

static int find_param(CodeGen *g, const char *name) {
    if (!g->current_method) return -1;
    for (int i = 0; i < g->current_method->param_count; i++) {
        if (strcmp(g->current_method->params[i], name) == 0) return i;
    }
    return -1;
}

static void emit_c_string_literal(CodeGen *g, const char *chars, int length) {
    emit(g, "\"");
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)chars[i];
        switch (c) {
            case '\n': emit(g, "\\n"); break;
            case '\r': emit(g, "\\r"); break;
            case '\t': emit(g, "\\t"); break;
            case '"':  emit(g, "\\\""); break;
            case '\\': emit(g, "\\\\"); break;
            default:
                if (c < 32 || c >= 127) {
                    emit(g, "\\%03o", c);
                } else {
                    emit(g, "%c", c);
                }
                break;
        }
    }
    emit(g, "\"");
}

/* =========================================================================
 * Expression Generation
 * ========================================================================= */

static void gen_expr(CodeGen *g, Expr *e) {
    switch (e->type) {
        case EXPR_INT:
            emit(g, "obj_int(%lldLL)", (long long)e->as.int_value);
            break;
            
        case EXPR_FLOAT:
            emit(g, "obj_float(%g)", e->as.float_value);
            break;
            
        case EXPR_STRING:
            emit(g, "obj_string(");
            emit_c_string_literal(g, e->as.string_value.chars, e->as.string_value.length);
            emit(g, ", %d)", e->as.string_value.length);
            break;
            
        case EXPR_BOOL:
            emit(g, "obj_bool(%s)", e->as.bool_value ? "true" : "false");
            break;
            
        case EXPR_NIL:
            emit(g, "obj_nil()");
            break;
            
        case EXPR_SELF:
            emit(g, "self");
            break;
            
        case EXPR_IDENTIFIER: {
            int param_idx = find_param(g, e->as.identifier.name);
            if (param_idx >= 0) {
                emit(g, "args[%d]", param_idx);
            } else if (is_local(g, e->as.identifier.name)) {
                char *cname = to_c_name(e->as.identifier.name);
                emit(g, "%s", cname);
                free(cname);
            } else {
                char *cname = to_c_name(e->as.identifier.name);
                emit(g, "%s", cname);
                free(cname);
            }
            break;
        }
        
        case EXPR_BINARY: {
            const char *sel = NULL;
            switch (e->as.binary.op) {
                case OP_ADD: sel = "+"; break;
                case OP_SUB: sel = "-"; break;
                case OP_MUL: sel = "*"; break;
                case OP_DIV: sel = "/"; break;
                case OP_EQ:  sel = "="; break;
                case OP_NE:
                    emit(g, "obj_bool(!obj_as_bool(send1(");
                    gen_expr(g, e->as.binary.left);
                    emit(g, ", selector_intern(\"=\"), ");
                    gen_expr(g, e->as.binary.right);
                    emit(g, ")))");
                    return;
                case OP_LT:  sel = "<"; break;
                case OP_GT:  sel = ">"; break;
                case OP_LE:  sel = "<="; break;
                case OP_GE:  sel = ">="; break;
                case OP_AND:
                    emit(g, "(obj_is_truthy(");
                    gen_expr(g, e->as.binary.left);
                    emit(g, ") ? (obj_is_truthy(");
                    gen_expr(g, e->as.binary.right);
                    emit(g, ") ? obj_bool(true) : obj_bool(false)) : obj_bool(false))");
                    return;
                case OP_OR:
                    emit(g, "(obj_is_truthy(");
                    gen_expr(g, e->as.binary.left);
                    emit(g, ") ? obj_bool(true) : (obj_is_truthy(");
                    gen_expr(g, e->as.binary.right);
                    emit(g, ") ? obj_bool(true) : obj_bool(false)))");
                    return;
            }
            emit(g, "send1(");
            gen_expr(g, e->as.binary.left);
            emit(g, ", selector_intern(\"%s\"), ", sel);
            gen_expr(g, e->as.binary.right);
            emit(g, ")");
            break;
        }
        
        case EXPR_UNARY:
            if (e->as.unary.op == OP_NEG) {
                emit(g, "obj_int(-obj_as_int(");
                gen_expr(g, e->as.unary.operand);
                emit(g, "))");
            } else {
                emit(g, "obj_bool(!obj_is_truthy(");
                gen_expr(g, e->as.unary.operand);
                emit(g, "))");
            }
            break;
            
        case EXPR_CALL: {
            if (e->as.call.arg_count == 0) {
                emit(g, "send0(");
                gen_expr(g, e->as.call.receiver);
                emit(g, ", selector_intern(\"%s\"))", e->as.call.selector);
            } else if (e->as.call.arg_count == 1) {
                emit(g, "send1(");
                gen_expr(g, e->as.call.receiver);
                emit(g, ", selector_intern(\"%s\"), ", e->as.call.selector);
                gen_expr(g, e->as.call.args[0]);
                emit(g, ")");
            } else {
                emit(g, "({ Object *_args[] = {");
                for (int i = 0; i < e->as.call.arg_count; i++) {
                    if (i > 0) emit(g, ", ");
                    gen_expr(g, e->as.call.args[i]);
                }
                emit(g, "}; send(");
                gen_expr(g, e->as.call.receiver);
                emit(g, ", selector_intern(\"%s\"), _args, %d); })",
                       e->as.call.selector, e->as.call.arg_count);
            }
            break;
        }
        
        case EXPR_GET_FIELD: {
            if (e->as.get_field.object->type == EXPR_SELF && g->current_class) {
                int idx = find_field(g, e->as.get_field.field_name);
                if (idx >= 0) {
                    char *upper = to_upper(g->current_class->name);
                    char *fupper = to_upper(e->as.get_field.field_name);
                    emit(g, "obj_get_field(self, %s_FIELD_%s)", upper, fupper);
                    free(upper);
                    free(fupper);
                    return;
                }
            }
            emit(g, "obj_get_named_field(");
            gen_expr(g, e->as.get_field.object);
            emit(g, ", \"%s\")", e->as.get_field.field_name);
            break;
        }
        
        case EXPR_SET_FIELD: {
            if (e->as.set_field.object->type == EXPR_SELF && g->current_class) {
                int idx = find_field(g, e->as.set_field.field_name);
                if (idx >= 0) {
                    char *upper = to_upper(g->current_class->name);
                    char *fupper = to_upper(e->as.set_field.field_name);
                    emit(g, "(obj_set_field(self, %s_FIELD_%s, ", upper, fupper);
                    gen_expr(g, e->as.set_field.value);
                    emit(g, "), obj_nil())");
                    free(upper);
                    free(fupper);
                    return;
                }
            }
            emit(g, "(obj_set_named_field(");
            gen_expr(g, e->as.set_field.object);
            emit(g, ", \"%s\", ", e->as.set_field.field_name);
            gen_expr(g, e->as.set_field.value);
            emit(g, "), obj_nil())");
            break;
        }
        
        case EXPR_NEW: {
            char *cname = to_c_name(e->as.new_expr.class_name);
            emit(g, "%s_new(NULL, NULL, 0)", cname);
            free(cname);
            break;
        }
        
        case EXPR_ARRAY:
            emit(g, "({ Object *_arr = obj_array(%d); ", e->as.array.count);
            for (int i = 0; i < e->as.array.count; i++) {
                emit(g, "obj_array_push(_arr, ");
                gen_expr(g, e->as.array.elements[i]);
                emit(g, "); ");
            }
            emit(g, "_arr; })");
            break;
            
        case EXPR_INDEX:
            emit(g, "obj_array_get(");
            gen_expr(g, e->as.index.object);
            emit(g, ", (uint32_t)obj_as_int(");
            gen_expr(g, e->as.index.index);
            emit(g, "))");
            break;
    }
}

/* =========================================================================
 * Statement Generation
 * ========================================================================= */

static void gen_stmt(CodeGen *g, Stmt *s) {
    switch (s->type) {
        case STMT_EXPR:
            emit_indent(g);
            gen_expr(g, s->as.expr.expression);
            emit(g, ";\n");
            break;
            
        case STMT_VAR: {
            char *cname = to_c_name(s->as.var.name);
            add_local(g, s->as.var.name);
            emit_indent(g);
            emit(g, "Object *%s = ", cname);
            if (s->as.var.initializer) {
                gen_expr(g, s->as.var.initializer);
            } else {
                emit(g, "obj_nil()");
            }
            emit(g, ";\n");
            free(cname);
            break;
        }
        
        case STMT_ASSIGN: {
            char *cname = to_c_name(s->as.assign.name);
            emit_indent(g);
            emit(g, "%s = ", cname);
            gen_expr(g, s->as.assign.value);
            emit(g, ";\n");
            free(cname);
            break;
        }
        
        case STMT_IF:
            emit_indent(g);
            emit(g, "if (obj_is_truthy(");
            gen_expr(g, s->as.if_stmt.condition);
            emit(g, ")) {\n");
            g->indent++;
            for (int i = 0; i < s->as.if_stmt.then_count; i++) {
                gen_stmt(g, s->as.if_stmt.then_branch[i]);
            }
            g->indent--;
            if (s->as.if_stmt.else_count > 0) {
                emit_indent(g);
                emit(g, "} else {\n");
                g->indent++;
                for (int i = 0; i < s->as.if_stmt.else_count; i++) {
                    gen_stmt(g, s->as.if_stmt.else_branch[i]);
                }
                g->indent--;
            }
            emit_indent(g);
            emit(g, "}\n");
            break;
            
        case STMT_WHILE:
            emit_indent(g);
            emit(g, "while (obj_is_truthy(");
            gen_expr(g, s->as.while_stmt.condition);
            emit(g, ")) {\n");
            g->indent++;
            for (int i = 0; i < s->as.while_stmt.body_count; i++) {
                gen_stmt(g, s->as.while_stmt.body[i]);
            }
            g->indent--;
            emit_indent(g);
            emit(g, "}\n");
            break;
            
        case STMT_RETURN:
            emit_indent(g);
            emit(g, "return ");
            if (s->as.return_stmt.value) {
                gen_expr(g, s->as.return_stmt.value);
            } else {
                emit(g, "obj_nil()");
            }
            emit(g, ";\n");
            break;
            
        case STMT_BLOCK:
            emit_indent(g);
            emit(g, "{\n");
            g->indent++;
            for (int i = 0; i < s->as.block.count; i++) {
                gen_stmt(g, s->as.block.statements[i]);
            }
            g->indent--;
            emit_indent(g);
            emit(g, "}\n");
            break;
    }
}

/* =========================================================================
 * Class Generation
 * ========================================================================= */

static void gen_class(CodeGen *g, ClassDef *cls) {
    char *cname = to_c_name(cls->name);
    char *upper = to_upper(cls->name);
    
    g->current_class = cls;
    
    /* Field indices */
    emit(g, "/* ========== Class: %s ========== */\n", cls->name);
    for (int i = 0; i < cls->field_count; i++) {
        char *fupper = to_upper(cls->fields[i]);
        emit(g, "#define %s_FIELD_%s %d\n", upper, fupper, i);
        free(fupper);
    }
    emit(g, "#define %s_FIELD_COUNT %d\n", upper, cls->field_count);
    emit(g, "static ClassID CLASS_%s;\n", upper);
    
    /* Selectors */
    for (int i = 0; i < cls->method_count; i++) {
        char *mupper = to_upper(cls->methods[i]->name);
        emit(g, "static SelectorID SEL_%s_%s;\n", upper, mupper);
        free(mupper);
    }
    emit(g, "\n");
    
    /* Methods */
    for (int i = 0; i < cls->method_count; i++) {
        MethodDef *m = cls->methods[i];
        char *mname = to_c_name(m->name);
        
        g->current_method = m;
        g->local_count = 0;
        
        emit(g, "static Object *%s_%s(Object *self, Object **args, int argc) {\n",
             cname, mname);
        emit(g, "    (void)self; (void)args; (void)argc;\n");
        
        g->indent = 1;
        for (int j = 0; j < m->body_count; j++) {
            gen_stmt(g, m->body[j]);
        }
        
        emit(g, "    return obj_nil();\n");
        emit(g, "}\n\n");
        free(mname);
    }
    
    /* Constructor */
    emit(g, "static Object *%s_new(Object *self, Object **args, int argc) {\n", cname);
    emit(g, "    (void)self; (void)args; (void)argc;\n");
    emit(g, "    return obj_instance(CLASS_%s, %s_FIELD_COUNT);\n", upper, upper);
    emit(g, "}\n\n");
    
    /* Registration */
    emit(g, "static void register_%s_class(void) {\n", cname);
    for (int i = 0; i < cls->method_count; i++) {
        char *mupper = to_upper(cls->methods[i]->name);
        emit(g, "    SEL_%s_%s = selector_intern(\"%s\");\n", 
             upper, mupper, cls->methods[i]->name);
        free(mupper);
    }
    emit(g, "    CLASS_%s = class_register(\"%s\", %s_FIELD_COUNT);\n",
         upper, cls->name, upper);
    for (int i = 0; i < cls->field_count; i++) {
        emit(g, "    class_add_field(CLASS_%s, %d, \"%s\");\n",
             upper, i, cls->fields[i]);
    }
    emit(g, "    class_add_method_arity(CLASS_%s, SEL_NEW, %s_new, 0);\n", upper, cname);
    for (int i = 0; i < cls->method_count; i++) {
        char *mname = to_c_name(cls->methods[i]->name);
        char *mupper = to_upper(cls->methods[i]->name);
        emit(g, "    class_add_method_arity(CLASS_%s, SEL_%s_%s, %s_%s, %d);\n",
             upper, upper, mupper, cname, mname, cls->methods[i]->param_count);
        free(mname);
        free(mupper);
    }
    emit(g, "}\n\n");
    
    g->current_class = NULL;
    free(cname);
    free(upper);
}

/* =========================================================================
 * Main Generation
 * ========================================================================= */

void codegen_generate(Program *prog, FILE *out) {
    CodeGen g = {
        .out = out,
        .indent = 0,
        .current_class = NULL,
        .current_method = NULL,
        .locals = NULL,
        .local_count = 0,
        .local_capacity = 0
    };
    
    /* Header */
    emit(&g, "/* Generated by the Sprite Compiler */\n");
    emit(&g, "#include \"runtime.h\"\n");
    emit(&g, "#include <stdio.h>\n\n");
    
    /* Generate classes */
    for (int i = 0; i < prog->class_count; i++) {
        gen_class(&g, prog->classes[i]);
    }
    
    /* Generate main */
    if (prog->main_method) {
        g.current_method = prog->main_method;
        g.local_count = 0;
        
        emit(&g, "int main(void) {\n");
        emit(&g, "    runtime_init(1024 * 1024);  /* 1MB arena */\n\n");
        
        /* Register all classes */
        for (int i = 0; i < prog->class_count; i++) {
            char *cname = to_c_name(prog->classes[i]->name);
            emit(&g, "    register_%s_class();\n", cname);
            free(cname);
        }
        emit(&g, "\n");
        
        /* Main body */
        g.indent = 1;
        for (int i = 0; i < prog->main_method->body_count; i++) {
            gen_stmt(&g, prog->main_method->body[i]);
        }
        
        emit(&g, "\n    runtime_shutdown();\n");
        emit(&g, "    return 0;\n");
        emit(&g, "}\n");
    }
    
    /* Cleanup */
    for (int i = 0; i < g.local_count; i++) {
        free(g.locals[i]);
    }
    free(g.locals);
}
