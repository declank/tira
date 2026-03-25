#pragma once

/*
Following file will be for the bytecode VM:

Overall design decisions are:
16bit opcode (8bits may not be enough, or this allows for bitflagging/traps)
Variable length encoded
Values are directly encoded, no constant table but there is a function table

The above is designed for simplicity and to have a TAC/SSA like format that later will 
form the conversion to native code if required.
The main important decision is to minimise the number of instructions in generated code, to reduce
pressure on decode/dispatch.


Example source:

var width = 1280
var height = 720
puts(width*height)

Output:

loadk_int 0 1280
loadk_int 1 720

mul_int 2 0 1   OR   mul 2 0 1 (generic)
call f0 1 2

Decision needs to be made regarding closures and lexical variables referenced up particular frames,
or the use of dynamic binding if used.

*/

#include <stdint.h>
#include "parser.h"
#include "platform.h"
#include "print.h"


static uint16_t bytecode[4096];
static uint16_t code_size = 0;

typedef struct {
    void (*func)(int64_t);
} FuncEntry;

void tira_rt__puts(int64_t val) {
    static char buf[32];
    b8 negative = (val < 0);

    if (val == 0) {
        buf[0] = '0'; buf[1] = '\n'; buf[2] = '\0';
        console_out(buf, 2);
    }

    int i = 0;
    while (val > 0) { buf[i++] = '0' + val % 10; val /= 10; }

    // 025, i = 3

    int hi = i - 1, lo = 0;
    while (lo < hi) { 
        char tmp = buf[lo];
        buf[lo++] = buf[hi];
        buf[hi--] = tmp;
    }

    buf[i++] = '\n';
    buf[i]   = '0';
    console_out(buf, i);
}

static FuncEntry function_table[256];

static uint16_t var_slots = 0;

enum {
    BYOP_NOP,
    BYOP_LOADK,
    BYOP_LOADK_INT,
    BYOP_ADD_INT,
    BYOP_SUB_INT,
    BYOP_MUL_INT,
    BYOP_DIV_INT,
    BYOP_CALL,
};

void bytecode_emit_16(uint16_t val) {
    bytecode[code_size++] = val;
}

void bytecode_emit_64(uint64_t val) {
    bytecode[code_size++] = val >> 48 & 0xFFFF;
    bytecode[code_size++] = val >> 32 & 0xFFFF;
    bytecode[code_size++] = val >> 16 & 0xFFFF;
    bytecode[code_size++] = val & 0xFFFF;
}

uint16_t get_func_slot(ParserNode *callee) {
    return 0;
}

void bootstrap_codegen_inner(ParserNode *node) {
    uint16_t target_slot, lhs_slot, rhs_slot, func_slot;

    intptr_t int_value;
    uint16_t func_arg_count, arg_slot_start;

    switch (node->kind) {
        case NODE_VAR_DECL: {
            assert(node->const_var_decl.rhs->kind == NODE_NUMBER);
            target_slot = var_slots++;
            int_value = node->const_var_decl.rhs->int_value;

            printf("LOADK_INT %uh %lld\n", target_slot, int_value);
            bytecode_emit_16(BYOP_LOADK_INT);
            bytecode_emit_16(target_slot); bytecode_emit_64(int_value);
        } break;

        case NODE_BINARY_OP: {
            assert(node->binary_op.lhs->kind && node->binary_op.rhs->kind);
            target_slot = var_slots++;

            switch (node->binary_op.type) {
                case BINOP_ADD: {
                    printf("ADD_INT %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                    bytecode_emit_16(BYOP_ADD_INT);
                    bytecode_emit_16(target_slot); bytecode_emit_16(lhs_slot); bytecode_emit_16(rhs_slot);                    
                } break;

                case BINOP_SUB: {
                    printf("SUB_INT %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                    bytecode_emit_16(BYOP_SUB_INT);
                    bytecode_emit_16(target_slot); bytecode_emit_16(lhs_slot); bytecode_emit_16(rhs_slot);
                } break;

                case BINOP_MUL: {
                    printf("MUL_INT %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                    bytecode_emit_16(BYOP_MUL_INT);
                    bytecode_emit_16(target_slot); bytecode_emit_16(lhs_slot); bytecode_emit_16(rhs_slot);
                } break;

                case BINOP_DIV: {
                    printf("DIV_INT %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                    bytecode_emit_16(BYOP_DIV_INT);
                    bytecode_emit_16(target_slot); bytecode_emit_16(lhs_slot); bytecode_emit_16(rhs_slot);
                } break;

                default: { printf("Incorrect binary operation\n"); } break;
            }

            
        } break;

        case NODE_FUNC_CALL: {
            // TODO note that we need to have inner functions possibly so the function table
            // needs to account for this?
            assert(node->func_call.callee);
            assert(node->func_call.args_count <= UINT16_MAX);
            func_arg_count = node->func_call.args_count;
            func_slot = get_func_slot(node->func_call.callee);

            printf("CALL f%uh %uh %uh", func_slot, func_arg_count, arg_slot_start);
            bytecode_emit_16(BYOP_CALL); 
            bytecode_emit_16(func_slot); bytecode_emit_16(func_arg_count); bytecode_emit_16(arg_slot_start);
        } break;

        default: {
            printf("Not yet implemented.\n");
        } break;

    }
}

void bootstrap_codegen(Parser *p) {
    function_table[0] = (FuncEntry) {
        .func = tira_rt__puts,    
    };

    for (uint32_t i = 0; i < p->node_count; i++) {
        ParserNode *node = &p->nodes[i];
        bootstrap_codegen_inner(node);
    }
}




