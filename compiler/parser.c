/*
 * parser.c - Recursive Descent Parser Implementation
 * 
 * Parses your Lua-inspired syntax into an AST.
 */

#define _POSIX_C_SOURCE 200809L

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* =========================================================================
 * Error Handling
 * ========================================================================= */

static void error_at(Parser *p, Token *token, const char *message) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    p->had_error = 1;
    
    fprintf(stderr, "[line %d, col %d] Error", token->line, token->column);
    
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type != TOKEN_ERROR) {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    
    fprintf(stderr, ": %s\n", message);

    if (token->type != TOKEN_ERROR && token->start != NULL) {
        const char *line_start = token->start;
        while (line_start > p->lexer.source_start && line_start[-1] != '\n') {
            line_start--;
        }

        const char *line_end = token->start;
        while (*line_end != '\0' && *line_end != '\n') {
            line_end++;
        }

        int line_length = (int)(line_end - line_start);
        if (line_length > 0) {
            fprintf(stderr, "  %.*s\n  ", line_length, line_start);
            int caret_column = token->column > 0 ? token->column : 1;
            for (int i = 1; i < caret_column; i++) {
                char c = line_start[i - 1];
                fputc(c == '\t' ? '\t' : ' ', stderr);
            }
            fprintf(stderr, "^\n");
        }
    }
}

static void error(Parser *p, const char *message) {
    error_at(p, &p->previous, message);
}

static void error_current(Parser *p, const char *message) {
    error_at(p, &p->current, message);
}

static void error_currentf(Parser *p, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    error_at(p, &p->current, buffer);
}

/* =========================================================================
 * Token Helpers
 * ========================================================================= */

static void advance(Parser *p) {
    p->previous = p->current;
    
    for (;;) {
        p->current = lexer_next(&p->lexer);
        if (p->current.type != TOKEN_ERROR) break;
        error_current(p, p->current.start);
    }
}

