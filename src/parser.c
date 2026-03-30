#pragma once

#include <stdint.h>
#include "common.h"
#include "string.h"

/*

//-
//- We will define the EBNF grammar so that it is searchable
//- Each grammar definition starts with '//-' so these are easy to grep
//- e.g. `grep //- src/parser.h`
//-
//- Grammar specification may inaccurately reflect the language implementation, and is meant to be a rough guide.
//- 


*/

typedef struct {
    String message;
    uint32_t line;
    uint32_t column;
} ParseError;

#define X_PARSER_NODE_KINDS \
    X(INVALID) X(NUMBER) X(STRING) X(CHARACTER) X(IDENTIFIER) X(BINARY_OP) X(UNARY) X(VAR_DECL) X(CONST_DECL) \
    X(ASSIGNMENT) X(BLOCK) X(CALL) X(ARRAY_LITERAL) X(FUNC_DECL) X(IF_EXPR) X(RETURN) \
    X(NIL) X(FUNC_CALL) X(INDEX) X(RANGE) X(DOT_ACCESS) X(BOOL) X(AGGREGATE) X(TERNARY) \
    X(FOR_EXPR) X(WHILE_EXPR) X(CAST) X(PARAM) X(DOLLAR)

#define X(KIND) NODE_##KIND,
typedef enum { X_PARSER_NODE_KINDS } ParserNodeKind;
#undef X

#define X(KIND) S(#KIND),
static String node_kind_strings[] = {X_PARSER_NODE_KINDS};
#undef X

typedef struct ParserNode ParserNode;

typedef enum {
    BINOP_INVALID,
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,

    BINOP_EQUAL_TO,
    BINOP_NOT_EQUAL_TO,

    BINOP_LOGICAL_AND,
    BINOP_LOGICAL_OR,
} BinaryOpType;

typedef enum : uint8_t {
    QUALIFIER_UNKNOWN,
    QUALIFIER_CONST,
    QUALIFIER_VAR
} TypeQualifier;

typedef struct {
    String str;
    size_t hash;
} InternedStr;

// clang-format off
// TODO need to have extra data contingent too, possibly in a union
typedef struct { InternedStr name; InternedStr type; } Param;
typedef struct { InternedStr name; } TypeParam;
typedef struct { Param *params; int params_count; InternedStr ret; } FuncData;

typedef struct {
    TypeQualifier qualifier;
    InternedStr type_name;
} TypeInfo;

typedef struct { ParserNode **stmts; Param *params; uint16_t statements_count; uint16_t params_count; } Node_Block;
typedef struct { ParserNode *identifier; ParserNode *block; FuncData *func_data; } Node_FuncDecl;
typedef struct { ParserNode *callee; ParserNode **args; size_t args_count; } Node_FuncCall;
typedef struct { ParserNode *expr; } Node_Return;
typedef struct { ParserNode *identifier; ParserNode *rhs; TypeInfo *type_info; } Node_ConstVarDecl;
typedef struct { ParserNode *lhs; ParserNode *rhs; } Node_Assign;
typedef struct { ParserNode **elems; size_t count; ParserNode *length_expr; } Node_ArrayLiteral;
typedef struct { TokenType op; ParserNode *lhs; ParserNode *rhs; } Node_BinaryOp;
typedef struct { ParserNode *cond; ParserNode *then_branch; ParserNode *else_branch; } Node_IfExpr;
typedef struct { ParserNode *base; ParserNode *index; } Node_Index;
typedef struct { ParserNode *lhs; ParserNode *rhs; b8 inclusive; } Node_Range;
typedef struct { ParserNode *lhs; Token *member; } Node_DotAccess;
typedef struct { TokenType op; ParserNode *expr; } Node_Unary;
typedef struct { ParserNode **targets; ParserNode **values; size_t count; } Node_Aggregate;
typedef struct { ParserNode *cond; ParserNode *then_expr; ParserNode *else_expr; } Node_Ternary;
typedef struct { ParserNode *ident; ParserNode *iter_expr; ParserNode *block; } Node_ForExpr;
typedef struct { ParserNode *cond; ParserNode *block; } Node_WhileExpr;
typedef struct { ParserNode *to_type; } Node_Cast;

// clang-format on
// TODO: Move to using uint32_t indices instead of pointers
// e.g. kind | lhs | rhs | (possible extra data) = 16 bytes
struct ParserNode { // minimised from 64 to 32 bytes 
    ParserNodeKind kind;
    union {
        double real_value;
        intptr_t int_value;
        b8 bool_value;
        String string;
        String character_literal;
        InternedStr identifier;
        Node_Block block;
        Node_FuncDecl func_decl;
        Node_FuncCall func_call;
        Node_Return ret;
        Node_Assign assign;
        Node_BinaryOp binary_op;
        Node_IfExpr if_expr;
        Node_ArrayLiteral array_literal;
        Node_Index index;
        Node_Range range;
        Node_DotAccess dot_access;
        Node_Unary unary;
        Node_Aggregate aggregate;
        Node_ConstVarDecl const_var_decl;
        Node_Ternary ternary;
        Node_ForExpr for_expr;
        Node_WhileExpr while_expr;
        Node_Cast cast;
    };
};

bool hash_match(size_t hash1, size_t hash2) {
    // TODO
    return false;
}

