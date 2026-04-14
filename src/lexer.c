

#define X_TOKEN_TYPES \
    X(INVALID) X(IDENT) X(NUM) X(DOLLAR) X(STRING) X(CHARACTER) X(SEMICOLON) X(COLON) X(DOUBLE_COLON) X(COLON_EQ) \
    X(EQUALS) X(EQEQ) X(NOT_EQ) X(COMMA) X(DOTDOT) X(DOTDOTEQ) X(DOT) X(QUESTION) \
    X(LBRACE) X(RBRACE) X(LSQBRACKET) X(RSQBRACKET) X(LPAREN) X(RPAREN) X(PIPE) \
    X(LOGICAL_OR) X(BITWISE_OR) X(LOGICAL_AND) X(AMPERSAND) X(XOR) X(NOT) \
    X(LT) X(LTEQ) X(GT) X(GTEQ) \
    X(PLUS) X(MINUS) X(ASTERISK) X(SLASH) X(PERCENT) \
    \
    X(VAR) X(CONST) X(IMPORT) X(CFFI) X(FUNC) X(TRUE) X(FALSE) X(RETURN) \
    X(IF) X(ELSE) X(CASE) X(MAIN) X(FOR) X(IN) X(WHILE) \
    X(STRUCT) X(NIL) X(TYPE) X(REF) \
    \
    X(EQUALS_RARROW) \
    X(CT_DIRECTIVE) \
    X(NEWLINE) \
    X(END) \
    X(COUNT)


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

/* typedef struct {
    uint64_t offset;
    uint64_t hash;
    uint32_t len;
    TokenType type;
} InternSlot; */

typedef struct {
    const char *source; // pointer into source buffer
    uint64_t    hash;
    uint32_t    len;
    TokenType   type;
} InternSlot;

typedef struct {
    InternSlot *slots;
    uint64_t    count;
    uint64_t    cap;
    Arena      *arena;
    uint32_t    grow_threshold; // precomputed: cap * load_factor
    float       load_factor;
} InternTable;

typedef struct {
    Token *tokens;
    size_t size;
    size_t cap;

    String input;
    size_t pos;
    bool at_line_start;

    size_t line;
    size_t col;

    InternTable table;
} Lexer;

typedef struct {
    const char *str;
    uint32_t len;
    TokenType type;
} KeywordEntry;

static const KeywordEntry lexer_keywords[] = {
    { "func",   4, T_FUNC   },
    { "true",   4, T_TRUE   },
    { "false",  5, T_FALSE  },
    { "if",     2, T_IF     },
    { "else",   4, T_ELSE   },
    { "struct", 6, T_STRUCT },
    { "case",   4, T_CASE   },
    { "cffi",   4, T_CFFI   },
    { "return", 6, T_RETURN },
    { "main",   4, T_MAIN   },
    { "nil",    3, T_NIL    },
    { "for",    3, T_FOR    },
    { "in",     2, T_IN     },
    { "ref",    3, T_REF    },
    { "var",    3, T_VAR    },
    { "const",  5, T_CONST  },
    { "while",  5, T_WHILE  },
};

static int lexer_keyword_count = countof(lexer_keywords);



static uint64_t kw_identifier_hash(const char *str, uint32_t len) { // fnv1a
    uint64_t h = 14695981039346656037ull;
    for (uint32_t i = 0; i < len; i++) {
        h ^= (uint8_t)str[i];
        h *= 1099511628211ull;
    }
    return h;
}

static void intern_table_grow(InternTable *t) {
    uint64_t    new_cap   = t->cap * 2;
    InternSlot *new_slots = new(t->arena, InternSlot, new_cap);
    uint32_t    new_mask  = new_cap - 1;
    
    // Rehash existing entries into new table
    for (uint64_t i = 0; i < t->cap; i++) {
        InternSlot *s = &t->slots[i];
        if (!s->hash) continue;
        uint64_t slot = (s->hash & new_mask);
        while (new_slots[slot].hash) {
            slot = (slot + 1) & new_mask;
        }
        new_slots[slot] = *s;
    }

    t->slots = new_slots;
    t->cap = new_cap;
    t->grow_threshold = (uint32_t)(t->cap * t->load_factor);
}