static int check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static int match(Parser *p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

static int consume(Parser *p, TokenType type, const char *message) {
    if (p->current.type == type) {
        advance(p);
        return 1;
    }
    error_current(p, message);
    return 0;
}

static int is_expression_boundary(TokenType type) {
    return type == TOKEN_END ||
           type == TOKEN_ELSE ||
           type == TOKEN_EOF ||
           type == TOKEN_COMMA ||
           type == TOKEN_RPAREN ||
           type == TOKEN_RBRACKET;
}

static char *copy_token_string(Token *token) {
    char *str = malloc(token->length + 1);
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static Expr *parse_expression(Parser *p);
static Stmt *parse_statement(Parser *p);
static Stmt **parse_block(Parser *p, int *count);

/* =========================================================================
 * Expression Parsing (Pratt Parser Style)
 * ========================================================================= */

typedef enum {
    PREC_NONE,
    PREC_MESSAGE,     /* receiver message, receiver keyword: arg */
    PREC_OR,          /* or */
    PREC_AND,         /* and */
    PREC_EQUALITY,    /* == != */
    PREC_COMPARISON,  /* < > <= >= */
    PREC_TERM,        /* + - */
    PREC_FACTOR,      /* * / */
    PREC_UNARY,       /* - not */
    PREC_CALL,        /* . () [] */
    PREC_PRIMARY
} Precedence;

static Expr *parse_precedence(Parser *p, Precedence prec);

static int next_token_is_colon(Parser *p) {
    Token saved_current = p->current;
    Token saved_previous = p->previous;
    const char *saved_lexer_start = p->lexer.start;
    const char *saved_lexer_current = p->lexer.current;
    int saved_lexer_line = p->lexer.line;
    int saved_lexer_newline = p->lexer.at_line_start;

    advance(p);
    int is_colon = check(p, TOKEN_COLON);

    p->current = saved_current;
    p->previous = saved_previous;
    p->lexer.start = saved_lexer_start;
    p->lexer.current = saved_lexer_current;
    p->lexer.line = saved_lexer_line;
    p->lexer.at_line_start = saved_lexer_newline;

    return is_colon;
}

static Expr *parse_keyword_argument(Parser *p) {
    if (is_expression_boundary(p->current.type)) {
        error_current(p, "Expected expression after keyword ':'.");
        return expr_nil(p->current.line);
    }

    int previous_stop = p->stop_at_keyword;
    p->stop_at_keyword = 1;
    Expr *arg = parse_precedence(p, PREC_MESSAGE);
    p->stop_at_keyword = previous_stop;
    return arg;
}

/* Primary expressions */
static Expr *parse_number(Parser *p) {
    if (p->previous.type == TOKEN_INT) {
        int64_t value = strtoll(p->previous.start, NULL, 10);
        return expr_int(value, p->previous.line);
    } else {
        double value = strtod(p->previous.start, NULL);
        return expr_float(value, p->previous.line);
    }
}

static Expr *parse_string(Parser *p) {
    const char *raw = p->previous.start + 1;  /* Skip opening quote. */
    int raw_length = p->previous.length - 2;  /* Skip both quotes. */
    char *chars = malloc(raw_length + 1);
    int length = 0;

    for (int i = 0; i < raw_length; i++) {
        if (raw[i] == '\\' && i + 1 < raw_length) {
            char escaped = raw[++i];
            switch (escaped) {
                case 'n': chars[length++] = '\n'; break;
                case 'r': chars[length++] = '\r'; break;
                case 't': chars[length++] = '\t'; break;
                case '"': chars[length++] = '"'; break;
                case '\\': chars[length++] = '\\'; break;
                case '0': chars[length++] = '\0'; break;
                default:
                    chars[length++] = escaped;
                    break;
            }
        } else {
            chars[length++] = raw[i];
        }
    }

    chars[length] = '\0';
    Expr *expr = expr_string(chars, length, p->previous.line);
    free(chars);
    return expr;
}

static Expr *parse_literal(Parser *p) {
    switch (p->previous.type) {
        case TOKEN_TRUE:  return expr_bool(1, p->previous.line);
        case TOKEN_FALSE: return expr_bool(0, p->previous.line);
        case TOKEN_NIL:   return expr_nil(p->previous.line);
        default:          return NULL;  /* Unreachable */
    }
}

static Expr *parse_identifier(Parser *p) {
    char *name = copy_token_string(&p->previous);
    int line = p->previous.line;
    
    /* Check for 'ClassName new' pattern */
    if (match(p, TOKEN_NEW)) {
        Expr *e = expr_new(name, line);
        free(name);
        return e;
    }
    
    return expr_identifier(name, line);
}

static Expr *parse_self(Parser *p) {
    return expr_self(p->previous.line);
}

static Expr *parse_grouping(Parser *p) {
    Expr *e = parse_expression(p);
    consume(p, TOKEN_RPAREN, "Expected ')' after expression.");
    return e;
}

static Expr *parse_array_literal(Parser *p) {
    int line = p->previous.line;
    Expr **elements = NULL;
    int count = 0;
    int capacity = 0;
    
    if (!check(p, TOKEN_RBRACKET)) {
        do {
            if (is_expression_boundary(p->current.type)) {
                error_current(p, "Expected array element expression.");
                break;
            }
            if (count >= capacity) {
                capacity = capacity == 0 ? 8 : capacity * 2;
                elements = realloc(elements, sizeof(Expr*) * capacity);
            }
            elements[count++] = parse_expression(p);
        } while (match(p, TOKEN_COMMA));
    }
    
    consume(p, TOKEN_RBRACKET, "Expected ']' after array elements.");
    return expr_array(elements, count, line);
}

static Expr *parse_unary(Parser *p) {
    TokenType op_type = p->previous.type;
    int line = p->previous.line;
    
    Expr *operand = parse_precedence(p, PREC_UNARY);
    
    switch (op_type) {
        case TOKEN_MINUS: return expr_unary(OP_NEG, operand, line);
        case TOKEN_NOT:   return expr_unary(OP_NOT, operand, line);
        default:          return NULL;
    }
}

/* Infix expressions */
static Expr *parse_binary(Parser *p, Expr *left) {
    TokenType op_type = p->previous.type;
    int line = p->previous.line;
    
    /* Get the precedence of the operator */
    Precedence prec;
    BinaryOp op;
    
    switch (op_type) {
        case TOKEN_PLUS:   prec = PREC_TERM;       op = OP_ADD; break;
        case TOKEN_MINUS:  prec = PREC_TERM;       op = OP_SUB; break;
        case TOKEN_STAR:   prec = PREC_FACTOR;     op = OP_MUL; break;
        case TOKEN_SLASH:  prec = PREC_FACTOR;     op = OP_DIV; break;
        case TOKEN_EQEQ:   prec = PREC_EQUALITY;   op = OP_EQ;  break;
        case TOKEN_BANGEQ: prec = PREC_EQUALITY;   op = OP_NE;  break;
        case TOKEN_LT:     prec = PREC_COMPARISON; op = OP_LT;  break;
        case TOKEN_GT:     prec = PREC_COMPARISON; op = OP_GT;  break;
        case TOKEN_LE:     prec = PREC_COMPARISON; op = OP_LE;  break;
        case TOKEN_GE:     prec = PREC_COMPARISON; op = OP_GE;  break;
        case TOKEN_AND:    prec = PREC_AND;        op = OP_AND; break;
        case TOKEN_OR:     prec = PREC_OR;         op = OP_OR;  break;
        default:           return left;
    }
    
    Expr *right = parse_precedence(p, (Precedence)(prec + 1));
    return expr_binary(op, left, right, line);
}

static Expr *parse_dot(Parser *p, Expr *left) {
    int line = p->previous.line;
    if (!consume(p, TOKEN_IDENTIFIER, "Expected field name after '.'. Use syntax like player.health.")) {
        return left;
    }
    char *field = copy_token_string(&p->previous);
    
    
    
    /* Check for assignment: obj.field = value */
    if (match(p, TOKEN_EQ)) {
        if (is_expression_boundary(p->current.type)) {
            error_currentf(p, "Expected expression to assign to field '%s' after '='.", field);
            Expr *e = expr_set_field(left, field, expr_nil(p->current.line), line);
            free(field);
            return e;
        }
        Expr *value = parse_expression(p);
        
        Expr *e = expr_set_field(left, field, value, line);
        free(field);
        return e;
    }
    
    Expr *e = expr_get_field(left, field, line);
    free(field);
    return e;
}

static Expr *parse_index(Parser *p, Expr *left) {
    int line = p->previous.line;
    if (is_expression_boundary(p->current.type)) {
        error_current(p, "Expected index expression inside '[...]'.");
        return expr_index(left, expr_nil(p->current.line), line);
    }
    Expr *index = parse_expression(p);
    consume(p, TOKEN_RBRACKET, "Expected ']' after index.");
    return expr_index(left, index, line);
}

static Expr *parse_call(Parser *p, Expr *left) {
    /* 
     * Message send syntax: receiver selector: arg1 arg2: arg2
     * Example: System random: 1 max: 100
     */
    char *selector = copy_token_string(&p->previous);
    int line = p->previous.line;
    
    /* Check for colon (indicates arguments) */
    if (match(p, TOKEN_COLON)) {
        /* Append colon to selector name */
        size_t len = strlen(selector);
        selector = realloc(selector, len + 2);
        selector[len] = ':';
        selector[len + 1] = '\0';
        
        /* Parse arguments */
        Expr **args = NULL;
        int arg_count = 0;
        int arg_capacity = 0;
        
        /* Parse first argument */
        if (arg_count >= arg_capacity) {
            arg_capacity = 4;
            args = realloc(args, sizeof(Expr*) * arg_capacity);
        }
        args[arg_count++] = parse_keyword_argument(p);

        while (check(p, TOKEN_IDENTIFIER) && !p->current.after_newline) {
            Token saved_current = p->current;
            Token saved_previous = p->previous;
            const char *saved_lexer_start = p->lexer.start;
            const char *saved_lexer_current = p->lexer.current;
            int saved_lexer_line = p->lexer.line;
            int saved_lexer_newline = p->lexer.at_line_start;

            advance(p);
            char *keyword = copy_token_string(&p->previous);

            if (!match(p, TOKEN_COLON)) {
                free(keyword);
                p->current = saved_current;
                p->previous = saved_previous;
                p->lexer.start = saved_lexer_start;
                p->lexer.current = saved_lexer_current;
                p->lexer.line = saved_lexer_line;
                p->lexer.at_line_start = saved_lexer_newline;
                break;
            }

            size_t selector_len = strlen(selector);
            size_t keyword_len = strlen(keyword);
            selector = realloc(selector, selector_len + keyword_len + 2);
            memcpy(selector + selector_len, keyword, keyword_len);
            selector[selector_len + keyword_len] = ':';
            selector[selector_len + keyword_len + 1] = '\0';
            free(keyword);

            if (arg_count >= arg_capacity) {
                arg_capacity *= 2;
                args = realloc(args, sizeof(Expr*) * arg_capacity);
            }
            args[arg_count++] = parse_keyword_argument(p);
        }
        
        Expr *e = expr_call(left, selector, args, arg_count, line);
        free(selector);
        return e;
    }
    
    /* No-argument message */
    Expr *e = expr_call(left, selector, NULL, 0, line);
    free(selector);
    return e;
}

/* Main expression parser */
static Expr *parse_precedence(Parser *p, Precedence prec) {
    advance(p);
    
    /* Prefix expression */
    Expr *left = NULL;
    
    switch (p->previous.type) {
        case TOKEN_INT:
        case TOKEN_FLOAT:
            left = parse_number(p);
            break;
        case TOKEN_STRING:
            left = parse_string(p);
            break;
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_NIL:
            left = parse_literal(p);
            break;
        case TOKEN_IDENTIFIER:
            left = parse_identifier(p);
            break;
        case TOKEN_SELF:
            left = parse_self(p);
            break;
        case TOKEN_LPAREN:
            left = parse_grouping(p);
            break;
        case TOKEN_LBRACKET:
            left = parse_array_literal(p);
            break;
        case TOKEN_MINUS:
        case TOKEN_NOT:
            left = parse_unary(p);
            break;
        default:
            if (is_expression_boundary(p->previous.type)) {
                error(p, "Expected expression before this token.");
            } else {
                error(p, "Expected expression.");
            }
            return expr_nil(p->previous.line);
    }
    
    /* Infix expressions */
    while (prec <= PREC_CALL) {
        if (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS) ||
            check(p, TOKEN_STAR) || check(p, TOKEN_SLASH)) {
            if (prec > PREC_FACTOR) break;
            advance(p);
            left = parse_binary(p, left);
        }
        else if (check(p, TOKEN_EQEQ) || check(p, TOKEN_BANGEQ)) {
            if (prec > PREC_EQUALITY) break;
            advance(p);
            left = parse_binary(p, left);
        }
        else if (check(p, TOKEN_LT) || check(p, TOKEN_GT) ||
                 check(p, TOKEN_LE) || check(p, TOKEN_GE)) {
            if (prec > PREC_COMPARISON) break;
            advance(p);
            left = parse_binary(p, left);
        }
        else if (check(p, TOKEN_AND)) {
            if (prec > PREC_AND) break;
            advance(p);
            left = parse_binary(p, left);
        }
        else if (check(p, TOKEN_OR)) {
            if (prec > PREC_OR) break;
            advance(p);
            left = parse_binary(p, left);
        }
        else if (check(p, TOKEN_DOT)) {
            advance(p);
            left = parse_dot(p, left);
        }
        else if (check(p, TOKEN_LBRACKET)) {
            advance(p);
            left = parse_index(p, left);
        }
        else if (check(p, TOKEN_IDENTIFIER)) {
            /* 
             * Message send: obj message
             *
             * Messages have lower precedence than binary operators so:
             *     "Gold: " + self.gold print
             * parses as:
             *     ("Gold: " + self.gold) print
             *
             * A message immediately after a primary still works normally:
             *     input to_int
             *
             * IMPORTANT: Don't chain message sends across newlines.
             * This allows statements like:
             *     hero attack
             *     enemy flee
             * to be separate statements instead of one chained expression.
             */
            if (prec > PREC_MESSAGE) break;
            if (p->current.after_newline) break;
            if (p->stop_at_keyword && next_token_is_colon(p)) break;
            advance(p);
            left = parse_call(p, left);
        }
        else {
            break;
        }
    }
    
    return left;
}

static Expr *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_MESSAGE);
}

