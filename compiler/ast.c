/*
 * ast.c - Abstract Syntax Tree Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * Helper: Duplicate a string
 * ========================================================================= */

static char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static char *str_ndup(const char *s, int len) {
    if (!s) return NULL;
    char *copy = malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

/* =========================================================================
 * Expression Constructors
 * ========================================================================= */

static Expr *expr_alloc(ExprType type, int line) {
    Expr *e = calloc(1, sizeof(Expr));
    e->type = type;
    e->line = line;
    return e;
}

Expr *expr_int(int64_t value, int line) {
    Expr *e = expr_alloc(EXPR_INT, line);
    e->as.int_value = value;
    return e;
}

Expr *expr_float(double value, int line) {
    Expr *e = expr_alloc(EXPR_FLOAT, line);
    e->as.float_value = value;
    return e;
}

Expr *expr_string(const char *chars, int length, int line) {
    Expr *e = expr_alloc(EXPR_STRING, line);
    e->as.string_value.chars = str_ndup(chars, length);
    e->as.string_value.length = length;
    return e;
}

Expr *expr_bool(int value, int line) {
    Expr *e = expr_alloc(EXPR_BOOL, line);
    e->as.bool_value = value;
    return e;
}

Expr *expr_nil(int line) {
    return expr_alloc(EXPR_NIL, line);
}

Expr *expr_identifier(const char *name, int line) {
    Expr *e = expr_alloc(EXPR_IDENTIFIER, line);
    e->as.identifier.name = str_dup(name);
    return e;
}

Expr *expr_self(int line) {
    return expr_alloc(EXPR_SELF, line);
}

Expr *expr_binary(BinaryOp op, Expr *left, Expr *right, int line) {
    Expr *e = expr_alloc(EXPR_BINARY, line);
    e->as.binary.op = op;
    e->as.binary.left = left;
    e->as.binary.right = right;
    return e;
}

Expr *expr_unary(UnaryOp op, Expr *operand, int line) {
    Expr *e = expr_alloc(EXPR_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.operand = operand;
    return e;
}

Expr *expr_call(Expr *receiver, const char *selector, Expr **args, int count, int line) {
    Expr *e = expr_alloc(EXPR_CALL, line);
    e->as.call.receiver = receiver;
    e->as.call.selector = str_dup(selector);
    e->as.call.args = args;
    e->as.call.arg_count = count;
    return e;
}

Expr *expr_get_field(Expr *object, const char *field, int line) {
    Expr *e = expr_alloc(EXPR_GET_FIELD, line);
    e->as.get_field.object = object;
    e->as.get_field.field_name = str_dup(field);
    return e;
}

Expr *expr_set_field(Expr *object, const char *field, Expr *value, int line) {
    Expr *e = expr_alloc(EXPR_SET_FIELD, line);
    e->as.set_field.object = object;
    e->as.set_field.field_name = str_dup(field);
    e->as.set_field.value = value;
    return e;
}

Expr *expr_new(const char *class_name, int line) {
    Expr *e = expr_alloc(EXPR_NEW, line);
    e->as.new_expr.class_name = str_dup(class_name);
    return e;
}

Expr *expr_array(Expr **elements, int count, int line) {
    Expr *e = expr_alloc(EXPR_ARRAY, line);
    e->as.array.elements = elements;
    e->as.array.count = count;
    return e;
}

Expr *expr_index(Expr *object, Expr *idx, int line) {
    Expr *e = expr_alloc(EXPR_INDEX, line);
    e->as.index.object = object;
    e->as.index.index = idx;
    return e;
}

/* =========================================================================
 * Statement Constructors
 * ========================================================================= */

static Stmt *stmt_alloc(StmtType type, int line) {
    Stmt *s = calloc(1, sizeof(Stmt));
    s->type = type;
    s->line = line;
    return s;
}

Stmt *stmt_expr(Expr *expression, int line) {
    Stmt *s = stmt_alloc(STMT_EXPR, line);
    s->as.expr.expression = expression;
    return s;
}

Stmt *stmt_var(const char *name, Expr *initializer, int line) {
    Stmt *s = stmt_alloc(STMT_VAR, line);
    s->as.var.name = str_dup(name);
    s->as.var.initializer = initializer;
    return s;
}

Stmt *stmt_assign(const char *name, Expr *value, int line) {
    Stmt *s = stmt_alloc(STMT_ASSIGN, line);
    s->as.assign.name = str_dup(name);
    s->as.assign.value = value;
    return s;
}

Stmt *stmt_if(Expr *cond, Stmt **then_b, int then_c, Stmt **else_b, int else_c, int line) {
    Stmt *s = stmt_alloc(STMT_IF, line);
    s->as.if_stmt.condition = cond;
    s->as.if_stmt.then_branch = then_b;
    s->as.if_stmt.then_count = then_c;
    s->as.if_stmt.else_branch = else_b;
    s->as.if_stmt.else_count = else_c;
    return s;
}

Stmt *stmt_while(Expr *cond, Stmt **body, int count, int line) {
    Stmt *s = stmt_alloc(STMT_WHILE, line);
    s->as.while_stmt.condition = cond;
    s->as.while_stmt.body = body;
    s->as.while_stmt.body_count = count;
    return s;
}

Stmt *stmt_return(Expr *value, int line) {
    Stmt *s = stmt_alloc(STMT_RETURN, line);
    s->as.return_stmt.value = value;
    return s;
}

Stmt *stmt_block(Stmt **stmts, int count, int line) {
    Stmt *s = stmt_alloc(STMT_BLOCK, line);
    s->as.block.statements = stmts;
    s->as.block.count = count;
    return s;
}

/* =========================================================================
 * Definition Constructors
 * ========================================================================= */

MethodDef *method_def(const char *name, char **params, int param_count,
                      Stmt **body, int body_count, int line) {
    MethodDef *m = calloc(1, sizeof(MethodDef));
    m->name = str_dup(name);
    m->params = params;
    m->param_count = param_count;
    m->body = body;
    m->body_count = body_count;
    m->line = line;
    return m;
}

ClassDef *class_def(const char *name, char **fields, int field_count,
                    MethodDef **methods, int method_count, int line) {
    ClassDef *c = calloc(1, sizeof(ClassDef));
    c->name = str_dup(name);
    c->fields = fields;
    c->field_count = field_count;
    c->methods = methods;
    c->method_count = method_count;
    c->line = line;
    return c;
}

Program *program_new(void) {
    return calloc(1, sizeof(Program));
}

void program_add_class(Program *prog, ClassDef *cls) {
    prog->class_count++;
    prog->classes = realloc(prog->classes, sizeof(ClassDef*) * prog->class_count);
    prog->classes[prog->class_count - 1] = cls;
}

void program_set_main(Program *prog, MethodDef *main) {
    prog->main_method = main;
}

/* =========================================================================
 * Debug Printing
 * ========================================================================= */

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

static void print_expr(Expr *e, int depth);
static void print_stmt(Stmt *s, int depth);

static void print_expr(Expr *e, int depth) {
    if (!e) { printf("(null)"); return; }
    
    switch (e->type) {
        case EXPR_INT:
            printf("%lld", (long long)e->as.int_value);
            break;
        case EXPR_FLOAT:
            printf("%g", e->as.float_value);
            break;
        case EXPR_STRING:
            printf("\"%s\"", e->as.string_value.chars);
            break;
        case EXPR_BOOL:
            printf("%s", e->as.bool_value ? "true" : "false");
            break;
        case EXPR_NIL:
            printf("nil");
            break;
        case EXPR_IDENTIFIER:
            printf("%s", e->as.identifier.name);
            break;
        case EXPR_SELF:
            printf("self");
            break;
        case EXPR_BINARY:
            printf("(");
            print_expr(e->as.binary.left, depth);
            switch (e->as.binary.op) {
                case OP_ADD: printf(" + "); break;
                case OP_SUB: printf(" - "); break;
                case OP_MUL: printf(" * "); break;
                case OP_DIV: printf(" / "); break;
                case OP_EQ:  printf(" == "); break;
                case OP_NE:  printf(" != "); break;
                case OP_LT:  printf(" < "); break;
                case OP_GT:  printf(" > "); break;
                case OP_LE:  printf(" <= "); break;
                case OP_GE:  printf(" >= "); break;
                case OP_AND: printf(" and "); break;
                case OP_OR:  printf(" or "); break;
            }
            print_expr(e->as.binary.right, depth);
            printf(")");
            break;
        case EXPR_UNARY:
            printf("(%s", e->as.unary.op == OP_NEG ? "-" : "not ");
            print_expr(e->as.unary.operand, depth);
            printf(")");
            break;
        case EXPR_CALL:
            printf("(send ");
            print_expr(e->as.call.receiver, depth);
            printf(" %s", e->as.call.selector);
            for (int i = 0; i < e->as.call.arg_count; i++) {
                printf(" ");
                print_expr(e->as.call.args[i], depth);
            }
            printf(")");
            break;
        case EXPR_GET_FIELD:
            print_expr(e->as.get_field.object, depth);
            printf(".%s", e->as.get_field.field_name);
            break;
        case EXPR_SET_FIELD:
            print_expr(e->as.set_field.object, depth);
            printf(".%s = ", e->as.set_field.field_name);
            print_expr(e->as.set_field.value, depth);
            break;
        case EXPR_NEW:
            printf("(%s new)", e->as.new_expr.class_name);
            break;
        case EXPR_ARRAY:
            printf("[");
            for (int i = 0; i < e->as.array.count; i++) {
                if (i > 0) printf(", ");
                print_expr(e->as.array.elements[i], depth);
            }
            printf("]");
            break;
        case EXPR_INDEX:
            print_expr(e->as.index.object, depth);
            printf("[");
            print_expr(e->as.index.index, depth);
            printf("]");
            break;
    }
}

static void print_stmt(Stmt *s, int depth) {
    print_indent(depth);
    
    switch (s->type) {
        case STMT_EXPR:
            print_expr(s->as.expr.expression, depth);
            printf("\n");
            break;
        case STMT_VAR:
            printf("var %s", s->as.var.name);
            if (s->as.var.initializer) {
                printf(" = ");
                print_expr(s->as.var.initializer, depth);
            }
            printf("\n");
            break;
        case STMT_ASSIGN:
            printf("%s = ", s->as.assign.name);
            print_expr(s->as.assign.value, depth);
            printf("\n");
            break;
        case STMT_IF:
            printf("if ");
            print_expr(s->as.if_stmt.condition, depth);
            printf(" do\n");
            for (int i = 0; i < s->as.if_stmt.then_count; i++) {
                print_stmt(s->as.if_stmt.then_branch[i], depth + 1);
            }
            if (s->as.if_stmt.else_count > 0) {
                print_indent(depth);
                printf("else\n");
                for (int i = 0; i < s->as.if_stmt.else_count; i++) {
                    print_stmt(s->as.if_stmt.else_branch[i], depth + 1);
                }
            }
            print_indent(depth);
            printf("end\n");
            break;
        case STMT_WHILE:
            printf("while ");
            print_expr(s->as.while_stmt.condition, depth);
            printf(" do\n");
            for (int i = 0; i < s->as.while_stmt.body_count; i++) {
                print_stmt(s->as.while_stmt.body[i], depth + 1);
            }
            print_indent(depth);
            printf("end\n");
            break;
        case STMT_RETURN:
            printf("return");
            if (s->as.return_stmt.value) {
                printf(" ");
                print_expr(s->as.return_stmt.value, depth);
            }
            printf("\n");
            break;
        case STMT_BLOCK:
            printf("do\n");
            for (int i = 0; i < s->as.block.count; i++) {
                print_stmt(s->as.block.statements[i], depth + 1);
            }
            print_indent(depth);
            printf("end\n");
            break;
    }
}

static void print_method(MethodDef *m, int depth) {
    print_indent(depth);
    printf("method %s", m->name);
    if (m->param_count > 0) {
        printf(":");
        for (int i = 0; i < m->param_count; i++) {
            printf(" %s", m->params[i]);
        }
    }
    printf(" do\n");
    for (int i = 0; i < m->body_count; i++) {
        print_stmt(m->body[i], depth + 1);
    }
    print_indent(depth);
    printf("end\n");
}

void ast_print_program(Program *prog) {
    for (int i = 0; i < prog->class_count; i++) {
        ClassDef *c = prog->classes[i];
        printf("class %s\n", c->name);
        for (int j = 0; j < c->field_count; j++) {
            printf("  var %s\n", c->fields[j]);
        }
        for (int j = 0; j < c->method_count; j++) {
            print_method(c->methods[j], 1);
        }
        printf("end\n\n");
    }
    
    if (prog->main_method) {
        print_method(prog->main_method, 0);
    }
}

/* =========================================================================
 * Memory Cleanup (simplified - doesn't free everything)
 * ========================================================================= */

void ast_free_program(Program *prog) {
    /* In a real compiler, we'd walk and free everything.
     * For now, let the OS clean up when we exit. */
    free(prog->classes);
    free(prog);
}
