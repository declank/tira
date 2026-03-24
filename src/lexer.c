

#include "lexer.h"

Token lex_next(Lexer *L) {
    Token tok = {0};
    
    // Skip whitespace and comments
    b32 has_newline_in_block_comment = false;
    for (;;) {
        while (!lex_eof(L) && (is_space(lex_peek(L)))) {
            lex_advance(L);
        }

        if (is_line_comment_start(L))  { skip_line_comment(L); continue; } 
        if (is_block_comment_start(L)) { has_newline_in_block_comment = skip_block_comment(L); continue; }
        // TODO should we break instead above to handle the newline token

        break;
    }

    tok.start = L->pos;
    tok.line = L->line;

    if (has_newline_in_block_comment) { tok.type = T_NEWLINE; return tok; }
    if (lex_eof(L)) { tok.type = T_END; return tok; }

    char c = lex_advance(L);
    
    if (c == '\n') {
        L->line++;
        L->col = 1;
        L->at_line_start = false;

        tok.type = T_NEWLINE;
        tok.length = L->pos - tok.start;
        return tok;    
    }

    switch (c) {
        // New line
        case '\n': { 
            tok.type = T_NEWLINE;

        } break;

        // Single character
        case ';': { tok.type = T_SEMICOLON; } break;
        case ',': { tok.type = T_COMMA; } break;
        case '{': { tok.type = T_LBRACE; } break;
        case '}': { tok.type = T_RBRACE; } break;
        case '(': { tok.type = T_LPAREN; } break;
        case ')': { tok.type = T_RPAREN; } break;
        case '[': { tok.type = T_LSQBRACKET; } break;
        case ']': { tok.type = T_RSQBRACKET; } break;

        case '+': { tok.type = T_PLUS; } break;
        case '-': { tok.type = T_MINUS; } break;
        case '*': { tok.type = T_ASTERISK; } break;
        case '/': { tok.type = T_SLASH; } break;
        case '^': { tok.type = T_XOR; } break;
        case '?': { tok.type = T_QUESTION; } break;

        // Multi-character
        case '.': { 
            tok.type = T_DOT;
            switch (lex_peek(L)) {
                case '.': { 
                    tok.type = T_DOTDOT; lex_advance(L);
                    if (lex_peek(L) == '=') { tok.type = T_DOTDOTEQ; lex_advance(L); }
                } break;
            }
        } break;

        case '=': {
            tok.type = T_EQUALS;
            switch (lex_peek(L)) {
                case '=': { tok.type = T_EQEQ; lex_advance(L); } break;
                case '>': { tok.type = T_EQUALS_RARROW; lex_advance(L); } break;
            }
        } break;

        case '!': { 
            tok.type = T_NOT;
            switch (lex_peek(L)) {
                case '=': { tok.type = T_NOT_EQ; lex_advance(L); } break;
            } 
        } break;

        case '<': {
            tok.type = T_LT;
            switch (lex_peek(L)) {
                case '=': { tok.type = T_LTEQ; lex_advance(L); } break;
            }
        } break;
        case '>': {
            tok.type = T_GT;
            switch (lex_peek(L)) {
                case '=': { tok.type = T_GTEQ; lex_advance(L); } break;
            }
        } break;
        case '|': {
            tok.type = T_BITWISE_OR;
            switch (lex_peek(L)) {
                case '|': { tok.type = T_LOGICAL_OR; lex_advance(L); } break;
                case '>': { tok.type = T_PIPE; lex_advance(L); } break;
            }
        } break;
        case '&': {
            tok.type = T_AMPERSAND;
            switch (lex_peek(L)) {
                case '&': { tok.type = T_LOGICAL_AND; lex_advance(L); } break;
            }
        } break;
        case ':': {
            tok.type = T_COLON;
            switch (lex_peek(L)) {
                case '=': { tok.type = T_COLON_EQ; lex_advance(L); } break;
                case ':': { tok.type = T_DOUBLE_COLON; lex_advance(L); } break;
            }
        } break;

        // Strings
        case '"': {
            while_lex(lex_peek(L) != '"');
            lex_advance(L);
            tok.type = T_STRING;
        } break;

        case '%': {
            // modulo
            tok.type = T_PERCENT;
            // compile-time directive
            if (L->at_line_start) {
                while_lex(is_alnum(lex_peek(L)) || lex_peek(L) == '_');
                if (L->pos > tok.start + 1) tok.type = T_CT_DIRECTIVE;
            }
        } break;


        default: {
            if (is_alpha(c) || c == '_') {
                while_lex(is_alnum(lex_peek(L)) || lex_peek(L) == '_');
                tok.type = keyword_or_identifier(L->input.data + tok.start, L->pos - tok.start);
            } else if (is_digit(c)) {
                char next = lex_peek(L);
                if (c == '0' && (next == 'x' || next == 'X')) {
                    lex_advance(L);

                    if (!is_hex_digit(lex_peek(L))) {
                        tok.type = T_INVALID;
                        break;
                    }

                    while_lex(is_hex_digit(lex_peek(L)));
                    tok.type = T_NUM;
                } else {
                    while_lex(is_digit(lex_peek(L)));
                    tok.type = T_NUM;
                }
            } else {
                tok.type = T_INVALID; // Not necessary due to ZII but put in for code clarity
            }

        } break;
    }

    L->at_line_start = false;
    tok.length = L->pos - tok.start;
    return tok;
}