/* =========================================================================
 * Statement Parsing
 * ========================================================================= */

static Stmt *parse_var_statement(Parser *p) {
    int line = p->previous.line;
    if (!consume(p, TOKEN_IDENTIFIER, "Expected variable name after 'var'.")) {
        return stmt_var("_error", NULL, line);
    }
    char *name = copy_token_string(&p->previous);
    
    Expr *initializer = NULL;
    if (match(p, TOKEN_EQ)) {
        if (is_expression_boundary(p->current.type)) {
            error_currentf(p, "Expected initializer expression for variable '%s' after '='.", name);
            return stmt_var(name, expr_nil(p->current.line), line);
        }
        initializer = parse_expression(p);
    }
    
    return stmt_var(name, initializer, line);
}

static Stmt *parse_if_statement(Parser *p) {
    int line = p->previous.line;
    Expr *condition = parse_expression(p);
    consume(p, TOKEN_DO, "Expected 'do' after if condition.");
    
    int then_count;
    Stmt **then_branch = parse_block(p, &then_count);
    
    int else_count = 0;
    Stmt **else_branch = NULL;
    
    if (match(p, TOKEN_ELSE)) {
        if (match(p, TOKEN_IF)) {
            /* else if */
            else_branch = malloc(sizeof(Stmt*));
            else_branch[0] = parse_if_statement(p);
            else_count = 1;
        } else {
            consume(p, TOKEN_DO, "Expected 'do' after else.");
            else_branch = parse_block(p, &else_count);
            consume(p, TOKEN_END, "Expected 'end' after else block.");
        }
    } else {
        consume(p, TOKEN_END, "Expected 'end' after if block.");
    }
    
    return stmt_if(condition, then_branch, then_count, 
                   else_branch, else_count, line);
}

