

#include <assert.h>

#include "common.h"
#include "memory.h"
#include "platform.h"
#include "string.h"

typedef enum {
    STAGE_UNINITIALIZED,
    STAGE_INITIALIZED,
    STAGE_LEXED,
    STAGE_PARSED,
    STAGE_SEMANTIC,
    STAGE_CODEGEN,
    STAGE_RAN,
    STAGE_COUNT,
} CompileStage;

typedef struct { 
    Time start_time;
    Time end_time;
    uint64_t mem_used;
} PassTimer;

typedef struct {
    CompileStage stage;
    Lexer lexer;
    Parser parser;
    SemanticAnalyzer sema;
    CodeGen codegen;

    uint16_t *bytecode;
    size_t bytecode_size;
    Arena *arena;
    Arena *code_arena;
    Arena *temp_arena;
    Arena *nodes_arena;
    Arena *stmts_arena;

    PassTimer timers[STAGE_COUNT];
} Compiler;

int read_entire_file(FileBuf *buf, const char *path, Arena *arena) {
    return platform_read_entire_file(buf, (String) { .data = (char*)path, .len = strlen(path) }, arena);
}

Compiler compiler_init(Arena *arena, Arena *code_arena, Arena *temp_arena, Arena *nodes_arena, Arena *stmts_arena) {
    return (Compiler) { 
	    .stage = STAGE_INITIALIZED, 
	    .arena = arena, .code_arena = code_arena, .temp_arena = temp_arena, .nodes_arena = nodes_arena, .stmts_arena = stmts_arena,
    };
}