bool params_match(Param *a, Param *b) {
    if (a == b) return true;
    if (!a || !b) return false;

    if (hash_match(a->name.hash, b->name.hash)) return true;
    if (string_compare(a->name.str, b->name.str) == 0) return true;

    return false;
}

bool nodes_match(ParserNode *a, ParserNode *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case NODE_BLOCK: {
            Node_Block *ba = &a->block;
            Node_Block *bb = &b->block;
            if (ba->statements_count != bb->statements_count) return false;
            if (ba->params_count != bb->params_count) return false;

            for (uint16_t i = 0; i < ba->statements_count; i++) {
                if (!nodes_match(ba->stmts[i], bb->stmts[i])) return false;
            }
            for (uint16_t i = 0; i < ba->params_count; i++) {
                if (!params_match(&ba->params[i], &bb->params[i])) return false;
            }

            return true;
        } break;

        default: {
            assert(0); // Not defined
        }
    }

    return false;
}

typedef struct {
    uint32_t line;
    uint32_t col;
    char *msg;
} ParserError;

typedef struct {
    String source;
    uint32_t pos;

    ParserNode* context_sensitive_within_array_index; // TODO replace with node index

    // Taken from the lexer
    Token *tokens;
    uint32_t token_count;

    ParserNode *nodes;
    uint32_t node_count;
    uint32_t node_cap;

    ParserError *errors;
    uint32_t error_count;
    uint32_t error_cap;

/*     uint32_t *statements;
    uint32_t statement_count;
    uint32_t statement_cap; */

    Arena *arena;
} Parser;

static String token_string(Parser *p, Token *tok) {
    return (String){.data = p->source.data + tok->start, .len = tok->length};
}

typedef enum {
    OP_ADD,        // +
    OP_SUB,        // -
    OP_MUL,        // *
    OP_DIV,        // /
    OP_MOD,        // %
    OP_EQ,         // ==
    OP_NEQ,        // !=
    OP_LT,         // <
    OP_GT,         // >
    OP_LTE,        // <=
    OP_GTE,        // >=
    OP_AND,        // &&
    OP_OR,         // ||
    OP_NOT,        // !
    OP_BITWISE_OR, // |
    OP_COUNT
} OpKind;

static const char *op_kind_strings[] = {
    "+", "-",  "*",  "/",  "%",  "==", "!=", "<",
    ">", "<=", ">=", "&&", "||", "!",  "|"};
_Static_assert(countof(op_kind_strings) == OP_COUNT,
               "op_kind_strings must match OpKind count");

static inline int get_op_precedence(OpKind op) {
    // Value is from highest to lowest (1=highest)
    switch (op) {
    case OP_MUL:
    case OP_DIV:
        return 1;
    case OP_ADD:
    case OP_SUB:
        return 2;
    case OP_BITWISE_OR:
        return 3;
    default:
        assert(0); // have the program fail for now
        return -1;
    }
}

typedef enum {
    VT_NONE,
    VT_OBJECT,
    VT_ATOM,
    VT_INT,
    VT_SEQ,
    VT_STRING,
    ST_FUNC,
} ScopeType;

#define PRINT_FUNC_NAME                                                        \
    do {                                                                       \
        print(S(__FUNCTION__));                                                \
        print(S("\n"));                                                        \
    } while (0)

typedef enum {
    PREC_NONE,
    PREC_COMMA,
    PREC_ASSIGN,
    PREC_TERNARY,
    PREC_RANGE,
    PREC_OR,
    PREC_AND,
    PREC_CMP,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY,
    PREC_COUNT,

    PREC_LOWEST = PREC_NONE,
    PREC_HIGHEST = PREC_PRIMARY,
} Precedence;

typedef ParserNode *(*PrefixFn)(Parser *);
typedef ParserNode *(*InfixFn)(Parser *, ParserNode *);

typedef struct {
    PrefixFn prefix;
    InfixFn infix;
    Precedence precedence;
} ParseRule;

///// Utility functions for the parser

static Token *peek(Parser *p) { return &p->tokens[p->pos]; }
static Token *advance(Parser *p) { return &p->tokens[p->pos++]; }

static int match(Parser *p, TokenType t) {
    if (peek(p)->type == t) {
        advance(p);
        return 1;
    }
    return 0;
}

static InternedStr str_intern(Parser *p, Token *tok) {
    // TODO refactor this to add to the string table p->string_table
    return (InternedStr) {
        .str = (String) { .data = p->source.data + tok->start, .len = tok->length },
        .hash = 0
    };
}

static void raise_error(Parser *p, const char *msg) {

    error("Parser error at source.tira:%zu: expected %s got ", peek(p)->line,
          msg); // TODO: allow for column in peek(p)->col
    print((String){.data = &p->source.data[peek(p)->start],
                   .len = peek(p)->length});
    print(S(".\n"));
    assert(true);
    // TODO add to error list

    p->error_count++;
}

static Token *previous(Parser *p) {
    PRINT_FUNC_NAME;
    assert(p->pos > 0);
    return &p->tokens[p->pos - 1];
}

static Token* current(Parser *p) {
    return &p->tokens[p->pos];    
}

static Token *expect(Parser *p, TokenType type, const char *msg) {
    (void)msg; // TODO
    Token *next_tok = peek(p);

    if (next_tok->type != type) {
        // raise_error(p, msg); TODO replace with expected token type
        raise_error(p, token_type_strings[type].data);
        return NULL;
    }

    return advance(p);
}

