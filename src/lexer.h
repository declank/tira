#pragma once

#include <stdbool.h>

#include "common.h"
#include "memory.h"
#include "platform.h"
#include "string.h"

#define X_TOKEN_TYPES \
    X(INVALID) X(IDENT) X(NUM) X(STRING) X(SEMICOLON) X(COLON) X(DOUBLE_COLON) X(COLON_EQ) \
    X(EQUALS) X(EQEQ) X(NOT_EQ) X(COMMA) X(DOTDOT) X(DOTDOTEQ) X(DOT) X(QUESTION) \
    X(LBRACE) X(RBRACE) X(LSQBRACKET) X(RSQBRACKET) X(LPAREN) X(RPAREN) X(PIPE) \
    X(LOGICAL_OR) X(BITWISE_OR) X(LOGICAL_AND) X(AMPERSAND) X(XOR) X(NOT) \
    X(LT) X(LTEQ) X(GT) X(GTEQ) \
    X(PLUS) X(MINUS) X(ASTERISK) X(SLASH) X(PERCENT) \
    \
    X(VAR) X(CONST) X(IMPORT) X(CFFI) X(FUNC) X(TRUE) X(FALSE) X(RETURN) \
    X(IF) X(ELSE) X(CASE) X(MAIN) X(FOR) X(IN) \
    X(STRUCT) X(NIL) X(TYPE) X(CAST) X(REF) \
    \
    X(EQUALS_RARROW) \
    X(CT_DIRECTIVE) \
    X(NEWLINE) \
    X(END) \
    X(COUNT)

/* #define X(TYPE) T_##TYPE, 
typedef enum {
    X_TOKEN_TYPES
} TokenType;
#undef X */


typedef enum {
#define X(TYPE) T_##TYPE,
    X_TOKEN_TYPES
#undef X
} TokenType;

//typedef uint8_t TokenType;
//_Static_assert(T_COUNT < UINT8_MAX, "Lex token count should fit in uint8_t");

#define X(TYPE) S(#TYPE),
static String token_type_strings[] = {
    X_TOKEN_TYPES
};
#undef X

typedef struct {
    TokenType type;
    size_t start;
    size_t length;
    size_t line;
} Token;

typedef struct {
    Token *tokens;
    size_t size;
    size_t cap;

    String input;
    size_t pos;
    bool at_line_start;

    size_t line;
    size_t col;
} Lexer;

static inline int is_space(int c) {    
    return c == ' '  || c == '\f' || 
           c == '\r' || c == '\t' || c == '\v';
}

static inline int is_digit(int c) {
    return c >= '0' && c <= '9';
}

static inline int is_alpha(int c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z');
}

static inline int is_alnum(int c) {
    return is_alpha(c) || is_digit(c);
}

static TokenType keyword_or_identifier(char *str, size_t n) {
    //if (n==6 && !memcmp(str, "import", 6))  return T_IMPORT;
    if (n==4 && !memcmp(str, "func", 4))    return T_FUNC;
    if (n==4 && !memcmp(str, "true", 4))    return T_TRUE;
    if (n==5 && !memcmp(str, "false", 5))   return T_FALSE;
    if (n==2 && !memcmp(str, "if", 2))      return T_IF;
    if (n==4 && !memcmp(str, "else", 4))    return T_ELSE;
    if (n==6 && !memcmp(str, "struct", 6))  return T_STRUCT;
    if (n==4 && !memcmp(str, "case", 4))    return T_CASE;
    if (n==4 && !memcmp(str, "cffi", 4))    return T_CFFI;
    if (n==6 && !memcmp(str, "return", 6))  return T_RETURN;
    if (n==4 && !memcmp(str, "main", 4))    return T_MAIN;
    if (n==3 && !memcmp(str, "nil", 3))     return T_NIL;
    if (n==3 && !memcmp(str, "for", 3))     return T_FOR;
    if (n==2 && !memcmp(str, "in", 2))      return T_IN;
    if (n==4 && !memcmp(str, "cast", 4))    return T_CAST;
    if (n==3 && !memcmp(str, "ref", 3))     return T_REF;
    if (n==3 && !memcmp(str, "var", 3))     return T_VAR;
    if (n==5 && !memcmp(str, "const", 5))   return T_CONST;
    //if (n==4 && !memcmp(str, "type", 4))    return T_TYPE;
    return T_IDENT;
}

static inline b32 lex_eof(Lexer *lexer) {
    // TODO: signed vs unsigned sizes
    return lexer->pos >= lexer->input.len;
}

static inline char lex_advance(Lexer *lexer) {
    if (lex_eof(lexer)) { return '\0'; }

    char c = lexer->input.data[lexer->pos++];
    lexer->col++;

    return c;
}

static inline char lex_peek(Lexer *lexer) {
    return lex_eof(lexer) ? '\0' : lexer->input.data[lexer->pos];
}

static inline b32 is_line_comment_start(Lexer *L) {
    return lex_peek(L) == '/' 
        && (L->pos + 1) < L->input.len
        && L->input.data[L->pos + 1] == '/'; 
}

static inline b32 is_block_comment_start(Lexer *L) {
    return lex_peek(L) == '/'
        && (L->pos + 1) < L->input.len
        && L->input.data[L->pos + 1] == '*';
}

static void skip_line_comment(Lexer *L) {
    // consume //
    lex_advance(L); lex_advance(L);

    while (!lex_eof(L)) {
        char c = lex_peek(L);
        if (c == '\n') break;
        lex_advance(L);
    }
}

static b32 skip_block_comment(Lexer *L) {
    // consume /*
    lex_advance(L); lex_advance(L);

    int depth = 1;
    b32 has_newline = false;

    while (!lex_eof(L) && depth > 0) {
        char c = lex_advance(L);

        if (c == '\n') { has_newline = true; L->line++; L->col = 1; continue; }
        if (c == '/' && lex_peek(L) == '*') { depth++; lex_advance(L); continue; }
        if (c == '*' && lex_peek(L) == '/') { depth--; lex_advance(L); continue; }
    }

    if (depth > 0) {
        // TODO: error
    }

    return has_newline;
}

#define while_lex(cond) while (!lex_eof(L) && (cond)) { lex_advance(L); }

Token lex_next(Lexer *L);

static inline bool is_hex_digit(char c) {
    return is_digit(c) ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

