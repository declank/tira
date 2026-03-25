#pragma once

/*
//- We will define the grammar so that it is searchable
//- Each grammar definition starts with '//-' so these are easy to grep
//-
*/

#include "common.h"
#include "lexer.h"
#include "memory.h"
#include "print.h"
#include "string.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>


typedef struct {
    String message;
    uint32_t line;
    uint32_t column;
} ParseError;

#define X_PARSER_NODE_KINDS                                                           \
    X(NUMBER) X(STRING) X(IDENTIFIER) X(BINARY_OP) X(UNARY) X(VAR_DECL) X(CONST_DECL) \
    X(ASSIGNMENT) X(BLOCK) X(CALL) X(ARRAY_LITERAL) X(FUNC_DECL) X(IF_EXPR) X(RETURN) \
    X(NIL) X(FUNC_CALL) X(INDEX) X(RANGE) X(DOT_ACCESS) X(BOOL) X(AGGREGATE) X(TERNARY) \
    X(FOR_EXPR) X(CAST) X(PARAM)

// X(FUNC_DECL) X(CFFI_FUNC_DECL)

#define X(KIND) NODE_##KIND,
typedef enum { X_PARSER_NODE_KINDS } ParserNodeKind;
#undef X

#define X(KIND) S(#KIND),
static String node_kind_strings[] = {X_PARSER_NODE_KINDS};
#undef X

typedef struct ParserNode ParserNode;

typedef int8_t b8;

typedef enum {
    BINOP_INVALID,
    BINOP_ADD,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
} BinaryOpType;

typedef enum {
    QUALIFIER_UNKNOWN,
    QUALIFIER_CONST,
    QUALIFIER_VAR
} TypeQualifier;

typedef struct {
    String str;
    size_t hash;
} InternedStr;

typedef struct { InternedStr name; InternedStr type; } Param;
typedef struct { InternedStr name; } TypeParam;

typedef struct { ParserNode **stmts; size_t count; Param *params; size_t params_count; } Node_Block;
typedef struct { InternedStr identifier; Param *params; int param_count; InternedStr ret; ParserNode *block; } Node_FuncDecl;
typedef struct { ParserNode *callee; ParserNode **args; size_t args_count; } Node_FuncCall;
typedef struct { ParserNode *expr; } Node_Return;
typedef struct { InternedStr identifier; ParserNode *type; ParserNode *rhs; TypeQualifier qualifier; } Node_ConstVarDecl;
typedef struct { ParserNode *lhs; ParserNode *rhs; } Node_Assign;
typedef struct { ParserNode **elems; size_t count; ParserNode *length_expr; } Node_ArrayLiteral;
typedef struct { BinaryOpType type; ParserNode *lhs; ParserNode *rhs; } Node_BinaryOp;
typedef struct { ParserNode *cond; ParserNode *then_branch; ParserNode *else_branch; } Node_IfExpr;
typedef struct { ParserNode *base; ParserNode *index; } Node_Index;
typedef struct { ParserNode *lhs; ParserNode *rhs; b8 inclusive; } Node_Range;
typedef struct { ParserNode *lhs; Token member; } Node_DotAccess; // TODO
typedef struct { TokenType op; ParserNode *expr; } Node_Unary;
typedef struct { ParserNode **targets; ParserNode **values; int count; } Node_Aggregate;
typedef struct { ParserNode *cond; ParserNode *then_expr; ParserNode *else_expr; } Node_Ternary;
typedef struct { ParserNode *ident; ParserNode *iter_expr; } Node_ForExpr;
typedef struct { ParserNode *to_type; } Node_Cast;

struct ParserNode {
    ParserNodeKind kind;
    union {
        double real_value;
        intptr_t int_value;
        b8 bool_value;
        InternedStr identifier;
        String string;
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
        Node_Cast cast;
    };
};

typedef struct {
    uint32_t line;
    uint32_t col;
    char *msg;
} ParserError;