static void block_add_stmt(Parser *p, Node_Block *block, ParserNode *stmt) {
    block->stmts = 
        realloc_array(p->arena, block->stmts, ParserNode*, block->statements_count + 1);
    block->stmts[block->statements_count++] = stmt;
}

static ParserNode *new_node(Parser *p, ParserNodeKind kind) {
    print(S(__FUNCTION__));
    print(S(": "));
    print(node_kind_strings[kind]);
    print(S("\n"));

    if (p->node_count >= p->node_cap) {
        raise_error(p, "Exceeded node count in parser.");
        exit(1);
    }

    ParserNode *node = &p->nodes[p->node_count++];
    mem_zero(node);
    node->kind = kind;
    
    return node;
}

static void dump_parser_state(Parser *p) {
    printf("pos=%u, nodes=%u, errors=%u\n",
        p->pos, p->node_count, p->error_count);
    printf("next token: %s\n", token_type_strings[peek(p)->type].data);
}

// TODO is this needed? used originally for the idea of function calls without parentheses
static bool is_expr_start(TokenType type) {
    switch (type) {
        case T_IDENT:
        case T_NUM:
        case T_STRING:
        case T_TRUE:
        case T_FALSE:
        case T_NIL:

        // unary operators TODO review if unary plus added
        case T_MINUS:
        case T_NOT:

        // grouping / collections
        case T_LPAREN:
        case T_LSQBRACKET:  // array literal
        case T_LBRACE:      // block/map literal

        // keywords that produce values
        case T_IF:          // if as expression
        //case T_CAST:
            return true;

        default:
            return false;
    }
}

static void skip_newlines(Parser *p) {
    while (current(p)->type == T_NEWLINE)
        advance(p);
}

b32 assert_tokentype_is_binop(TokenType type) {
    switch (type) {
        case T_PLUS:
        case T_MINUS:
        case T_ASTERISK:
        case T_SLASH:
        case T_LOGICAL_AND:
        case T_LOGICAL_OR:
        case T_EQEQ:
        case T_NOT_EQ:
            return true;
        
        default:
            return false;
    }
}

///// Forward declarations for the recursive descent and Pratt parser functions

static ParserNode *parse_stmt(Parser *p);
static ParserNode *parse_expr(Parser *p);
static ParserNode *parse_block(Parser *p, TokenType block_end);
static ParserNode *parse_expr_prec(Parser *p, Precedence prec);
static ParserNode *parse_number(Parser *p);
static ParserNode *parse_string(Parser *p);
static ParserNode *parse_character(Parser *p);
static ParserNode *parse_dollar(Parser *p);
static ParserNode *parse_identifier(Parser *p);
static ParserNode *parse_grouping(Parser *p);
static ParserNode *parse_call(Parser *p, ParserNode *lhs);
static ParserNode *parse_binary(Parser *p, ParserNode *lhs);
static ParserNode *parse_unary(Parser *p);
static ParserNode *parse_nil(Parser *p);
static ParserNode *parse_bool(Parser *p);
static ParserNode *parse_array_lit(Parser *p);
static ParserNode *parse_index(Parser *p, ParserNode *lhs);
static ParserNode *parse_range(Parser *p, ParserNode *lhs);
static ParserNode *parse_dot(Parser *p, ParserNode *lhs);
static ParserNode *parse_assign(Parser *p, ParserNode *lhs);
static ParserNode *parse_if_expr(Parser *p);
static ParserNode *parse_for_expr(Parser *p);
static ParserNode *parse_while_expr(Parser *p);
static ParserNode *parse_ternary(Parser *p, ParserNode *lhs);
static ParserNode *parse_comma(Parser *p, ParserNode *lhs);
static void parse_primary(Parser *p);
static Param parse_param(Parser *p);
static int collect_lhs_tuple(Parser *p, ParserNode *first, ParserNode **targets);
static int collect_rhs_tuple(Parser *p, ParserNode **values);
static ParserNode *make_multi_assign(Parser *p,
                                     ParserNode **targets, int target_count,
                                     ParserNode **values, int value_count);
static void parse_struct_init(Parser *p);
static ParserNode *parse_return(Parser *p);

///// Rule table for the Pratt Parser

