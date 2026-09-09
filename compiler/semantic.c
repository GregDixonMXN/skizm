/*
 * semantic.c - Semantic validation before C code generation
 */

#define _POSIX_C_SOURCE 200809L

#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Program *program;
    ClassDef *current_class;
    MethodDef *current_method;
    char **locals;
    int local_count;
    int local_capacity;
    int had_error;
} Semantic;

static void validate_stmt(Semantic *s, Stmt *stmt);
static void validate_expr(Semantic *s, Expr *expr);

static void semantic_error(Semantic *s, int line, const char *message) {
    fprintf(stderr, "[line %d] Semantic error: %s\n", line, message);
    s->had_error = 1;
}

static void semantic_errorf(Semantic *s, int line, const char *fmt, const char *name) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), fmt, name);
    semantic_error(s, line, buffer);
}

static int is_local(Semantic *s, const char *name) {
    for (int i = 0; i < s->local_count; i++) {
        if (strcmp(s->locals[i], name) == 0) return 1;
    }
    return 0;
}

static void add_local(Semantic *s, const char *name) {
    if (is_local(s, name)) return;
    if (s->local_count >= s->local_capacity) {
        s->local_capacity = s->local_capacity == 0 ? 16 : s->local_capacity * 2;
        s->locals = realloc(s->locals, sizeof(char*) * s->local_capacity);
    }
    s->locals[s->local_count++] = strdup(name);
}

static int is_param(Semantic *s, const char *name) {
    if (!s->current_method) return 0;
    for (int i = 0; i < s->current_method->param_count; i++) {
        if (strcmp(s->current_method->params[i], name) == 0) return 1;
    }
    return 0;
}

static ClassDef *find_class(Semantic *s, const char *name) {
    for (int i = 0; i < s->program->class_count; i++) {
        if (strcmp(s->program->classes[i]->name, name) == 0) {
            return s->program->classes[i];
        }
    }
    return NULL;
}

static void clear_locals(Semantic *s) {
    for (int i = 0; i < s->local_count; i++) {
        free(s->locals[i]);
    }
    s->local_count = 0;
}

static void validate_expr(Semantic *s, Expr *expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STRING:
        case EXPR_BOOL:
        case EXPR_NIL:
        case EXPR_SELF:
            break;

        case EXPR_IDENTIFIER:
            if (strcmp(expr->as.identifier.name, "System") != 0 &&
                !is_param(s, expr->as.identifier.name) &&
                !is_local(s, expr->as.identifier.name)) {
                semantic_errorf(s, expr->line, "Unknown variable '%s'. Declare it with 'var' before using it.", expr->as.identifier.name);
            }
            break;

        case EXPR_BINARY:
            validate_expr(s, expr->as.binary.left);
            validate_expr(s, expr->as.binary.right);
            break;

        case EXPR_UNARY:
            validate_expr(s, expr->as.unary.operand);
            break;

        case EXPR_CALL:
            validate_expr(s, expr->as.call.receiver);
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                validate_expr(s, expr->as.call.args[i]);
            }
            break;

        case EXPR_GET_FIELD:
            /* Messages-only state: any field access desugars to a message
               send, so there is nothing to check against self here. */
            validate_expr(s, expr->as.get_field.object);
            break;

        case EXPR_SET_FIELD:
            validate_expr(s, expr->as.set_field.object);
            validate_expr(s, expr->as.set_field.value);
            break;

        case EXPR_NEW:
            if (strcmp(expr->as.new_expr.class_name, "Array") != 0 &&
                !find_class(s, expr->as.new_expr.class_name)) {
                semantic_errorf(s, expr->line, "Unknown class '%s'.", expr->as.new_expr.class_name);
            }
            break;

        case EXPR_ARRAY:
            for (int i = 0; i < expr->as.array.count; i++) {
                validate_expr(s, expr->as.array.elements[i]);
            }
            break;

        case EXPR_INDEX:
            validate_expr(s, expr->as.index.object);
            validate_expr(s, expr->as.index.index);
            break;
    }
}

static void validate_stmt(Semantic *s, Stmt *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_EXPR:
            validate_expr(s, stmt->as.expr.expression);
            break;

        case STMT_VAR:
            if (stmt->as.var.initializer) {
                validate_expr(s, stmt->as.var.initializer);
            }
            add_local(s, stmt->as.var.name);
            break;

        case STMT_ASSIGN:
            if (!is_local(s, stmt->as.assign.name) && !is_param(s, stmt->as.assign.name)) {
                semantic_errorf(s, stmt->line, "Cannot assign to unknown variable '%s'. Declare it with 'var' first.", stmt->as.assign.name);
            }
            validate_expr(s, stmt->as.assign.value);
            break;

        case STMT_IF:
            validate_expr(s, stmt->as.if_stmt.condition);
            for (int i = 0; i < stmt->as.if_stmt.then_count; i++) {
                validate_stmt(s, stmt->as.if_stmt.then_branch[i]);
            }
            for (int i = 0; i < stmt->as.if_stmt.else_count; i++) {
                validate_stmt(s, stmt->as.if_stmt.else_branch[i]);
            }
            break;

        case STMT_WHILE:
            validate_expr(s, stmt->as.while_stmt.condition);
            for (int i = 0; i < stmt->as.while_stmt.body_count; i++) {
                validate_stmt(s, stmt->as.while_stmt.body[i]);
            }
            break;

        case STMT_RETURN:
            validate_expr(s, stmt->as.return_stmt.value);
            break;

        case STMT_BLOCK:
            for (int i = 0; i < stmt->as.block.count; i++) {
                validate_stmt(s, stmt->as.block.statements[i]);
            }
            break;
    }
}

static void validate_method(Semantic *s, ClassDef *cls, MethodDef *method) {
    s->current_class = cls;
    s->current_method = method;
    clear_locals(s);

    for (int i = 0; i < method->body_count; i++) {
        validate_stmt(s, method->body[i]);
    }
}

int semantic_validate(Program *prog) {
    Semantic s = {
        .program = prog,
        .current_class = NULL,
        .current_method = NULL,
        .locals = NULL,
        .local_count = 0,
        .local_capacity = 0,
        .had_error = 0
    };

    for (int i = 0; i < prog->class_count; i++) {
        ClassDef *cls = prog->classes[i];
        for (int j = 0; j < cls->method_count; j++) {
            validate_method(&s, cls, cls->methods[j]);
        }
    }

    if (prog->main_method) {
        validate_method(&s, NULL, prog->main_method);
    }

    clear_locals(&s);
    free(s.locals);
    return !s.had_error;
}