static InternSlot *intern_insert(InternTable *t, const char *source, uint32_t len, TokenType type) {
    /* if ((float)t->count / (float)t->cap >= t->load_factor) 
        intern_table_grow(t); */
    if (UNLIKELY(t->count >= t->grow_threshold))
        intern_table_grow(t);

    uint64_t hash = kw_identifier_hash(source, len);
    uint64_t mask = t->cap - 1;
    uint64_t slot = hash & mask;

    while (t->slots[slot].hash) {
        if (t->slots[slot].hash == hash &&
            t->slots[slot].len  == len  &&
            memcmp(source, t->slots[slot].source, len) == 0)
            return &t->slots[slot];
        slot = (slot + 1) & mask;
    }

    t->slots[slot] = (InternSlot){
        .hash = hash,
        .len = len,
        //.offset = 0, //offset,
        .source = source,
        .type = type,
    };
    t->count++;

    return &t->slots[slot];
}

static uint64_t next_pow2(uint64_t n) {
    if (n < 2) return 1;
    return (uint64_t)1 << (64 - __builtin_clzll(n - 1));
}

static void intern_table_init(Lexer *l, Arena *arena, uint32_t initial_cap) {
    l->table.arena          = arena;
    l->table.cap            = next_pow2(initial_cap);
    l->table.count          = 0;
    l->table.load_factor    = .75f;
    l->table.grow_threshold = (uint32_t)(l->table.cap * l->table.load_factor); // make sure to update when cap changes!!
    l->table.slots          = new(arena, InternSlot, l->table.cap);

    for (int i = 0; i < lexer_keyword_count; i++) {
        intern_insert(&l->table, lexer_keywords[i].str, lexer_keywords[i].len, lexer_keywords[i].type);
    }
}

static TokenType keyword_or_identifier2(InternTable *t, const char *source, uint32_t len) {
    InternSlot *slot = intern_insert(t, source, len, T_IDENT); // default to identifier unless found
    return slot->type;
}

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
    if (n==3 && !memcmp(str, "ref", 3))     return T_REF;
    if (n==3 && !memcmp(str, "var", 3))     return T_VAR;
    if (n==5 && !memcmp(str, "const", 5))   return T_CONST;
    if (n==5 && !memcmp(str, "while", 5))   return T_WHILE;
    //if (n==4 && !memcmp(str, "type", 4))    return T_TYPE;
    return T_IDENT;
}

static inline b32 lex_eof(Lexer *lexer) {
    // TODO: signed vs unsigned sizes
    return lexer->pos >= lexer->input.len;
}

static inline char lex_advance(Lexer *lexer) {
    //if (lex_eof(lexer)) { return '\0'; }

    char c = lexer->input.data[lexer->pos++];
    //lexer->col++; // TODO removed to reduce instructions in hotpath

    return c;
}

static inline char lex_peek(Lexer *lexer) {
    return lex_eof(lexer) ? '\0' : lexer->input.data[lexer->pos];
}

/* static inline void lex_scan_identifier(Lexer *L) {
    const char *cur = L->input.data + L->pos;
    const char *end = L->input.data + L->input.len;
    while (cur < end && (is_alnum(*cur) || *cur == '_')) cur++;
    L->pos = cur - L->input.data;
} */

static const uint8_t is_ident_char[256] = {
    ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1,
    ['f'] = 1, ['g'] = 1, ['h'] = 1, ['i'] = 1, ['j'] = 1,
    ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1, ['o'] = 1,
    ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1,
    ['u'] = 1, ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1,
    ['z'] = 1,
    ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1,
    ['F'] = 1, ['G'] = 1, ['H'] = 1, ['I'] = 1, ['J'] = 1,
    ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1, ['O'] = 1,
    ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1,
    ['U'] = 1, ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1,
    ['Z'] = 1,
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1,
    ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
    ['_'] = 1,
};

static inline void lex_scan_identifier(Lexer *L) {
    const char *cur = L->input.data + L->pos;
    while (is_ident_char[(uint8_t)*cur]) cur++;
    L->pos = cur - L->input.data;
}

static inline void lex_scan_digits(Lexer *L) {
    const char *cur = L->input.data + L->pos;
    const char *end = L->input.data + L->input.len;
    while (cur < end && is_digit(*cur)) cur++;
    L->pos = cur - L->input.data;
}

