/*
 * lexer.c - Lexical Analyzer Implementation
 * 
 * Reads your clean Lua-inspired syntax and produces tokens.
 */

#include "lexer.h"
#include <string.h>
#include <ctype.h>

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source_start = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->at_line_start = 1;
}

/* =========================================================================
 * Helper Functions
 * ========================================================================= */

static int is_at_end(Lexer *lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer *lexer) {
    return *lexer->current++;
}

static char peek(Lexer *lexer) {
    return *lexer->current;
}

static char peek_next(Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static int match(Lexer *lexer, char expected) {
    if (is_at_end(lexer)) return 0;
    if (*lexer->current != expected) return 0;
    lexer->current++;
    return 1;
}

static Token make_token(Lexer *lexer, TokenType type) {
    const char *line_start = lexer->start;
    while (line_start > lexer->source_start && line_start[-1] != '\n') {
        line_start--;
    }

    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = (int)(lexer->start - line_start) + 1;
    token.after_newline = lexer->at_line_start;
    lexer->at_line_start = 0;  /* Reset after creating token */
    return token;
}

static Token error_token(Lexer *lexer, const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    token.column = 1;
    token.after_newline = lexer->at_line_start;
    lexer->at_line_start = 0;
    return token;
}

static void skip_whitespace(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                lexer->at_line_start = 1;  /* Mark that we crossed a newline */
                advance(lexer);
                break;
            case '-':
                /* Check for -- comment */
                if (peek_next(lexer) == '-') {
                    /* Skip to end of line */
                    while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                        advance(lexer);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* =========================================================================
 * Keyword Detection
 * ========================================================================= */

static TokenType check_keyword(Lexer *lexer, int start, int length,
                               const char *rest, TokenType type) {
    if (lexer->current - lexer->start == start + length &&
        memcmp(lexer->start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(Lexer *lexer) {
    /* Trie-based keyword matching */
    switch (lexer->start[0]) {
        case 'a': return check_keyword(lexer, 1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(lexer, 1, 4, "lass", TOKEN_CLASS);
        case 'd': return check_keyword(lexer, 1, 1, "o", TOKEN_DO);
        case 'e':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'l': return check_keyword(lexer, 2, 2, "se", TOKEN_ELSE);
                    case 'n': return check_keyword(lexer, 2, 1, "d", TOKEN_END);
                }
            }
            break;
        case 'f': return check_keyword(lexer, 1, 4, "alse", TOKEN_FALSE);
        case 'i': return check_keyword(lexer, 1, 1, "f", TOKEN_IF);
        case 'm': return check_keyword(lexer, 1, 5, "ethod", TOKEN_METHOD);
        case 'n':
            if (lexer->current - lexer->start > 1) {
                switch (lexer->start[1]) {
                    case 'e': return check_keyword(lexer, 2, 1, "w", TOKEN_NEW);
                    case 'i': return check_keyword(lexer, 2, 1, "l", TOKEN_NIL);
                    case 'o': return check_keyword(lexer, 2, 1, "t", TOKEN_NOT);
                }
            }
            break;
        case 'o': return check_keyword(lexer, 1, 1, "r", TOKEN_OR);
        case 'r': return check_keyword(lexer, 1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(lexer, 1, 3, "elf", TOKEN_SELF);
        case 't': return check_keyword(lexer, 1, 3, "rue", TOKEN_TRUE);
        case 'v': return check_keyword(lexer, 1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(lexer, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

/* =========================================================================
 * Token Scanning
 * ========================================================================= */

static Token scan_identifier(Lexer *lexer) {
    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    return make_token(lexer, identifier_type(lexer));
}

static Token scan_number(Lexer *lexer) {
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
    
    /* Look for decimal part */
    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer);  /* Consume '.' */
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, TOKEN_FLOAT);
    }
    
    return make_token(lexer, TOKEN_INT);
}

static Token scan_string(Lexer *lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\\') {
            advance(lexer);
            if (!is_at_end(lexer)) {
                if (peek(lexer) == '\n') lexer->line++;
                advance(lexer);
            }
            continue;
        }
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }
    
    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string.");
    }
    
    advance(lexer);  /* Closing quote */
    return make_token(lexer, TOKEN_STRING);
}

Token lexer_next(Lexer *lexer) {
    skip_whitespace(lexer);
    lexer->start = lexer->current;
    
    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }
    
    char c = advance(lexer);
    
    /* Identifiers and keywords */
    if (isalpha(c) || c == '_') {
        return scan_identifier(lexer);
    }
    
    /* Numbers */
    if (isdigit(c)) {
        return scan_number(lexer);
    }
    
    /* Other tokens */
    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '[': return make_token(lexer, TOKEN_LBRACKET);
        case ']': return make_token(lexer, TOKEN_RBRACKET);
        case '.': return make_token(lexer, TOKEN_DOT);
        case ':': return make_token(lexer, TOKEN_COLON);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '+': return make_token(lexer, TOKEN_PLUS);
        case '-': return make_token(lexer, TOKEN_MINUS);
        case '*': return make_token(lexer, TOKEN_STAR);
        case '/': return make_token(lexer, TOKEN_SLASH);
        
        case '=':
            return make_token(lexer, match(lexer, '=') ? TOKEN_EQEQ : TOKEN_EQ);
        case '!':
            return match(lexer, '=') ? make_token(lexer, TOKEN_BANGEQ)
                                     : error_token(lexer, "Expected '=' after '!'");
        case '<':
            return make_token(lexer, match(lexer, '=') ? TOKEN_LE : TOKEN_LT);
        case '>':
            return make_token(lexer, match(lexer, '=') ? TOKEN_GE : TOKEN_GT);
        
        case '"': return scan_string(lexer);
    }
    
    return error_token(lexer, "Unexpected character.");
}

Token lexer_peek(Lexer *lexer) {
    /* Save state */
    const char *start = lexer->start;
    const char *current = lexer->current;
    int line = lexer->line;
    
    Token token = lexer_next(lexer);
    
    /* Restore state */
    lexer->start = start;
    lexer->current = current;
    lexer->line = line;
    
    return token;
}
