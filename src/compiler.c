

#include <assert.h>

#include "memory.h"
#include "platform.h"

typedef enum {
    STAGE_UNINITIALIZED,
    STAGE_INITIALIZED,
    STAGE_LEXED,
    STAGE_PARSED,
    STAGE_CODEGEN,
    STAGE_RAN,
} CompileStage;

typedef struct {
    CompileStage stage;
    Lexer lexer;
    Parser parser;
    uint16_t *bytecode;
    size_t bytecode_size;
    Arena *arena;
    Arena *code_arena;
    Arena *temp_arena;
} Compiler;

FileBuf read_entire_file(const char *path, Arena *arena) {
    return platform_read_entire_file((String) { .data = (char*)path, .len = strlen(path) }, arena);
}

Compiler compiler_init(Arena *arena, Arena *code_arena, Arena *temp_arena) {
    return (Compiler) { 
	    .stage = STAGE_INITIALIZED, 
	    .arena = arena, .code_arena = code_arena, .temp_arena = temp_arena
    };
}

Compiler compiler_init_default(void) {
    Arena arena = arena_create(megabytes(16));
    Arena code_arena = arena_create_code(megabytes(4));
    Arena temp_arena = arena_create(megabytes(16));

    return compiler_init(&arena, &code_arena, &temp_arena);
}

void compiler_destroy(Compiler *c) {
    // TODO free the arenas
    memzero(c, sizeof(*c));
}

typedef enum {
    COMPILE_MODE_DEBUG,
    COMPILE_MODE_RELEASE,
} CompileMode;

void compile_source_string(Compiler *c, char *source, CompileMode mode) {
    // TODO
}

void compiler_reset(Compiler *c) {
    compiler_destroy(c);
    *c = compiler_init_default();
}

void compiler_lex_file(Compiler *c, const FileBuf *input_file) {
    assert(c->stage == STAGE_INITIALIZED);
    
    Token token;
    Lexer *lexer = &c->lexer;

    lexer->cap = 512;
    lexer->size = 0;
    lexer->tokens = new(c->arena, Token, lexer->cap);
    lexer->line = 1;
    lexer->input = *input_file;
    lexer->at_line_start = true;

    // Parse out the #!
    if (lexer->pos == 0 && 
            lexer->input.len > 2 &&
            lexer->input.data[0] == '#' &&
            lexer->input.data[1] == '!') {
        
        while (lex_peek(lexer) != '\n') lex_advance(lexer);
    }

    // TODO consume newline also above

    while ((token = lex_next(&c->lexer)).type != T_END) {
        // print(token_type_strings[token.type]); print(S("\n"));
        if (lexer->size >= lexer->cap) {
            lexer->cap *= 2;
            Token *new_tokens = new(c->arena, Token, lexer->cap);
            if (lexer->tokens) {
                memcpy(new_tokens, lexer->tokens, lexer->size * sizeof(Token));
            }
            lexer->tokens = new_tokens;
        }
        lexer->tokens[lexer->size++] = token;
    }

    lexer->tokens[lexer->size++] = (Token) { .type = T_END };

    c->stage = STAGE_LEXED;
}

String source_line(String source, size_t lineno) {
    // This is suboptimal as we are scanning source each time but sufficient for debug printing
    size_t cur_line = 1;
    size_t start = 0;

    for (size_t i = 0; i < source.len; i++) {
        if (cur_line == lineno) {
            start = i;
            break;
        }

        if (source.data[i] == '\n') cur_line++;
    }

    // if line not found
    if (cur_line != lineno) {
        return (String) { .data = NULL, .len = 0 };
    }

    // find end of line
    size_t end = start;
    while (end < source.len && source.data[end] != '\n') end++;

    return (String) {
        .data = source.data + start,
        .len = end - start
    };
}

#ifdef DEBUG
void debug_print_lexer(Compiler *c) {
    Lexer *lexer = &c->lexer;
    assert(lexer->size > 0);
    StringBuilder sb = sb_create_temp(*c->temp_arena);
    
    size_t cur_line = lexer->tokens[0].line;

    for (size_t i = 0; i < lexer->size; i++) {
        Token cur = lexer->tokens[i];
        sb_build(&sb, i);
        sb_build(&sb, S(":L"));
        sb_build(&sb, cur.line);
        sb_char(&sb, ':');
        sb_build(&sb, token_type_strings[cur.type]);

        if (cur.type != T_NEWLINE) {
            sb_char(&sb, ':');
            sb_char(&sb, '"');
            sb_build(&sb, ((String) { .data = lexer->input.data + cur.start, .len = cur.length }));
            sb_char(&sb, '"');
        }
        sb_newline(&sb);
    }
    sb_build(&sb, S("==========\n"));

    for (size_t i = 0; i < lexer->size; i++) {
        Token cur = lexer->tokens[i];

        if (cur.line != cur_line) {
            sb_newline(&sb);
            // output original source line
            sb_build(&sb, source_line(lexer->input, cur_line));
            sb_newline(&sb);
            cur_line = cur.line;
        }

        sb_build(&sb, token_type_strings[cur.type]);
        sb_char(&sb, ' ');
    }

    sb_newline(&sb);
    print_sb(sb);
}
#endif

