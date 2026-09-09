/*
 * lexer.h - Lexical Analyzer
 * 
 * The lexer reads source code character by character and produces tokens.
 * 
 * Usage:
 *     Lexer lexer;
 *     lexer_init(&lexer, source_code);
 *     
 *     Token token;
 *     while ((token = lexer_next(&lexer)).type != TOKEN_EOF) {
 *         // process token
 *     }
 */

#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct Lexer {
    const char *source_start; /* Start of source buffer */
    const char *start;    /* Start of current lexeme */
    const char *current;  /* Current position in source */
    int line;             /* Current line number */
    int at_line_start;    /* Did we just pass a newline? */
} Lexer;

/* Initialize lexer with source code */
void lexer_init(Lexer *lexer, const char *source);

/* Get the next token */
Token lexer_next(Lexer *lexer);

/* Peek at the next token without consuming it */
Token lexer_peek(Lexer *lexer);

#endif /* LEXER_H */