static Stmt *parse_while_statement(Parser *p) {
    int line = p->previous.line;
    Expr *condition = parse_expression(p);
    consume(p, TOKEN_DO, "Expected 'do' after while condition.");
    
    int body_count;
    Stmt **body = parse_block(p, &body_count);
    consume(p, TOKEN_END, "Expected 'end' after while body.");
    
    return stmt_while(condition, body, body_count, line);
}

static Stmt *parse_return_statement(Parser *p) {
    int line = p->previous.line;
    Expr *value = NULL;
    
    if (!check(p, TOKEN_END) && !check(p, TOKEN_ELSE) && !check(p, TOKEN_EOF)) {
        value = parse_expression(p);
    }
    
    return stmt_return(value, line);
}

static Stmt *parse_expression_statement(Parser *p) {
    int line = p->current.line;
    Expr *e = parse_expression(p);
    
    /* Check for assignment: name = value */
    /* This is handled in parse_expression for field assignment */
    
    return stmt_expr(e, line);
}

static Stmt *parse_statement(Parser *p) {
    if (match(p, TOKEN_VAR)) {
        return parse_var_statement(p);
    }
    if (match(p, TOKEN_IF)) {
        return parse_if_statement(p);
    }
    if (match(p, TOKEN_WHILE)) {
        return parse_while_statement(p);
    }
    if (match(p, TOKEN_RETURN)) {
        return parse_return_statement(p);
    }
    
    /* Check for simple assignment: identifier = value (NOT obj.field = value) */
    if (check(p, TOKEN_IDENTIFIER)) {
        /* Peek ahead: if it's IDENT = VALUE (not IDENT.FIELD = VALUE), handle here */
        Token saved_current = p->current;
        Token saved_previous = p->previous;
        const char *saved_lexer_start = p->lexer.start;
        const char *saved_lexer_current = p->lexer.current;
        int saved_lexer_line = p->lexer.line;
        int saved_lexer_newline = p->lexer.at_line_start;
        
        advance(p);
        char *name = copy_token_string(&p->previous);
        int line = p->previous.line;
        
        
        
        if (match(p, TOKEN_EQ)) {
            /* Simple assignment like: x = 42 */
            if (is_expression_boundary(p->current.type)) {
                error_currentf(p, "Expected expression to assign to '%s' after '='.", name);
                return stmt_assign(name, expr_nil(p->current.line), line);
            }
            Expr *value = parse_expression(p);
            return stmt_assign(name, value, line);
        }
        
        /* Not a simple assignment (maybe obj.field = or message send) */
        /* Restore EVERYTHING including lexer state */
        free(name);
        p->current = saved_current;
        p->previous = saved_previous;
        p->lexer.start = saved_lexer_start;
        p->lexer.current = saved_lexer_current;
        p->lexer.line = saved_lexer_line;
        p->lexer.at_line_start = saved_lexer_newline;
    }
    
    
    return parse_expression_statement(p);
}