static ParseRule rules[T_COUNT] = {
// TODO cleanup order of the table for precedence instead of categories
// TODO possibly refactor this table into a switch statement 
    // ----- Terminals (literals and primaries)
    [T_NUM] = {parse_number, NULL, PREC_NONE},
    [T_STRING] = {parse_string, NULL, PREC_NONE},
    [T_CHARACTER] = {parse_character, NULL, PREC_NONE},
    //[T_IDENT] = {parse_identifier, NULL, PREC_NONE},
    [T_IDENT] = {parse_identifier, NULL, PREC_NONE},
    //  [T_TRUE]       = { parse_bool,      NULL,         PREC_NONE },
    //  [T_FALSE]      = { parse_bool,      NULL,         PREC_NONE },
    [T_NIL] = {parse_nil, NULL, PREC_NONE},
    [T_TRUE] = {parse_bool, NULL, PREC_NONE},
    [T_FALSE] = {parse_bool, NULL, PREC_NONE},
    [T_DOLLAR] = {parse_dollar, NULL, PREC_NONE},

    // ----- Grouping and collections
    [T_LPAREN] = {parse_grouping, parse_call, PREC_CALL},
    [T_LSQBRACKET] = {parse_array_lit, parse_index, PREC_CALL},
    //[T_LBRACE] = {parse_block, NULL, PREC_NONE},
    [T_COMMA] = {NULL, parse_comma, PREC_COMMA},

    // ----- Unary
    [T_MINUS] = {parse_unary, parse_binary, PREC_TERM},
    [T_NOT] = {parse_unary, NULL, PREC_NONE},

    // ----- Infix arithmetic
    [T_PLUS] = {NULL, parse_binary, PREC_TERM},
    [T_ASTERISK] = {NULL, parse_binary, PREC_FACTOR},
    [T_SLASH] = {NULL, parse_binary, PREC_FACTOR},
    //  [T_PERCENT]    = { NULL,            parse_binary, PREC_FACTOR },

    // ----- Comparison
    [T_LT] = {NULL, parse_binary, PREC_CMP},
    [T_GT] = {NULL, parse_binary, PREC_CMP},
    [T_LTEQ] = {NULL, parse_binary, PREC_CMP},
    [T_GTEQ] = {NULL, parse_binary, PREC_CMP},
    [T_EQEQ] = {NULL, parse_binary, PREC_CMP},
    [T_NOT_EQ] = {NULL, parse_binary, PREC_CMP},

    // ----- Logic
    [T_LOGICAL_AND] = {NULL, parse_binary, PREC_AND},
    [T_LOGICAL_OR] = {NULL, parse_binary, PREC_OR},

    //  [T_AND]        = { NULL,            parse_binary, PREC_AND },
    //  [T_OR]         = { NULL,            parse_binary, PREC_OR  },

    // ----- Ternary operator ?:
    [T_QUESTION] = {NULL, parse_ternary, PREC_TERNARY},

    // ----- Ranges
    [T_DOTDOT] = {NULL, parse_range, PREC_RANGE},
    [T_DOTDOTEQ] = {NULL, parse_range, PREC_RANGE},

    // ----- Method invocation/method access
    [T_DOT] = {NULL, parse_dot, PREC_CALL},

    // ----- Assignment
    [T_EQUALS] = {NULL, parse_assign, PREC_ASSIGN},

    // ----- Keywords
    [T_IF] = {parse_if_expr, NULL, PREC_NONE},
    //[T_FUNC] = {parse_func_decl, NULL, PREC_NONE},
    [T_FOR] = {parse_for_expr, NULL, PREC_NONE},
    [T_WHILE] = {parse_while_expr, NULL, PREC_NONE},
    // T_RBRACE and others to catch errors instead of segfault
};

//- ----- START OF GRAMMAR -----
//-
//- program                = { top_level_declaration }
//- top_level_declaration  = const_decl
//-                        | var_decl
//-                        | func_decl 
//-                        | main_block
//- 
void parse_program(Parser *p) {
    PRINT_FUNC_NAME;
    print(S("{\n\n"));
    ParserNode *program = new_node(p, NODE_BLOCK);

    while (peek(p)->type != T_END) {
        if (match(p, T_NEWLINE) || match(p, T_SEMICOLON))
            continue;
        
        ParserNode *stmt = parse_stmt(p);
        block_add_stmt(p, &program->block, stmt);

        //getchar();

        print(S("\n"));

        if (peek(p)->type == T_END)
            break;
        if (!match(p, T_NEWLINE) && !match(p, T_SEMICOLON))        
            raise_error(p, "No newline at end of statement\n");
    }

    print(S("}\n"));

    if (p->error_count) {
        console_error("Errors found in parsing.\n", 25);
    }
}

//- const_decl             = 'const', identifier, [ '=', expr ]
//- var_decl               = 'var', identifier, [ '=', expr ]
//-
static ParserNode *parse_const_var_decl(Parser *p, TokenType type) {
    PRINT_FUNC_NAME;

    assert(type == T_VAR || type == T_CONST);

    ParserNode *node = new_node(p, (type == T_VAR) ? NODE_VAR_DECL : NODE_CONST_DECL);
    advance(p);
    ParserNode *identifier = parse_identifier(p);
    
    ParserNode *rhs = NULL;
    if (match(p, T_EQUALS)) rhs = parse_expr(p);
    
    node->const_var_decl.identifier = identifier;
    node->const_var_decl.rhs = rhs;

    return node;
}

//- function_decl          = 'func', identifier, [ params ], block
//-
static ParserNode *parse_func_decl(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_FUNC_DECL);
    // Advance if identifier (not main)
    match(p, T_IDENT);
    ParserNode *identifier = parse_identifier(p);

    TypeParam *type_params = NULL;
    size_t type_params_count = 0;

    Param *params = NULL;
    size_t params_count = 0;

    if (peek(p)->type == T_LPAREN) {
        advance(p);

        do {
            params = realloc_array(p->arena, params, Param, params_count + 1);
            params[params_count++] = parse_param(p);
        } while (match(p, T_COMMA));

        expect(p, T_RPAREN, "Expected ')' after arguments.");
    }


    FuncData *func_data = new(p->arena, FuncData, 1);
    func_data->params = params;
    func_data->params_count = params_count;
    func_data->ret = (InternedStr){0};

    node->func_decl.identifier = identifier;
    node->func_decl.func_data = func_data;
    expect(p, T_LBRACE, "Expected '{' at start of function block.");
    node->func_decl.block = parse_block(p, T_RBRACE);

    return node;
}

