/*
 * token.c - Token Utilities
 */

#include "token.h"
#include <stdio.h>

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:        return "EOF";
        case TOKEN_ERROR:      return "ERROR";
        case TOKEN_INT:        return "INT";
        case TOKEN_FLOAT:      return "FLOAT";
        case TOKEN_STRING:     return "STRING";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_CLASS:      return "CLASS";
        case TOKEN_METHOD:     return "METHOD";
        case TOKEN_VAR:        return "VAR";
        case TOKEN_DO:         return "DO";
        case TOKEN_END:        return "END";
        case TOKEN_IF:         return "IF";
        case TOKEN_ELSE:       return "ELSE";
        case TOKEN_WHILE:      return "WHILE";
        case TOKEN_RETURN:     return "RETURN";
        case TOKEN_SELF:       return "SELF";
        case TOKEN_TRUE:       return "TRUE";
        case TOKEN_FALSE:      return "FALSE";
        case TOKEN_NIL:        return "NIL";
        case TOKEN_NEW:        return "NEW";
        case TOKEN_AND:        return "AND";
        case TOKEN_OR:         return "OR";
        case TOKEN_NOT:        return "NOT";
        case TOKEN_PLUS:       return "PLUS";
        case TOKEN_MINUS:      return "MINUS";
        case TOKEN_STAR:       return "STAR";
        case TOKEN_SLASH:      return "SLASH";
        case TOKEN_EQ:         return "EQ";
        case TOKEN_EQEQ:       return "EQEQ";
        case TOKEN_BANGEQ:     return "BANGEQ";
        case TOKEN_LT:         return "LT";
        case TOKEN_GT:         return "GT";
        case TOKEN_LE:         return "LE";
        case TOKEN_GE:         return "GE";
        case TOKEN_LPAREN:     return "LPAREN";
        case TOKEN_RPAREN:     return "RPAREN";
        case TOKEN_LBRACKET:   return "LBRACKET";
        case TOKEN_RBRACKET:   return "RBRACKET";
        case TOKEN_DOT:        return "DOT";
        case TOKEN_COLON:      return "COLON";
        case TOKEN_COMMA:      return "COMMA";
        default:               return "UNKNOWN";
    }
}

void token_print(Token *token) {
    printf("%-12s '%.*s' (line %d, col %d)\n",
           token_type_name(token->type),
           token->length,
           token->start,
           token->line,
           token->column);
}