static Stmt **parse_block(Parser *p, int *count) {
    Stmt **stmts = NULL;
    int capacity = 0;
    *count = 0;
    
    while (!check(p, TOKEN_END) && !check(p, TOKEN_ELSE) && 
           !check(p, TOKEN_EOF)) {
        if (*count >= capacity) {
            capacity = capacity == 0 ? 8 : capacity * 2;
            stmts = realloc(stmts, sizeof(Stmt*) * capacity);
        }
        stmts[(*count)++] = parse_statement(p);
    }
    
    return stmts;
}

/* =========================================================================
 * Method and Class Parsing
 * ========================================================================= */

static MethodDef *parse_method(Parser *p) {
    int line = p->previous.line;
    if (!consume(p, TOKEN_IDENTIFIER, "Expected method name after 'method'.")) {
        return method_def("_error", NULL, 0, NULL, 0, line);
    }
    char *name = copy_token_string(&p->previous);
    
    /* Parse parameters: method foo: a bar: b do ... end */
    char **params = NULL;
    int param_count = 0;
    int param_capacity = 0;
    
    /* Build selector name with colons */
    size_t name_len = strlen(name);
    size_t name_cap = name_len + 32;
    name = realloc(name, name_cap);
    
    while (match(p, TOKEN_COLON)) {
        /* Add colon to name */
        if (name_len + 1 >= name_cap) {
            name_cap *= 2;
            name = realloc(name, name_cap);
        }
        name[name_len++] = ':';
        name[name_len] = '\0';
        
        /* Get parameter name */
        if (!consume(p, TOKEN_IDENTIFIER, "Expected parameter name after ':' in method signature.")) {
            break;
        }
        if (param_count >= param_capacity) {
            param_capacity = param_capacity == 0 ? 4 : param_capacity * 2;
            params = realloc(params, sizeof(char*) * param_capacity);
        }
        params[param_count++] = copy_token_string(&p->previous);
    }
    
    if (!consume(p, TOKEN_DO, "Expected 'do' after method signature.")) {
        return method_def(name, params, param_count, NULL, 0, line);
    }
    
    int body_count;
    Stmt **body = parse_block(p, &body_count);
    
    if (check(p, TOKEN_END)) {
        advance(p);
    } else if (check(p, TOKEN_EOF)) {
        error_currentf(p, "Method '%s' started on line %d is missing its closing 'end'.", name, line);
    } else {
        consume(p, TOKEN_END, "Expected 'end' after method body.");
    }
    
    return method_def(name, params, param_count, body, body_count, line);
}