//- main_decl              = 'main', [ params ], block
//- 
static ParserNode *parse_main_decl(Parser *p) {
    PRINT_FUNC_NAME;
    return parse_func_decl(p);
}

//- params                 = identifier, { ',', identifier }
//- 
static Param parse_param(Parser *p) {
    PRINT_FUNC_NAME;

    Token *param_tok = advance(p);
    Param param;
    param.name = str_intern(p, param_tok);

    // Parse the optional typename
    if (match(p, T_COLON)) {
        Token *type_tok = expect(p, T_IDENT, "Expected type after : in parameter");
        param.type = str_intern(p, type_tok);
    }

    return param;
}

//- (* Expressions ordered by precedence, lowest first *)
//- expr                   = assignment
//-
static ParserNode *parse_expr(Parser *p) {
    return parse_expr_prec(p, PREC_LOWEST);
}

static ParserNode *parse_expr_prec(Parser *p, Precedence prec) {
    PRINT_FUNC_NAME;

    Token *token = advance(p);
    print(token_type_strings[token->type]);
    print(S("\n"));

    ParseRule *rule = &rules[token->type];
    if (!rule->prefix) {
        raise_error(p, "Expected expression.\n");
        return NULL;
    }

    ParserNode *lhs = rule->prefix(p);

    while (prec < rules[peek(p)->type].precedence) {
        //assert(0);
        token = advance(p);
        rule = &rules[token->type];
        lhs = rule->infix(p, lhs);
    }

    return lhs;
}

//- assignment             = tuple_assign
//-
static ParserNode *parse_assign(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;
    ParserNode *node = new_node(p, NODE_ASSIGNMENT);
    node->assign.lhs = lhs;
    node->assign.rhs = parse_expr_prec(p, PREC_LOWEST);

    return node;
}

//- tuple_assign           = lvalue_list, "=", expr_list
//- lvalue_list            = lvalue, {",", lvalue}
//- expr_list              = expr, {",", expr}
//- lvalue                 = identifier | index_expr | dot_expr
//-
static ParserNode *parse_comma(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode **targets, **values;
    int target_count = collect_lhs_tuple(p, lhs, targets);
    expect(p, T_EQUALS, "Multiple values with comma should have assignment.");
    int value_count = collect_rhs_tuple(p, values);
    return make_multi_assign(p, targets, target_count, values, value_count);
}

/*
Note the asymmetry — collect_lhs_tuple takes the already-parsed first node
because your statement parser will have consumed it before knowing a comma
was coming, whereas collect_rhs_tuple starts fresh after the =.
*/

static int collect_lhs_tuple(Parser *p, ParserNode *first, ParserNode **targets) {
    PRINT_FUNC_NAME;

    ParserNode **nodes = new(p->arena, ParserNode*, 1);
    int len = 0;
    nodes[len++] = first;

    do {
        nodes = realloc_array(p->arena, nodes, ParserNode*, len++);
        if (nodes == NULL) {
            // run out of memory
            return 0;
        }
        ParserNode *target = parse_expr_prec(p, PREC_ASSIGN);
        // TODO Note we need to have valid stuff on the lhs e.g. identifiers or indexes as opposed to general expressions
        if (target->kind != NODE_IDENTIFIER) {
            raise_error(p, "Invalid assignment target in tuple, must be identifier.");
        }
        nodes[len] = target;
    } while(match(p, T_COMMA));
    
    targets = nodes;
    return len;
}

static int collect_rhs_tuple(Parser *p, ParserNode **values) {
    PRINT_FUNC_NAME;

    ParserNode **nodes = new(p->arena, ParserNode*, 1);
    int len = 0;
    do {
        nodes = realloc_array(p->arena, nodes, ParserNode*, len); // This won't resize on case first iteration where 1 is used?
        if (nodes == NULL) {
            // run out of memory
            return 0;
        }
        nodes[len++] = parse_expr_prec(p, PREC_ASSIGN);
    } while(match(p, T_COMMA));

    values = nodes;
    return len;
}

static ParserNode *make_multi_assign(Parser *p,
                                     ParserNode **targets, int target_count,
                                     ParserNode **values, int value_count) {
    // check first the target count and value count is the same
    if (target_count != value_count) {
        raise_error(p, "multi-assign mismatch: %d targets but %d values");
        return NULL;
    }

    ParserNode *node = new_node(p, NODE_AGGREGATE);
    node->aggregate.targets = targets;
    node->aggregate.values  = values;
    node->aggregate.count   = target_count;
    return node;
}

//- ternary                = range , [ "?" , expr , ":" , ternary ]
//-
static ParserNode *parse_ternary(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    // TODO check that this is properly right-associative?

    ParserNode *node = new_node(p, NODE_TERNARY);

    node->ternary.cond = lhs;  
    node->ternary.then_expr = parse_expr(p);
    expect(p, T_COLON, "Expected colon after then expression");
    node->ternary.else_expr = parse_expr_prec(p, PREC_TERNARY);

    return node;
}