static inline void lex_scan_string(Lexer *L) {
    const char *cur = L->input.data + L->pos;
    const char *end = L->input.data + L->input.len;
    while (cur < end && *cur != '"') cur++;
    L->pos = cur - L->input.data;
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

/* static void skip_line_comment(Lexer *L) {
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
 */


static void skip_line_comment(Lexer *L) {
    const char *src = L->input.data;
    L->pos += 2; // skip //
    while (src[L->pos] != '\n' && src[L->pos] != '\0') L->pos++;
}

static b32 skip_block_comment(Lexer *L) {
    const char *src = L->input.data;
    L->pos += 2; // skip /*

    int depth = 1;
    b32 has_newline = false;

    while (src[L->pos] != '\0' && depth > 0) {
        char c = src[L->pos++];
        if (c == '\n') {
            has_newline = true;
        } else if (c == '/' && src[L->pos] == '*') {
            depth++; L->pos++;
        } else if (c == '*' && src[L->pos] == '/') {
            depth--; L->pos++;
        }
    }

    if (depth > 0) {
        // TODO: error — unterminated block comment
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

typedef enum {
    C_OTHER,      // unknown/invalid
    C_SPACE,      // space, tab, \r
    C_NEWLINE,    // \n
    C_ALPHA,      // a-z, A-Z
    C_DIGIT,      // 1-9
    C_ZERO,       // 0 (special — hex prefix)
    C_UNDERSCORE, // _
    C_DQUOTE,     // "
    C_PLUS,       // +
    C_MINUS,      // -
    C_STAR,       // *
    C_SLASH,      // /
    C_PERCENT,    // %
    C_EQUALS,     // =
    C_BANG,       // !
    C_LT,         // 
    C_GT,         // >
    C_AMP,        // &
    C_PIPE,       // |
    C_CARET,      // ^
    C_DOT,        // .
    C_COLON,      // :
    C_SEMICOLON,  // ;
    C_COMMA,      // ,
    C_LPAREN,     // (
    C_RPAREN,     // )
    C_LBRACE,     // {
    C_RBRACE,     // }
    C_LSQBRACKET, // [
    C_RSQBRACKET, // ]
    C_QUESTION,   // ?
    C_HASH,       // #
    C_DOLLAR,     // $
    C_EOF,        // \0
} CharClass;

static const uint8_t char_class[256] = {
    // 0x00-0x08
    C_EOF, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER,
    // 0x09 \t, 0x0a \n, 0x0b, 0x0c, 0x0d \r
    C_SPACE, C_NEWLINE, C_OTHER, C_OTHER, C_SPACE,
    // 0x0e-0x1f
    C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER,
    C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER, C_OTHER,
    C_OTHER, C_OTHER,
    // 0x20 space
    [' ']  = C_SPACE,
    ['\n'] = C_NEWLINE, // needed for cross platform?
    ['!']  = C_BANG,
    ['"']  = C_DQUOTE,
    ['#']  = C_HASH,
    ['$']  = C_DOLLAR,
    ['%']  = C_PERCENT,
    ['&']  = C_AMP,
    ['(']  = C_LPAREN,
    [')']  = C_RPAREN,
    ['*']  = C_STAR,
    ['+']  = C_PLUS,
    [',']  = C_COMMA,
    ['-']  = C_MINUS,
    ['.']  = C_DOT,
    ['/']  = C_SLASH,
    ['0']  = C_ZERO,
    ['1']  = C_DIGIT, ['2'] = C_DIGIT, ['3'] = C_DIGIT, ['4'] = C_DIGIT,
    ['5']  = C_DIGIT, ['6'] = C_DIGIT, ['7'] = C_DIGIT, ['8'] = C_DIGIT,
    ['9']  = C_DIGIT,
    [':']  = C_COLON,
    [';']  = C_SEMICOLON,
    ['<']  = C_LT,
    ['=']  = C_EQUALS,
    ['>']  = C_GT,
    ['?']  = C_QUESTION,
    ['A']  = C_ALPHA, ['B'] = C_ALPHA, ['C'] = C_ALPHA, ['D'] = C_ALPHA,
    ['E']  = C_ALPHA, ['F'] = C_ALPHA, ['G'] = C_ALPHA, ['H'] = C_ALPHA,
    ['I']  = C_ALPHA, ['J'] = C_ALPHA, ['K'] = C_ALPHA, ['L'] = C_ALPHA,
    ['M']  = C_ALPHA, ['N'] = C_ALPHA, ['O'] = C_ALPHA, ['P'] = C_ALPHA,
    ['Q']  = C_ALPHA, ['R'] = C_ALPHA, ['S'] = C_ALPHA, ['T'] = C_ALPHA,
    ['U']  = C_ALPHA, ['V'] = C_ALPHA, ['W'] = C_ALPHA, ['X'] = C_ALPHA,
    ['Y']  = C_ALPHA, ['Z'] = C_ALPHA,
    ['[']  = C_LSQBRACKET,
    [']']  = C_RSQBRACKET,
    ['^']  = C_CARET,
    ['_']  = C_UNDERSCORE,
    ['a']  = C_ALPHA, ['b'] = C_ALPHA, ['c'] = C_ALPHA, ['d'] = C_ALPHA,
    ['e']  = C_ALPHA, ['f'] = C_ALPHA, ['g'] = C_ALPHA, ['h'] = C_ALPHA,
    ['i']  = C_ALPHA, ['j'] = C_ALPHA, ['k'] = C_ALPHA, ['l'] = C_ALPHA,
    ['m']  = C_ALPHA, ['n'] = C_ALPHA, ['o'] = C_ALPHA, ['p'] = C_ALPHA,
    ['q']  = C_ALPHA, ['r'] = C_ALPHA, ['s'] = C_ALPHA, ['t'] = C_ALPHA,
    ['u']  = C_ALPHA, ['v'] = C_ALPHA, ['w'] = C_ALPHA, ['x'] = C_ALPHA,
    ['y']  = C_ALPHA, ['z'] = C_ALPHA,
    ['{']  = C_LBRACE,
    ['|']  = C_PIPE,
    ['}']  = C_RBRACE,
};

Token lex_next(Lexer *L) {
    Token tok = {0};
    
    // Skip whitespace and comments
    const char *src = L->input.data;
    b32 has_newline_in_block_comment = false;
    /* for (;;) {
        while (!lex_eof(L) && (is_space(lex_peek(L)))) {
            lex_advance(L);
        }

        if (is_line_comment_start(L))  { skip_line_comment(L); continue; } 
        if (is_block_comment_start(L)) { has_newline_in_block_comment = skip_block_comment(L); continue; }
        // TODO should we break instead above to handle the newline token

        break;
    } */
    
    for (;;) {
        // skip spaces and tabs inline — no function calls
        while (char_class[(uint8_t)src[L->pos]] == C_SPACE) L->pos++;

        if (src[L->pos] == '/') {
            if (src[L->pos + 1] == '/') { skip_line_comment(L); continue; }
            if (src[L->pos + 1] == '*') { has_newline_in_block_comment = skip_block_comment(L); continue; }
        }
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

        // Single character
        case ';': { tok.type = T_SEMICOLON; } break;
        case ',': { tok.type = T_COMMA; } break;
        case '{': { tok.type = T_LBRACE; } break;
        case '}': { tok.type = T_RBRACE; } break;
        case '(': { tok.type = T_LPAREN; } break;
        case ')': { tok.type = T_RPAREN; } break;
        case '[': { tok.type = T_LSQBRACKET; } break;
        case ']': { tok.type = T_RSQBRACKET; } break;
        case '$': { tok.type = T_DOLLAR; } break;

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

        // Character value
        // TODO cleanup case: #\space or #\tab
        case '#': {
            // TODO reset the state if error
            if (lex_peek(L) == '\\') {
                tok.type = T_CHARACTER;
                lex_advance(L);
                lex_advance(L); 
            }
        } break;

        // Strings
        case '"': {
            //while_lex(lex_peek(L) != '"');
            //lex_advance(L);
            lex_scan_string(L);
            lex_advance(L);
            tok.type = T_STRING;
        } break;

        // Compile time directive
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
                //Approach 1: while_lex(is_alnum(lex_peek(L)) || lex_peek(L) == '_');
                //Approach 2:
                lex_scan_identifier(L);
                //Approach 3: const char *cur = L->input.data + L->pos;
                //while (is_ident_char[]) cur++; 
                //tok.type = keyword_or_identifier(L->input.data + tok.start, L->pos - tok.start);
                tok.type = keyword_or_identifier2(&L->table, L->input.data + tok.start, L->pos - tok.start);
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
                    //lex_scan_digits(L);
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
