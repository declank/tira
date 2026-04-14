

#include "string.h"
typedef struct {
    char    *data;
    size_t   len;
    uint32_t id;
} StringEntry;

typedef struct {
    InternedStr name;
    int32_t     offset;
} LocalVar;

typedef struct {
    Arena  *arena;
    Parser *parser;
    
    // output buffers — data and text emitted separately, merged at end
    char  *data_buf;
    size_t data_used;
    size_t data_cap;
    
    char  *text_buf;
    size_t text_used;
    size_t text_cap;

    // string literal table
    StringEntry strings[2048];
    uint32_t string_count;

    // local variable table for current function
    LocalVar locals[2048]; // TODO dynamic
    uint32_t local_count;
    uint32_t stack_size; // current frame size, grows downward

    // label counter for jumps
    uint32_t label_count;

    uint32_t function_count;

    const char* filename;
} CodeGen;


static void codegen_block(CodeGen *g, Node_Block *block);
static void codegen_node(CodeGen *g, ParserNode *node);

static void emit(CodeGen *g, const char *fmt, ...) {
    va_list ap;
    size_t remaining = g->text_cap - g->text_used;
    va_start(ap, fmt);
    int needed = vsnprintf(g->text_buf + g->text_used, remaining, fmt, ap);
    va_end(ap);

    if (needed < 0) return; // encoding error

    if ((size_t)needed >= remaining) {
        // didn't fit — grow and retry
        size_t new_cap = next_pow2(g->text_used + needed + 1);
        g->text_buf = realloc_array(g->arena, g->text_buf, char, g->text_cap, new_cap);
        g->text_cap = new_cap;

        remaining = g->text_cap - g->text_used;
        va_start(ap, fmt);
        vsnprintf(g->text_buf + g->text_used, remaining, fmt, ap);
        va_end(ap);
    }

    g->text_used += needed;
}

static void emit_char(CodeGen *g, char c) {
    if (UNLIKELY(g->text_used >= g->text_cap)) {
        size_t new_cap = g->text_cap * 2;
        g->text_buf = realloc_array(g->arena, g->text_buf, char, g->text_cap, new_cap);
        g->text_cap = new_cap;
    }
    g->text_buf[g->text_used++] = c;
}

/* static void emit_data_char(CodeGen *g, char c) {
    if (UNLIKELY(g->data_used >= g->data_cap)) {
        size_t new_cap = g->data_cap * 2;
        g->data_buf = realloc_array(g->arena, g->data_buf, char, g->data_cap, new_cap);
        g->data_cap = new_cap;
    }
    g->data_buf[g->data_used++] = c;
} */

static void codegen_emit_externals(CodeGen *g) {
    emit(g, "\n.intel_syntax noprefix\n");
    emit(g, "\n.file 1 \"%s\"\n\n", g->filename);
    emit(g, ".section .text\n");
}

static void emit_data(CodeGen *g, const char *fmt, ...) {
    // printf into g->data_buf, grow if needed
}

static uint32_t emit_label(CodeGen *g) {
    emit(g, ".LBB%d_%d:\n", g->function_count, g->label_count);
    return g->label_count++;
}

static void codegen_block(CodeGen *g, Node_Block *block) {
    for (uint32_t i = 0; i < block->statements_count; i++)
        codegen_node(g, block->stmts[i]);
}

static void codegen_func_call(CodeGen *g, ParserNode *node) {
    Node_FuncCall *call = &node->func_call;
    ParserNode    *callee = call->callee;

    // evaluate args left to right into registers
    // System V AMD64: rdi, rsi, rdx, rcx, r8, r9
    static const char *arg_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
    (void)arg_regs; // used below per-arg

    // for now handle single string arg case (puts/print)
    if (call->args_count == 1) {
        codegen_node(g, call->args[0]); // result lands in rdi/rsi for strings
    }

    // emit call — map known tira names to runtime names
    if (string_equal(callee->identifier.str, S("puts")))
        emit(g, "    call puts_tira\n");
    else if (string_equal(callee->identifier.str, S("print")))
        emit(g, "    call print_tira\n");
    else if (string_equal(callee->identifier.str, S("gets")))
        emit(g, "    call gets_tira\n");
    else if (string_equal(callee->identifier.str, S("eval")))
        emit(g, "    call eval_tira\n");
    else if(callee->kind == NODE_DOT_ACCESS) {
        if (callee->dot_access.lhs->kind == NODE_IDENTIFIER) {
            Token *member_tok = (Token*)(g->parser->tokens + callee->dot_access.member_tok_index);
            String member = (String) { .data = &g->parser->source.data[member_tok->start],
                                       .len  = member_tok->length };
            emit(g, "    call %S    #%S.%S\n", member, callee->dot_access.lhs->identifier.str, member);
        } else {
            assert(0);
        }
    }
    else
        emit(g, "    call %S\n", callee->identifier.str);
}