Compiler compiler_init_default(void) {
    Arena arena = arena_create(megabytes(16));
    Arena code_arena = arena_create_code(megabytes(4));
    Arena temp_arena = arena_create(megabytes(16));
    Arena nodes_arena = arena_create(megabytes(16));
    Arena stmts_arena = arena_create(megabytes(16));

    return compiler_init(&arena, &code_arena, &temp_arena, &nodes_arena, &stmts_arena);
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

void start_pass_timer(Compiler *c, CompileStage stage) {
    assert(stage < STAGE_COUNT);

    // ...
}

void end_pass_timer(Compiler *c, CompileStage stage) {
    assert(stage < STAGE_COUNT);

    // ...
}

void compiler_lex_file(Compiler *c, const FileBuf *input_file) {
    assert(c->stage == STAGE_INITIALIZED);

    start_pass_timer(c, STAGE_LEXED);
    
    Token token;
    Lexer *lexer = &c->lexer;

    //lexer->cap = 1000000;
    lexer->cap = 256;
    lexer->size = 0;
    lexer->tokens = new(c->arena, Token, lexer->cap);
    lexer->line = 1;
    lexer->input = *input_file;
    lexer->at_line_start = true;

    if (!lexer->tokens) {
        tira_error("Not enough memory\n");
        exit(1);
    }

    // Divide the input file by average identifier length
    uint64_t initial_table_cap = next_pow2(input_file->len / 8);
    intern_table_init(lexer, c->arena, initial_table_cap);

    if (lexer->table.slots == NULL) {
        tira_error("Lexer intern table slots not initialised\n");
        exit(1);
    } 
    
    if (lexer->pos == 0 && input_file->len > 2 &&
            input_file->data[0] == '#' && input_file->data[1] == '!') {
        while (lexer->input.data[lexer->pos] != '\n') lexer->pos++;
    }

    // TODO consume newline also above

    while ((token = lex_next(&c->lexer)).type != T_END) {
        if (UNLIKELY(lexer->size >= lexer->cap)) {
            size_t new_cap = lexer->cap * 2;
            Token *new_tokens = realloc_array(c->arena, lexer->tokens, Token, lexer->cap, new_cap);
            lexer->cap = new_cap;
            lexer->tokens = new_tokens;
        }
        lexer->tokens[lexer->size++] = token;
    }

    lexer->tokens[lexer->size++] = (Token) { .type = T_END };

    end_pass_timer(c, STAGE_LEXED);
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

//#ifdef DEBUG
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
#if 1   // switch used here as source_line() is incredibly slow but fine for debugging
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
#endif

    sb_newline(&sb);

    sb_build(&sb, S("Lines lexed: ")); sb_build(&sb, lexer->line); sb_newline(&sb);
    sb_build(&sb, S("Tokens: ")); sb_build(&sb, lexer->size); sb_newline(&sb);

    print_sb(sb);

}

void compiler_parse(Compiler *c) {
    assert(c->stage == STAGE_LEXED);
    c->stage = STAGE_PARSED;

    Lexer *lexer = &c->lexer;
    Parser *parser = &c->parser;

    parser->tokens = lexer->tokens;
    parser->token_count = lexer->size;
    parser->source = lexer->input;

    parser->node_cap = 256;
    parser->nodes = new(c->nodes_arena, ParserNode, parser->node_cap);
    parser->nodes_arena = c->nodes_arena;
    parser->misc_arena = c->arena;
    
    parser->stmts_arena = c->stmts_arena;

    // Fixup, touch all the nodes for paging
    // TODO: investigate RIORegisterBuffer for win32 for touching pages:
    // https://serverframework.com/asynchronousevents/2011/10/windows-8-registered-io-buffer-strategies.html
    for (size_t i = 0; i < parser->node_cap; i++) {
        parser->nodes[i] = (ParserNode){0};
    }

    if (!parser->nodes) {
        tira_error("Not enough memory\n");
        exit(1);
    }

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

void debug_print_node_extrainfo(Parser p, StringBuilder sb, ParserNode n) {
    sb_build(&g_sb, node_kind_strings[n.kind]);
    
    //const char *rhs = ", rhs=";
    //sb_build(&g_sb, S(" "));
    sb_build(&g_sb, S(" [parent = "));
    sb_build(&g_sb, (int)n.parent); // FIXME
    sb_build(&g_sb, S("] "));
#define NODE_DIFF(x) ((x) - &p.nodes[0])
    switch (n.kind) {
        case NODE_IDENTIFIER: 
            { sb_build(&g_sb, n.identifier.str); } break;
        //case NODE_FUNC_DECL:
        //    { sb_build(&g_sb, n.func_decl.identifier->string); } break;
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
        default: { /* unhandled extra info printing */ } break;
    }
}

void debug_print_node_typeinfo(Parser p, StringBuilder sb, ParserNode n) {
    sb_build(&g_sb, S(" "));

    switch (n.ty) {
        case TI_ARR_OF_STRING:
            sb_build(&g_sb, S("<[]String>"));
            break;

        case TI_bool:
            sb_build(&g_sb, S("<bool>"));
            break;

        case TI_i64:
            sb_build(&g_sb, S("<i64>"));
            break;

        case TI_nil:
            sb_build(&g_sb, S("<nil>"));
            break;

        case TI_String:
            sb_build(&g_sb, S("<String>"));
            break;

        case TI_UNDETERMINED:
        default:
            break;
    }
}

void debug_print_parser(Compiler *c) {
    Parser p = c->parser;
    assert(p.node_count > 0);
    g_sb = sb_create_temp(*c->temp_arena);

    for (size_t i = 0; i < p.node_count; i++) {
        ParserNode n = p.nodes[i];
        sb_build(&g_sb, i);
        sb_build(&g_sb, S(": "));

        debug_print_node_extrainfo(p, g_sb, n);
        debug_print_node_typeinfo(p, g_sb, n);
        sb_build(&g_sb, S("\n"));
    }
    print_sb(g_sb);
}

void compiler_codegen(Compiler *c, const char* filepath) {
    assert(c->stage == STAGE_PARSED);
    c->stage = STAGE_CODEGEN;

    CodeGen *g = &c->codegen;
    g->arena = c->arena;

    // TODO this data_cap/text_cap is super messy, will cause infinite loop in vsprintf as the buffer is fixed
    g->data_cap = 2048;
    g->data_buf = new(c->arena, char, g->data_cap);
    g->text_cap = 2048;
    g->text_buf = new(c->arena, char, g->text_cap);
    g->filename = filepath;

    codegen_compile_program(g, &c->parser);

    int bytes_written = 0;
    int fd = open_file("build/out.asm");
    if (fd) {
        bytes_written += write_to_file(fd, g->data_buf, g->data_used);
        bytes_written += write_to_file(fd, g->text_buf, g->text_used);
        close_file(fd);
    }
}

void compiler_semantic_ir(Compiler *c) {
    assert(c->stage == STAGE_PARSED);
    c->stage = STAGE_SEMANTIC;

    c->sema.parser = &c->parser;
    c->sema.arena = c->arena;
    c->sema.entities_cap = 256;

    c->sema.entities = new(c->arena, SemanticEntity, c->sema.entities_cap);

    semantic1(&c->sema);
}