static ClassDef *parse_class(Parser *p) {
    int line = p->previous.line;
    if (!consume(p, TOKEN_IDENTIFIER, "Expected class name after 'class'.")) {
        return class_def("_error", NULL, 0, NULL, 0, line);
    }
    char *name = copy_token_string(&p->previous);
    
    char **fields = NULL;
    int field_count = 0;
    int field_capacity = 0;
    
    MethodDef **methods = NULL;
    int method_count = 0;
    int method_capacity = 0;
    
    while (!check(p, TOKEN_END) && !check(p, TOKEN_EOF)) {
        if (match(p, TOKEN_VAR)) {
            if (!consume(p, TOKEN_IDENTIFIER, "Expected field name after 'var' in class body.")) {
                continue;
            }
            if (field_count >= field_capacity) {
                field_capacity = field_capacity == 0 ? 8 : field_capacity * 2;
                fields = realloc(fields, sizeof(char*) * field_capacity);
            }
            fields[field_count++] = copy_token_string(&p->previous);
        }
        else if (match(p, TOKEN_METHOD)) {
            if (method_count >= method_capacity) {
                method_capacity = method_capacity == 0 ? 8 : method_capacity * 2;
                methods = realloc(methods, sizeof(MethodDef*) * method_capacity);
            }
            methods[method_count++] = parse_method(p);
        }
        else {
            error_current(p, "Expected 'var', 'method', or 'end' in class.");
            advance(p);
        }
    }
    
    if (check(p, TOKEN_END)) {
        advance(p);
    } else if (check(p, TOKEN_EOF)) {
        error_currentf(p, "Class '%s' started on line %d is missing its closing 'end'.", name, line);
    } else {
        consume(p, TOKEN_END, "Expected 'end' after class body.");
    }
    
    return class_def(name, fields, field_count, methods, method_count, line);
}

/* =========================================================================
 * Top-Level Parser
 * ========================================================================= */

Program *parse(const char *source) {
    Parser parser;
    lexer_init(&parser.lexer, source);
    parser.had_error = 0;
    parser.panic_mode = 0;
    parser.stop_at_keyword = 0;
    
    advance(&parser);  /* Prime the pump */
    
    Program *prog = program_new();
    
    while (!check(&parser, TOKEN_EOF)) {
        if (match(&parser, TOKEN_CLASS)) {
            program_add_class(prog, parse_class(&parser));
        }
        else if (match(&parser, TOKEN_METHOD)) {
            MethodDef *m = parse_method(&parser);
            if (strcmp(m->name, "main") == 0) {
                program_set_main(prog, m);
            } else {
                /* Free-standing method - could add to a global class */
                fprintf(stderr, "Warning: free-standing method '%s' ignored\n", m->name);
            }
        }
        else {
            error_current(&parser, "Expected 'class' or 'method' at top level.");
            advance(&parser);
        }
    }
    
    if (parser.had_error) {
        ast_free_program(prog);
        return NULL;
    }
    
    return prog;
}
