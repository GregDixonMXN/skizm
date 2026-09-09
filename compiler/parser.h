/*
 * parser.h - Recursive Descent Parser
 * 
 * Reads tokens from the lexer and builds an AST.
 * 
 * Grammar (simplified):
 *     program     → (class_def | method_def)* EOF
 *     class_def   → "class" IDENTIFIER (var_decl | method_def)* "end"
 *     method_def  → "method" IDENTIFIER (":" IDENTIFIER)* "do" statement* "end"
 *     statement   → var_decl | assignment | if_stmt | while_stmt | return_stmt | expr_stmt
 *     expression  → (standard expression grammar with message sends)
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct Parser {
    Lexer lexer;
    Token current;
    Token previous;
    int had_error;
    int panic_mode;
    int stop_at_keyword;
} Parser;

/* Parse source code and return the AST */
Program *parse(const char *source);

#endif /* PARSER_H */