// TODO @Cleanup check for cases where a..b&&c..d (possibly this should be a parse error?)
// TODO @Cleanup consider the case where we force parens instead of having OR lower than AND?
// Essentially replacing `or` below with `logical_expr`?
// e.g. logical_expr = and_expr | or_expr | comparison
//- range                  = or, [ (".." | "..="), or ]
//-
static ParserNode *parse_range(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_RANGE);
    node->range.lhs = lhs;
    node->range.inclusive = (previous(p)->type == T_DOTDOTEQ);
    node->range.rhs = parse_expr_prec(p, PREC_RANGE);
    return node;
}

// TODO logical ops
//- or                     = and , { "||" , and } ;
//- and                    = comparison , { "&&" , comparison } ;
//- comparison             = term, { ("==" | "!=" | "<" | ">" | "<=" | ">="), term }
//- term                   = factor, { ("+", "-"), factor }
//- factor                 = unary, {("*" | "/"), factor }
//-
static ParserNode *parse_binary(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    TokenType binop_token_type = previous(p)->type;
    assert_tokentype_is_binop(binop_token_type);
    
    ParserNode *node = new_node(p, NODE_BINARY_OP);
    ParserNode *rhs = parse_expr_prec(p, PREC_FACTOR);
    node->binary_op.op = binop_token_type;
    node->binary_op.lhs = lhs;
    node->binary_op.rhs = rhs; // TODO need to do plus

    return node;
}
//- factor                 = unary, {("*" | "/"), factor }

// TODO "+"?
//- unary                  = ("-" | "!"), unary
//-                        | postfix
//-
static ParserNode *parse_unary(Parser *p) {
    ParserNode *node = new_node(p, NODE_UNARY);
    node->unary.op = previous(p)->type;
    node->unary.expr = parse_expr_prec(p, PREC_UNARY);
    return node;
}

//- postfix                = primary, {postfix_op}
//- postfix_op             = call_op | index_op | dot_op
//-
static void parse_postfix(Parser *p) {
    PRINT_FUNC_NAME;

    parse_primary(p);

    for (;;) {
        // Function call
        if (peek(p)->type == T_LPAREN) {
            advance(p);

            if (peek(p)->type != T_RPAREN) {
                do {
                    parse_expr(p);
                } while (match(p, T_COMMA));
            }

            expect(p, T_RPAREN, "");
            continue;
        }

        // Member access
        if (peek(p)->type == T_DOT) {
            assert(0); // @Cleanup not added properly to AST

            advance(p); // consume .
            expect(p, T_IDENT, "");
            continue;
        }

        if (peek(p)->type == T_LBRACE) {
            assert(0); // @Cleanup not added properly to AST

            parse_struct_init(p);
            continue;
        }

        break;
    }
}

// TODO how does this fit into precedence or use of {}?
static void parse_struct_init(Parser *p) {
    PRINT_FUNC_NAME;
    assert(0); // @Cleanup to be implemented
    expect(p, T_LBRACE, "");

    if (!match(p, T_RBRACE)) {
        do {
            if (peek(p)->type == T_NEWLINE)
                expect(p, T_NEWLINE, "");
            expect(p, T_DOT, "");
            expect(p, T_IDENT, "");
            expect(p, T_EQUALS, "");
            parse_expr(p);
            expect(p, T_COMMA, ""); // Optional for last so different than
                                    // function calls/definitions
            if (peek(p)->type == T_NEWLINE)
                expect(p, T_NEWLINE, "");
        } while (!match(p, T_RBRACE));
    }
}

// TODO review ambiguity here with regular grouping?
//- call_op                = "(" , [ arg_list ] , ")" ;
//- arg_list               = expr , { "," , expr } ;
//- 
static ParserNode *parse_call(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_FUNC_CALL);
    ParserNode **args = NULL;
    size_t count = 0;

    if (peek(p)->type != T_RPAREN) {
        do {
            // TODO: What about pushing elements into a dynamic array in
            // general?
            args = realloc_array(p->arena, args, ParserNode *, count + 1);
            args[count++] = parse_expr_prec(p, PREC_COMMA);
        } while (match(p, T_COMMA));
    }

    expect(p, T_RPAREN, "Expected ')' after arguments.");

    node->func_call.callee = lhs;
    node->func_call.args = args;
    node->func_call.args_count = count;
    return node;
}

// TODO is postfix correct
// TODO need to correct use of "$" here
//- index_op               = "[", expr, "]"
//- index_expr             = postfix            (* alias used in lvalue *)
//-                        | "$"                (* ... *)
//-
static ParserNode *parse_index(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    // Setting this context-sensitive feature for referring to length of an array within [] for slicing
    //p->context_sensitive_within_array_index = true; // TODO need to handle case such as a[b[$]] - use a stack!
    p->context_sensitive_within_array_index = lhs;
    ParserNode *node = new_node(p, NODE_INDEX);
    node->index.base = lhs;
    node->index.index = parse_expr_prec(p, PREC_LOWEST);
    expect(p, T_RSQBRACKET, "Expected ']' after index.");
    p->context_sensitive_within_array_index = NULL;
    return node;
}

static ParserNode *parse_dollar(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = NULL;
    if (p->context_sensitive_within_array_index != NULL) {
        node = new_node(p, NODE_IDENTIFIER);
        Token *tok = previous(p);
        node->identifier = str_intern(p, tok);
    } else {
        raise_error(p, "Invalid use of '$', this must be used in array indexing.");
    }
    return node;
}