static void codegen_func_decl(CodeGen *g, ParserNode *node) {
    Node_FuncDecl *decl = &node->func_decl;
    ParserNode    *ident = decl->identifier;

#ifdef OMIT_EXTRA_FUNC_CODEGEN
    if (!string_equal(ident->identifier.str, S("main"))) return;
#endif

    // reset locals for this function
    g->local_count = 0;
    g->stack_size  = 0;
    g->label_count = 0; // TODO: local vs global label counts?

    // function label
    const char *name = ident->identifier.str.data;
    size_t      nlen = ident->identifier.str.len;
    String      func_name = (String) {(char*)name, nlen};
    emit(g, "\n.global %S\n", func_name);
    emit(g, "%S:\n", func_name);


    //emit(g, "\nglobal %.*s\n", (int)nlen, name);
    //emit(g, "%.*s:\n", (int)nlen, name);
    //emit(g, "\n.global main\n");
    //emit(g, "main:\n");
    

    // prologue — reserve space for locals (placeholder, patch after block)
    emit(g, "    push rbp\n");
    emit(g, "    mov  rbp, rsp\n");
    // TODO patch stack reservation after knowing local count:
    // emit(g, "    sub  rsp, %d\n", aligned_stack_size);

    codegen_block(g, &decl->block->block);

    // epilogue
    emit(g, "    xor  eax, eax\n");
    emit(g, "    pop  rbp\n");
    emit(g, "    ret\n\n");

    g->function_count++;
}

static uint32_t codegen_intern_string(CodeGen *g, char *data, size_t len) {
    // check if already interned
    for (uint32_t i = 0; i < g->string_count; i++)
        if (g->strings[i].len == len && memcmp(g->strings[i].data, data, len) == 0)
            return g->strings[i].id;

    uint32_t id = g->string_count++;
    g->strings[id] = (StringEntry){ .data = data, .len = len, .id = id };
    return id;
}

static void codegen_string_literal(CodeGen *g, ParserNode *node) {
    // strip quotes
    char    *data = node->string.data + 1;
    size_t   len  = node->string.len  - 2;
    uint32_t id   = codegen_intern_string(g, data, len);
    // result: rdi = ptr, rsi = len
    //emit(g, "    mov  rdi, .L.str\n"); NO use lea
    emit(g, "    lea  rdi, [.LC%d]\n", id);
    emit(g, "    mov  rsi, %d\n", len); // TODO
}

static int32_t codegen_alloc_local(CodeGen *g, InternedStr name) {
    g->stack_size += 8;
    int32_t offset = -g->stack_size;
    //g->locals[g->local_count++] = (typeof(g->locals[0])){ name, offset };
    g->locals[g->local_count++] = (LocalVar){ name, offset };

    return offset;
}

static int32_t codegen_lookup_local(CodeGen *g, InternedStr name) {
    for (uint32_t i = 0; i < g->local_count; i++)
        if (string_equal(g->locals[i].name.str, name.str))
            return g->locals[i].offset;
    return 0; // not found — TODO error
}

static void codegen_var_decl(CodeGen *g, ParserNode *node) {
    Node_ConstVarDecl *decl = &node->const_var_decl;
    int32_t offset = codegen_alloc_local(g, decl->identifier->identifier);

    if (decl->rhs) {
        codegen_node(g, decl->rhs); // result in rax
        emit(g, "    mov qword ptr [rbp%+d], rax\n", offset);
    } else {
        emit(g, "    mov qword ptr [rbp%+d], 0\n", offset);
    }
}

static void codegen_identifier(CodeGen *g, ParserNode *node) {
    int32_t offset = codegen_lookup_local(g, node->identifier);
    if (offset != 0)
        emit(g, "    mov rax, qword ptr [rbp%+d]\n", offset);
    // else: global or function name — handled by caller
}

static void codegen_while(CodeGen *g, ParserNode *node) {
    ParserNode *cond = node->while_expr.cond;
    ParserNode *block = node->while_expr.block;

    int loop_label = emit_label(g);
    codegen_node(g, cond);
    emit(g, "    test al, 1\n");

    int exit_label = g->label_count; // reserve label index before emitting
    emit(g, "    je .LBB%d_%d\n", g->function_count, exit_label);
    g->label_count++;

    codegen_block(g, &block->block);
    emit(g, "    jmp .LBB%d_%d\n", g->function_count, loop_label);
    emit(g, ".LBB%d_%d:\n", g->function_count, exit_label);
}