void compiler_parse(Compiler *c) {
    assert(c->stage == STAGE_LEXED);
    c->stage = STAGE_PARSED;

    Lexer *lexer = &c->lexer;
    Parser *parser = &c->parser;

    parser->tokens = lexer->tokens;
    parser->token_count = lexer->size;
    parser->source = lexer->input;

    parser->node_cap = 256;
    parser->nodes = new(c->arena, ParserNode, parser->node_cap);

    parser->statement_cap = 256;
    parser->statements = new(c->arena, uint32_t, parser->statement_cap);

    parser->arena = c->arena;

    parse_program(&c->parser);
}

void print_sb(StringBuilder sb) {
    print(sb.buffer);
}

void sb_indent(StringBuilder *sb, size_t depth) {
    for (size_t i = 0; i < depth; i++) sb_build(sb, S("--"));
}

StringBuilder g_sb;

void sb_lhs(size_t index) {
    sb_build(&g_sb, S("(lhs = "));
    sb_build(&g_sb, index);
}

void sb_rhs(size_t index) {
    sb_build(&g_sb, S(", rhs = "));
    sb_build(&g_sb, index);
    sb_build(&g_sb, S(")"));
}

void debug_print_node_info(Parser p, StringBuilder sb, ParserNode n) {
    sb_build(&g_sb, node_kind_strings[n.kind]);
    
    //const char *rhs = ", rhs=";
    sb_build(&g_sb, S(" "));
#define NODE_DIFF(x) ((x) - &p.nodes[0])
    switch (n.kind) {
        case NODE_IDENTIFIER: 
            { sb_build(&g_sb, n.identifier.str); } break;
        case NODE_ASSIGNMENT:
            { sb_lhs(NODE_DIFF(n.assign.lhs)) ; sb_rhs(NODE_DIFF(n.assign.rhs)); } break;
        case NODE_NUMBER:
            { sb_build(&g_sb, (uint64_t)n.int_value); } break;
        case NODE_FUNC_CALL:
            { sb_lhs(NODE_DIFF(n.func_call.callee)); sb_build(&g_sb, S(")")); } break;
        case NODE_BINARY_OP:
            { sb_lhs(NODE_DIFF(n.binary_op.lhs)) ; sb_rhs(NODE_DIFF(n.binary_op.rhs)); } break;
        case NODE_BOOL:
            { if (n.bool_value) {sb_build(&g_sb, S("true"));} 
              else              {sb_build(&g_sb, S("false"));} 
            } break;
        //default: { exit(1); } break;
    }

    sb_build(&g_sb, S("\n"));
}

void debug_print_parser(Compiler *c) {
    Parser p = c->parser;
    assert(p.node_count > 0);
    g_sb = sb_create_temp(*c->temp_arena);

    for (size_t i = 0; i < p.node_count; i++) {
        ParserNode n = p.nodes[i];
        sb_build(&g_sb, i);
        sb_build(&g_sb, S(": "));

        debug_print_node_info(p, g_sb, n);
        //sb_newline(&sb);
    }
    
    sb_build(&g_sb, S("==========\n"));
    sb_build(&g_sb, S("STATEMENTS:\n"));

    for (uint32_t i; i < p.statement_count; i++) {
        uint32_t node_index = p.statements[i];
        ParserNode n = p.nodes[node_index];
        sb_build(&g_sb, (int)node_index);
        sb_build(&g_sb, S(": "));
        debug_print_node_info(p, g_sb, n);
    }

    print_sb(g_sb);
}

void compiler_codegen(Compiler *c) {
    assert(c->stage == STAGE_PARSED);
    c->stage = STAGE_CODEGEN;

    bootstrap_codegen(&c->parser);
}

void compiler_run(Compiler *c) {
    assert(c->stage == STAGE_CODEGEN);
    c->stage = STAGE_RAN;
}