// TODO should this be expression?
//- dot_op                 = ".", identifer 
//- index_expr             = postfix            (* alias used in lvalue *)
//-
static ParserNode *parse_dot(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_DOT_ACCESS);
    node->dot_access.lhs = lhs;

    Token *member = expect(p, T_IDENT, "Expected member name after '.'.");
    node->dot_access.member = member;
    return node;
}

//- primary                = number_lit | character_lit | string_lit | nil | bool_lit
//-                        | identifier
//-                        | "(", expr, ")"
//-
static void parse_primary(Parser *p) {
    PRINT_FUNC_NAME;

    Token *t = peek(p);
    print(S("parse_primary Token type: "));
    print(token_type_strings[t->type]);
    print_char('\n');

    switch (t->type) {
        case T_NUM:
        case T_IDENT:
        case T_STRING:
        case T_NIL:
        case T_TRUE:
        case T_FALSE:
        case T_CHARACTER:
            advance(p);
            break;

        case T_DOLLAR: {
            advance(p); // TODO: Should this be moved down into if body? Need to advance the parser.
            if (p->context_sensitive_within_array_index == NULL) {
                raise_error(p, "Invalid use of '$', this must be used in array indexing.");
            }
        } break;

        case T_LPAREN: {
            advance(p);
            parse_expr(p);
            expect(p, T_RPAREN, "");
        } break;

        default: {
            raise_error(p, "Primary incorrect. Oops. Expected expression.");
        } break;
    }
}

static ParserNode *parse_grouping(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *expr = parse_expr(p);
    expect(p, T_RPAREN, "Expected ')' after grouped expression.");
    return expr;
}

// TODO review floats with .5 and 0.5 and 5.
//- (* Literals *)
//- number_lit             = integer_lit | float_lit
//- integer_lit            = digits | ("0x", hex_digit, {hex_digit})
//- float_lit              = {digits, ".", digits} | {digits, "."} | {".", digits}

static ParserNode *parse_number(Parser *p) {
    ParserNode *node = new_node(p, NODE_NUMBER);

    Token *tok = previous(p);
    char *start = p->source.data + tok->start;
    bool ok = false;

    if (memchr(start, '.', tok->length)) {
        ok = string_to_double((String){.data = start, .len = tok->length}, &node->real_value);
    } else {
        ok = string_to_int64((String){.data = start, .len = tok->length}, &node->int_value);
    }

    if (!ok) {
        raise_error(p, "Number provided is invalid.");
    }

    return node;
}

//- character_lit          = "#\", char
//- char                   = printable_non_whitespace_char | "u0000" | "space" | "tab" | ... | other_character_classes
static ParserNode *parse_character(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_CHARACTER);
    node->character_literal = token_string(p, previous(p)); // We still need a string to hold a character literal
    node->character_literal.data += 2; // Increment past the "#\"
    node->character_literal.len -= 2;
    return node;
}

//- string_lit             = '"', {str_char}, '"'
static ParserNode *parse_string(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_STRING);
    node->string = token_string(p, previous(p));
    return node;
}

//- nil                    = "nil"
static ParserNode *parse_nil(Parser *p) {
    PRINT_FUNC_NAME;
    return new_node(p, NODE_NIL);
}

//- bool_lit               = "true" | "false"
//- 
static ParserNode *parse_bool(Parser *p) {
    PRINT_FUNC_NAME;
    ParserNode *node = new_node(p, NODE_BOOL);
    node->bool_value = previous(p)->type == T_TRUE;
    return node;
}

//- array_lit              = "[", expr, ";", expr, "]"   (* repeat form:  [0; width*height] *)
//-                        | "[" , [expr_list] , "]"     (* element form: [1, 2, 3] *)
//-
static ParserNode *parse_array_lit(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_ARRAY_LITERAL);
    ParserNode **elems = NULL;
    size_t count = 0;

    if (peek(p)->type != T_RSQBRACKET) {
        do {
            if (peek(p)->type == T_RSQBRACKET)
                break; // case of trailing comma (however what about) [,]
            elems = realloc_array(p->arena, elems, ParserNode *, count + 1);
            elems[count++] = parse_expr_prec(p, PREC_HIGHEST);
        } while (match(p, T_COMMA));
    }

    // Used to create array literals with repeated elements e.g. [nil; 1000] or [10; 1000]
    // where nil and 10 are initial values with an array of 1000 elements
    ParserNode *length = NULL;
    if (match(p, T_SEMICOLON)) {
        // TODO need to handle case with trailing comma e.g. [0,; 1000] should be invalid
        if (count != 1) { raise_error(p, "Initialisation of repeated elements must be in the form [value; length]"); }
        length = parse_expr(p);
    }

    expect(p, T_RSQBRACKET, "Expected ']' after array literal.");

    node->array_literal.elems = elems;
    node->array_literal.count = count;
    node->array_literal.length_expr = length;
    return node;
}

