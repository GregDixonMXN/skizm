/*
 * token.h - Token Definitions
 * 
 * The lexer reads your source code and produces a stream of tokens.
 * Each token represents a meaningful unit: a keyword, identifier, number, etc.
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

typedef enum TokenType {
    /* Special */
    TOKEN_EOF = 0,
    TOKEN_ERROR,
    
    /* Literals */
    TOKEN_INT,          /* 42, -7, 1000 */
    TOKEN_FLOAT,        /* 3.14, 0.5 */
    TOKEN_STRING,       /* "hello" */
    TOKEN_IDENTIFIER,   /* foo, player, health */
    
    /* Keywords */
    TOKEN_CLASS,        /* class */
    TOKEN_METHOD,       /* method */
    TOKEN_VAR,          /* var */
    TOKEN_DO,           /* do */
    TOKEN_END,          /* end */
    TOKEN_IF,           /* if */
    TOKEN_ELSE,         /* else */
    TOKEN_WHILE,        /* while */
    TOKEN_RETURN,       /* return */
    TOKEN_SELF,         /* self */
    TOKEN_TRUE,         /* true */
    TOKEN_FALSE,        /* false */
    TOKEN_NIL,          /* nil */
    TOKEN_NEW,          /* new */
    TOKEN_AND,          /* and */
    TOKEN_OR,           /* or */
    TOKEN_NOT,          /* not */
    
    /* Operators */
    TOKEN_PLUS,         /* + */
    TOKEN_MINUS,        /* - */
    TOKEN_STAR,         /* * */
    TOKEN_SLASH,        /* / */
    TOKEN_EQ,           /* = */
    TOKEN_EQEQ,         /* == */
    TOKEN_BANGEQ,       /* != */
    TOKEN_LT,           /* < */
    TOKEN_GT,           /* > */
    TOKEN_LE,           /* <= */
    TOKEN_GE,           /* >= */
    
    /* Punctuation */
    TOKEN_LPAREN,       /* ( */
    TOKEN_RPAREN,       /* ) */
    TOKEN_LBRACKET,     /* [ */
    TOKEN_RBRACKET,     /* ] */
    TOKEN_DOT,          /* . */
    TOKEN_COLON,        /* : */
    TOKEN_COMMA,        /* , */
    
    TOKEN_COUNT
} TokenType;

typedef struct Token {
    TokenType type;
    const char *start;  /* Pointer into source */
    int length;         /* Length of lexeme */
    int line;           /* Line number (for errors) */
    int column;         /* Column number (for errors), 1-based */
    int after_newline;  /* Was there a newline before this token? */
} Token;

/* Get a human-readable name for a token type */
const char *token_type_name(TokenType type);

/* Print a token for debugging */
void token_print(Token *token);

#endif /* TOKEN_H */