typedef struct {
    String source;
    uint32_t pos;

    // Taken from the lexer
    Token *tokens;
    uint32_t token_count;

    ParserNode *nodes;
    uint32_t node_count;
    uint32_t node_cap;

    ParserError *errors;
    uint32_t error_count;
    uint32_t error_cap;

    uint32_t *statements;
    uint32_t statement_count;
    uint32_t statement_cap;

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
    PREC_OR,
    PREC_AND,
    PREC_CMP,
    PREC_RANGE,
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

static void block_add_stmt(ParserNode *block, ParserNode *stmt) {
    // TODO
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

static ParserNode *parse_stmt(Parser *p);
static ParserNode *parse_expr(Parser *p);
static ParserNode *parse_block(Parser *p, TokenType block_end);
static Param parse_param(Parser *p);
static ParserNode *parse_expr_prec(Parser *p, Precedence prec);

static ParserNode *parse_number(Parser *p);
static ParserNode *parse_string(Parser *p);
static ParserNode *parse_ident(Parser *p);
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
static ParserNode *parse_cast(Parser *p);
static ParserNode *parse_ternary(Parser *p, ParserNode *lhs);
static ParserNode *parse_comma(Parser *p, ParserNode *lhs);

static ParseRule rules[T_COUNT] = {
    // ----- Terminals (literals and primaries)
    [T_NUM] = {parse_number, NULL, PREC_NONE},
    [T_STRING] = {parse_string, NULL, PREC_NONE},
    //[T_IDENT] = {parse_ident, NULL, PREC_NONE},
    [T_IDENT] = {parse_ident, NULL, PREC_NONE},
    //  [T_TRUE]       = { parse_bool,      NULL,         PREC_NONE },
    //  [T_FALSE]      = { parse_bool,      NULL,         PREC_NONE },
    [T_NIL] = {parse_nil, NULL, PREC_NONE},
    [T_TRUE] = {parse_bool, NULL, PREC_NONE},
    [T_FALSE] = {parse_bool, NULL, PREC_NONE},

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
    [T_CAST] = {parse_cast, NULL, PREC_NONE},

    // T_RBRACE and others to catch errors instead of segfault
};

//- program                = { top_level_declaration }
//- top_level_declaration  = const_decl
//-                        | var_decl
//-                        | func_decl 
//-                        | main_block
//- 
static void parse_program(Parser *p) {
    PRINT_FUNC_NAME;
    print(S("{\n\n"));

    ParserNode *program = new_node(p, NODE_BLOCK);

    while (peek(p)->type != T_END) {
        if (match(p, T_NEWLINE) || match(p, T_SEMICOLON))
            continue;
        
        ParserNode *stmt = parse_stmt(p);
        block_add_stmt(program, stmt);

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

    Token *ident = expect(p, T_IDENT, "Variable or constant declaration must have a valid identifier on left-hand side.");
    expect(p, T_EQUALS, "Variables must be initialised upon declaration. TODO remove"); // TODO

    ParserNode *rhs = parse_expr(p);
    ParserNode *node = NULL;

    if (type == T_VAR) {
        node = new_node(p, NODE_VAR_DECL);
        node->const_var_decl.identifier = str_intern(p, ident);
        node->const_var_decl.rhs = rhs;
    } else if (type == T_CONST) {
        node = new_node(p, NODE_CONST_DECL);
        node->const_var_decl.identifier = str_intern(p, ident);
        node->const_var_decl.rhs = rhs;
    } else {
        assert(0);
    }

    return node;
}

//- function_decl          = 'func', identifier, [ params ], block
//-
static ParserNode *parse_func_decl(Parser *p) {
    PRINT_FUNC_NAME;

    Token* ident = expect(p, T_IDENT, "Function name must follow 'func'");

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

    ParserNode *node = new_node(p, NODE_FUNC_DECL);
    node->func_decl.identifier = str_intern(p, ident);
    node->func_decl.params = params;
    node->func_decl.param_count = params_count;

    expect(p, T_LBRACE, "Expected '{' at start of function block.");
    node->func_decl.block = parse_block(p, T_RBRACE);

    return node;
}

//- main_decl              = 'main', [ params ], block
//- 
static ParserNode *parse_main_decl(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_FUNC_DECL);

    // Hacky way of saving "main" keyword also as the function name, later we will implement "main" as a macro instead of keyword
    Token* main = current(p);
    node->func_decl.identifier = str_intern(p, main);

    // TODO: Case for main(args: []string)
    node->func_decl.params = NULL;
    node->func_decl.param_count = 0;

    expect(p, T_LBRACE, "Expected '{' at start of function block.");
    node->func_decl.block = parse_block(p, T_RBRACE);

    return node;
}

//- params                 = identifier, { ',', identifier }
//- 
static Param parse_param(Parser *p) {
    PRINT_FUNC_NAME;

    Token *param_tok = advance(p);

    Param param;
    param.name = str_intern(p, param_tok);

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

static void dump_parser_state(Parser *p) {
    printf("pos=%u, nodes=%u, statements=%u, errors=%u\n",
        p->pos, p->node_count, p->statement_count, p->error_count);
    printf("next token: %s\n", token_type_strings[peek(p)->type].data);
}

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
        advance(p);
        break;

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

static void parse_struct_init(Parser *p) {
    PRINT_FUNC_NAME;
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
            advance(p); // consume .
            expect(p, T_IDENT, "");
            continue;
        }

        if (peek(p)->type == T_LBRACE) {
            parse_struct_init(p);
            continue;
        }

        break;
    }
}

static bool is_expr_start(TokenType type) {
    switch (type) {
        case T_IDENT:
        case T_NUM:
        case T_STRING:
        case T_TRUE:
        case T_FALSE:
        case T_NIL:

        // unary operators (no unary plus atm)
        case T_MINUS:
        case T_NOT:
        //case T_AMPERSAND:   // address-of

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

/*
Note the asymmetry — collect_lhs_tuple takes the already-parsed first node
because your statement parser will have consumed it before knowing a comma
was coming, whereas collect_rhs_tuple starts fresh after the =.
*/

int collect_lhs_tuple(Parser *p, ParserNode *first, ParserNode **targets) {
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

int collect_rhs_tuple(Parser *p, ParserNode **values) {
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

static ParserNode *parse_ident(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_IDENTIFIER);
    Token *tok = previous(p);
    node->identifier = str_intern(p, tok);

    return node;
}

static ParserNode *parse_for_expr(Parser *p) {
    PRINT_FUNC_NAME;
    //expect(p, T_FOR, "FOR EXPECTED");
    ParserNode *ident = parse_ident(p); 
    advance(p); // Due to pratt parser need to advance/consume token
    expect(p, T_IN, "'in' must follow the iterator variable before enumeratable.");
    ParserNode *iter_expr = parse_expr(p);
    expect(p, T_LBRACE, "Expected '{' after for condition");    
    ParserNode *body = parse_block(p, T_RBRACE);

    ParserNode *node = new_node(p, NODE_FOR_EXPR);
    node->for_expr.ident = ident;
    node->for_expr.iter_expr = iter_expr;
    return node;
}

static ParserNode *parse_cast(Parser *p) {
    expect(p, T_LPAREN, "Expected '(' after cast operator");
    // TODO review for the moment we will just parse ident but this would
    // be a type expression (e.g. []u32 or ref T or anything)
    ParserNode *type = parse_ident(p);
    advance(p);
    expect(p, T_RPAREN, "Expected ')' after type expression.");

    ParserNode *node = new_node(p, NODE_CAST);
    node->cast.to_type = type;
    return node;
}

static ParserNode *parse_return(Parser *p) {
    PRINT_FUNC_NAME;
    
    ParserNode *expr = parse_expr(p);
    ParserNode *ret = new_node(p, NODE_RETURN);

    ret->ret.expr = expr;

    return ret;
}

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

static ParserNode *parse_block(Parser *p, TokenType block_end) {
    PRINT_FUNC_NAME;
    ParserNode *block = new_node(p, NODE_BLOCK);

    // while (peek(p)->type != T_END) {
    while (peek(p)->type != block_end) {
        if (match(p, T_NEWLINE) || match(p, T_SEMICOLON))
            continue;
        ParserNode *stmt = parse_stmt(p);
        block_add_stmt(block, stmt);

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

static ParserNode *parse_binary(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *rhs = parse_expr_prec(p, PREC_FACTOR);

    ParserNode *node = new_node(p, NODE_BINARY_OP);
    node->binary_op.lhs = lhs;
    node->binary_op.rhs = rhs; // TODO need to do plus

    return node;
}

static ParserNode *parse_assign(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;
    ParserNode *rhs = parse_expr_prec(p, PREC_LOWEST);

    /* if (rhs->kind == NODE_IDENTIFIER && is_expr_start(current(p).type)) {
        rhs = parse_bare_call(p, rhs);
    } */

    ParserNode *node = new_node(p, NODE_ASSIGNMENT);
    node->assign.lhs = lhs;
    node->assign.rhs = rhs;

    return node;
}

static ParserNode *parse_string(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_STRING);
    node->string = token_string(p, previous(p));
    return node;
}

static ParserNode *parse_nil(Parser *p) {
    PRINT_FUNC_NAME;
    return new_node(p, NODE_NIL);
}

static ParserNode *parse_grouping(Parser *p) {
    PRINT_FUNC_NAME;

    ParserNode *expr = parse_expr(p);
    expect(p, T_RPAREN, "Expected ')' after grouped expression.");
    return expr;
}

static ParserNode *parse_call(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

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

    ParserNode *node = new_node(p, NODE_FUNC_CALL);
    node->func_call.callee = lhs;
    node->func_call.args = args;
    node->func_call.args_count = count;
    return node;
}

static ParserNode *parse_array_lit(Parser *p) {
    PRINT_FUNC_NAME;

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

    ParserNode *node = new_node(p, NODE_ARRAY_LITERAL);
    node->array_literal.elems = elems;
    node->array_literal.count = count;
    node->array_literal.length_expr = length;
    return node;
}

static ParserNode *parse_index(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_INDEX);
    node->index.base = lhs;
    node->index.index = parse_expr_prec(p, PREC_LOWEST);
    expect(p, T_RSQBRACKET, "Expected ']' after index.");
    return node;
}

static ParserNode *parse_range(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_RANGE);
    node->range.lhs = lhs;
    node->range.inclusive = (previous(p)->type == T_DOTDOTEQ);
    node->range.rhs = parse_expr_prec(p, PREC_RANGE);
    return node;
}

static ParserNode *parse_dot(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode *node = new_node(p, NODE_DOT_ACCESS);
    node->dot_access.lhs = lhs;

    Token *member = expect(p, T_IDENT, "Expected member name after '.'.");
    node->dot_access.member =
        *member; // TODO should this node contain pointer to tok or tok
    return node;
}

static ParserNode *parse_unary(Parser *p) {
    ParserNode *node = new_node(p, NODE_UNARY);
    node->unary.op = previous(p)->type;
    node->unary.expr = parse_expr_prec(p, PREC_UNARY);
    return node;
}

static ParserNode *parse_bool(Parser *p) {
    PRINT_FUNC_NAME;
    ParserNode *node = new_node(p, NODE_BOOL);
    node->bool_value = previous(p)->type == T_TRUE;
    return node;
}

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

static ParserNode *parse_comma(Parser *p, ParserNode *lhs) {
    PRINT_FUNC_NAME;

    ParserNode **targets, **values;
    int target_count = collect_lhs_tuple(p, lhs, targets);
    expect(p, T_EQUALS, "Multiple values with comma should have assignment.");
    int value_count = collect_rhs_tuple(p, values);
    return make_multi_assign(p, targets, target_count, values, value_count);
}


static void skip_newlines(Parser *p) {
    while (current(p)->type == T_NEWLINE)
        advance(p);
}


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