static void codegen_if(CodeGen *g, ParserNode *node) {
    ParserNode *cond = node->if_expr.cond;
    ParserNode *then_branch = node->if_expr.then_branch;
    ParserNode *else_branch = node->if_expr.else_branch;

    codegen_node(g, cond);
    int else_label = g->label_count++;
    emit(g, "    test al, 1\n");
    emit(g, "    je .LBB%d_%d\n", g->function_count, else_label);
    codegen_node(g, then_branch);
    int exit_label = g->label_count++;
    emit(g, "    jmp .LBB%d_%d\n", g->function_count, exit_label);
    emit(g, ".LBB%d_%d:\n", g->function_count, else_label);
    codegen_node(g, else_branch);
    emit(g, ".LBB%d_%d:\n", g->function_count, exit_label);
}

#define EMIT_LOC emit_loc(g, line_no, col_no)
#define EMIT_LOC 

String source_line(String source, size_t lineno); // forward decl from compiler.c
static void emit_loc(CodeGen *g, size_t line_no, size_t col_no) {
    // Emit the line in the form
    // # line...
    //emit(g, "\n# %S", source_line(g->parser->source, line_no)); // sadly super slow
    emit(g, "\n.loc 1 %d %d\n", line_no, col_no);
}

static void codegen_node(CodeGen *g, ParserNode *node) {
    if (!node) return;

    Token *tok = (g->parser->tokens + node->tok_index);
    size_t line_no = tok->line;
    size_t col_no = 0; // TODO tok->column;

    switch (node->kind) {
        case NODE_FUNC_DECL:    codegen_func_decl(g, node);      break;
        case NODE_FUNC_CALL:    EMIT_LOC; codegen_func_call(g, node);      break;
        case NODE_BLOCK:        codegen_block(g, &node->block);  break;
        case NODE_STRING:       codegen_string_literal(g, node); break;
        case NODE_VAR_DECL:     EMIT_LOC; codegen_var_decl(g, node);   break;
        case NODE_IDENTIFIER:   codegen_identifier(g, node); break;
        case NODE_WHILE_EXPR:   EMIT_LOC; codegen_while(g, node);      break;
        case NODE_IF_EXPR:      EMIT_LOC; codegen_if(g, node);         break;
        case NODE_BOOL: {
            //EMIT_LOC;
/*             if (node->bool_value) { emit(g, "    mov  eax, 1\n"); }
            else                  { emit(g, "    xor  eax, eax\n"); } */

            static const char *bool_asm[] = {
                "    xor  eax, eax\n",
                "    mov  eax, 1\n",
            };
            emit(g, bool_asm[node->bool_value & 1]);
        } break;
        case NODE_RETURN: {
            //EMIT_LOC;
            if (node->ret.expr) codegen_node(g, node->ret.expr);
            emit(g, "    pop  rbp\n");
            emit(g, "    ret\n");
        } break;
        default:
            emit(g, "    # TODO: node kind %d: %S\n", node->kind, node_kind_strings[node->kind]);
            break;
    }
}

static void codegen_emit_data_section(CodeGen *g) {
    emit(g, ".section .data\n");
    for (uint32_t i = 0; i < g->string_count; i++) {
        StringEntry *s = &g->strings[i];
        emit(g, ".LC%d:\n", i);
        emit(g, "    .asciz \"");
        // emit the string bytes, escaping special chars
        for (size_t j = 0; j < s->len; j++) {
            char c = s->data[j];
            if (0);
            //if (c == '\n')      emit(g, "\", 10, \"");
            //else if (c == '\t') emit(g, "\", 9, \"");
            else                emit_char(g, c);
        }
        emit(g, "\"\n\n");
        //emit(g, "\", 0\n");
        //emit(g, "\", 0\n");
        //emit(g, "    str_%u_len equ %zu\n", s->id, s->len);
    }
}

void codegen_compile_program(CodeGen *g, Parser *p) {
    g->parser = p;

    // text section header + externals
    codegen_emit_externals(g);

    // walk top level — first node is the program block
    ParserNode *program = &p->nodes[0];
    assert(program->kind == NODE_BLOCK);
    codegen_block(g, &program->block);

    // prepend data section to final output
    codegen_emit_data_section(g);
}