//- (* Control flow expressions *)
// TODO review case of "if (expr) then_stmt"
//- if_expr                = "if", expr, block, ["else", (if_expr | block)]
static ParserNode *parse_if_expr(Parser *p) {
    PRINT_FUNC_NAME;
    ParserNode *cond = parse_expr(p);

    skip_newlines(p);
    expect(p, T_LBRACE, "Expected '{' after if condition.");
    ParserNode *then_branch = parse_block(p, T_RBRACE);
    ParserNode *else_branch = NULL;

    // Parse else branch, look ahead skipping newlines/semicolons
    // need to save the position and restore if there is no else branch.
    // This is needed to determine end of statement in parse_block()
    uint32_t saved_pos = p->pos;
    skip_newlines(p);
    if (match(p, T_ELSE)) {
        skip_newlines(p);
        if (current(p)->type == T_IF) { // 
            advance(p);
            else_branch = parse_if_expr(p);
        } else {
            expect(p, T_LBRACE, "Expected '{' after else.");
            else_branch = parse_block(p, T_RBRACE);
        }
    } else {
        // Restore the position
        p->pos = saved_pos;
    }

    ParserNode *node = new_node(p, NODE_IF_EXPR);
    node->if_expr.cond = cond;
    node->if_expr.then_branch = then_branch;
    node->if_expr.else_branch = else_branch;
    return node;
}

// TODO this is ambiguous or rather the for_in should be moved up, productions are different
//- for_expr               = ("for", expr, block) | ("for", identifier, "in", expr, block)
//-
static ParserNode *parse_for_expr(Parser *p) {
    PRINT_FUNC_NAME;
    ParserNode *node = new_node(p, NODE_FOR_EXPR);

    //expect(p, T_FOR, "FOR EXPECTED");
    ParserNode *ident = parse_identifier(p); 
    advance(p); // Due to pratt parser need to advance/consume token

    ParserNode *iter_expr = NULL;
    if (match(p, T_IN)) {
        expect(p, T_IN, "'in' must follow the iterator variable before enumeratable.");
        iter_expr = parse_expr(p);
    }

    expect(p, T_LBRACE, "Expected '{' after for condition");    
    ParserNode *block = parse_block(p, T_RBRACE);

    node->for_expr.ident = ident;
    node->for_expr.iter_expr = iter_expr;
    node->for_expr.block = block;
    return node;
}

//- while_expr             = "while", expr, block
//- 
static ParserNode *parse_while_expr(Parser *p) {
    PRINT_FUNC_NAME;
    ParserNode *node = new_node(p, NODE_WHILE_EXPR);
    ParserNode* cond = parse_expr(p);

    expect(p, T_LBRACE, "Expected '{' after while condition");
    ParserNode *block = parse_block(p, T_RBRACE);

    node->while_expr.cond = cond;
    node->while_expr.block = block;
    return node;
}


//- (* Terminals *)
//- identifier             = letter , { letter | digit | "_" } ;
static ParserNode *parse_identifier(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_IDENTIFIER);
    Token *tok = previous(p);
    node->identifier = str_intern(p, tok);

    return node;
}
//- letter                 = "a" ... "z" | "A" ... "Z" | "_" ;
//- digit                  = "0" ... "9" ;
//- hex_digit              = digit | "a" ... "f" | "A" ... "F" ;
//- str_char               = any_char_except_double_quote_or_newline ;
//- newline                = "\n" | "\r\n" ;
//- 

//- (* Statements *)
//- statement              = shebang
//-                        | var_decl
//-                        | const_decl
//-                        | func_decl
//-                        | main_block
//-                        | return_stmt
//-                        | expr_stmt
static ParserNode *parse_stmt(Parser *p) {
    PRINT_FUNC_NAME;

    //if (match(p, T_FOR))    return parse_for_stmt(p);
    if (match(p, T_RETURN)) return parse_return(p);
    if (match(p, T_FUNC))   return parse_func_decl(p);
    if (match(p, T_MAIN))   return parse_main_decl(p);
    if (match(p, T_VAR))    return parse_const_var_decl(p, T_VAR);
    if (match(p, T_CONST))  return parse_const_var_decl(p, T_CONST);
    else                    return parse_expr(p);
}

//- return_stmt            = "return" , [expr]
//-
static ParserNode *parse_return(Parser *p) {
    PRINT_FUNC_NAME;
    
    ParserNode *ret = new_node(p, NODE_RETURN);
    ParserNode *expr = parse_expr(p);
    ret->ret.expr = expr;

    return ret;
}

//- (* Blocks *)
//- block                  = "{" , { statement | newline } , "}" ;
//- 
static ParserNode *parse_block(Parser *p, TokenType block_end) {
    PRINT_FUNC_NAME;
    ParserNode *block = new_node(p, NODE_BLOCK);

    // while (peek(p)->type != T_END) {
    while (peek(p)->type != block_end) {
        if (match(p, T_NEWLINE) || match(p, T_SEMICOLON))
            continue;
        ParserNode *stmt = parse_stmt(p);
        block_add_stmt(p, &block->block, stmt);

        if (match(p, T_NEWLINE) || match(p, T_SEMICOLON))
            continue;

        if (peek(p)->type == block_end)
            break;

        // Parse error here, statement expects newline or END
        raise_error(p, "No newline at end of statement\n");
    }

    expect(p, block_end, "Expected closing token");

    return block;
}

//- ----- END OF GRAMMAR -----
//-
// TODO maybe update EBNF for comments, shebang directive etc.
// TODO how may I handle significant newlines or avoid in this grammar specification?




