
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

#include "string.h"
int bytecode_disasm(uint16_t *bytecode, char ***disasm, size_t *instruction_count) {
    

    return 0; // return instruction count if succeeds, else return -1?
}

static uint16_t bytecode[4096];
static uint16_t code_size_bytes = 0;
static uint16_t code_size_instructions = 0;

typedef struct {
    //int64_t (*func)(int64_t);
    void *func;
} FuncEntry;

typedef struct {
    TiraVal *members;
    size_t count;
} ArrayLiteralEntry;

static FuncEntry function_table[256];
static ArrayLiteralEntry array_literal_table[256];
static String string_table[256];
static TiraVal var_table[256];

static uint16_t var_slots = 0;
static uint16_t arr_slots = 0;
static uint16_t arr_slots_capacity = 256;

enum {
    BYOP_NOP,
    BYOP_LOADK,
    BYOP_LOADK_INT,
    BYOP_LOAD_ARRAY,

    BYOP_ADD,
    BYOP_SUB,
    BYOP_MUL,
    BYOP_DIV,

    BYOP_ADD_INT,
    BYOP_SUB_INT,
    BYOP_MUL_INT,
    BYOP_DIV_INT,

    BYOP_CALL,
    BYOP_IS_NIL,

    BYOP_EQUAL_TO,
    BYOP_NOT_EQUAL_TO,
    BYOP_EQUAL_TO_STRING,
    BYOP_NOT_EQUAL_TO_STRING,
    
    BYOP_LOGICAL_AND,
    BYOP_LOGICAL_OR,

    BYOP_LOGICAL_AND_BOOL,
    BYOP_LOGICAL_OR_BOOL,
    
    BYOP_SET_TRUE,
    BYOP_SET_FALSE,
};

void bytecode_emit_16(uint16_t val) {
    bytecode[code_size_bytes++] = val;
}

void bytecode_emit_64(uint64_t val) {
    bytecode[code_size_bytes++] = val >> 48 & 0xFFFF;
    bytecode[code_size_bytes++] = val >> 32 & 0xFFFF;
    bytecode[code_size_bytes++] = val >> 16 & 0xFFFF;
    bytecode[code_size_bytes++] = val & 0xFFFF;
}

uint16_t get_func_slot(ParserNode *callee) {
    // TODO change to hashes nocheckin
    String id_str = callee->identifier.str;
    if (0);
    else if (string_compare(id_str, S("puts")) == 0) { return 0; }
    else if (string_compare(id_str, S("gets")) == 0) { return 1; }
    else if (string_compare(id_str, S("eval")) == 0) { return 2; }
    else if (string_compare(id_str, S("print")) == 0) { return 3; }

    return 0;
}

TiraVal *alloc_cg_array(size_t count) {
    return NULL;
}

uint16_t add_array_literal_to_table(Node_ArrayLiteral arr) {
    assert(arr_slots < arr_slots_capacity - 1);
    ArrayLiteralEntry *entry = &array_literal_table[arr_slots++];
    entry->count = arr.count;

    // allocate and copy elements
    entry->members = alloc_cg_array(arr.count);
    memcpy(entry->members, arr.elems, arr.count);

    return 0;
}

void bootstrap_codegen_inner(ParserNode *node) {
    uint16_t target_slot, lhs_slot, rhs_slot, func_slot;

    intptr_t int_value;
    b8 bool_value;
    uint16_t func_arg_count, arg_slot_start;

    switch (node->kind) {
        case NODE_VAR_DECL: {
            ParserNode *rhs = node->const_var_decl.rhs;
            //assert(node->const_var_decl.rhs->kind == NODE_NUMBER);
            //if (rhs->kind != NODE_NUMBER) break;
            
            switch (rhs->kind) {
                case NODE_NUMBER: {
                    target_slot = var_slots++;
                    int_value = node->const_var_decl.rhs->int_value;

                    printf("LOADK_INT %uh %lld\n", target_slot, int_value);
                    bytecode_emit_16(BYOP_LOADK_INT);
                    bytecode_emit_16(target_slot); bytecode_emit_64(int_value);
                } break;

                case NODE_BOOL: {
                    bool_value = node->const_var_decl.rhs->bool_value;
                    if (bool_value == true) {
                        printf("SET_TRUE %uh\n", target_slot);
                        bytecode_emit_16(BYOP_SET_TRUE); bytecode_emit_16(target_slot);
                    } else {
                        printf("SET_FALSE %uh\n", target_slot);
                        bytecode_emit_16(BYOP_SET_FALSE); bytecode_emit_16(target_slot);
                    }
                } break;

                case NODE_ARRAY_LITERAL: {
                    Node_ArrayLiteral array_literal = node->const_var_decl.rhs->array_literal;
                    uint16_t array_literal_slot = add_array_literal_to_table(array_literal);

                    printf("LOAD_ARRAY %uh %uh\n", target_slot, array_literal_slot);
                    bytecode_emit_16(BYOP_LOAD_ARRAY);
                    bytecode_emit_16(target_slot); bytecode_emit_16(array_literal_slot);
                }

                default: {
                    error("== not implemented for RHS kind: %S\n", node_kind_strings[rhs->kind]);
                } break;
            }
            
            
        } break;

        case NODE_BINARY_OP: {
            ParserNodeKind lhs_kind = node->binary_op.lhs->kind;
            ParserNodeKind rhs_kind = node->binary_op.rhs->kind;
            assert(lhs_kind != NODE_INVALID && rhs_kind != NODE_INVALID);

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

                case BINOP_EQUAL_TO: {
                    switch (rhs_kind) {
                        case NODE_NIL: {
                            printf("IS_NIL %uh %uh", target_slot, rhs_slot);   
                            bytecode_emit_16(BYOP_IS_NIL);
                            bytecode_emit_16(target_slot); bytecode_emit_16(rhs_slot);
                        } break;

                        case NODE_STRING: {
                            printf("STRING_EQUAL_TO %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                            bytecode_emit_16(BYOP_EQUAL_TO_STRING);
                            bytecode_emit_16(target_slot); bytecode_emit_16(lhs_slot); bytecode_emit_16(rhs_slot);
                        } break;

                        default: {
                            error("== not implemented for RHS kind: %S\n", node_kind_strings[rhs_kind]);
                            assert(0);
                        } break;
                    }
                    
                } break;

                case BINOP_LOGICAL_AND: {
                    // Assumes both sides return bool
                    printf("LOGICAL_AND_BOOL %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                    bytecode_emit_16(BYOP_LOGICAL_AND_BOOL);
                    bytecode_emit_16(target_slot); bytecode_emit_16(rhs_slot);
                } break;

                case BINOP_LOGICAL_OR: {
                    // Assumes both sides return bool
                    printf("LOGICAL_OR_BOOL %uh %uh %uh", target_slot, lhs_slot, rhs_slot);
                    bytecode_emit_16(BYOP_LOGICAL_OR_BOOL);
                    bytecode_emit_16(target_slot); bytecode_emit_16(rhs_slot);
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

            printf("CALL f%uh %uh %uh\n", func_slot, func_arg_count, arg_slot_start);
            bytecode_emit_16(BYOP_CALL); 
            bytecode_emit_16(func_slot); bytecode_emit_16(func_arg_count); bytecode_emit_16(arg_slot_start);
        } break;

        default: {
            printf("Not yet implemented: ");
            print(node_kind_strings[node->kind]);
            printf("\n");
        } break;

    }
}

int64_t tira_rt__puts(int64_t val); // TODO cleanup this forward declaration or refactor these to capture required funcs
int64_t tira_rt__gets(void);
int64_t tira_rt__eval(int64_t input);
int64_t tira_rt__print(void);

void bootstrap_codegen(Parser *p) {
    function_table[0] = (FuncEntry) { .func = tira_rt__puts };
    function_table[1] = (FuncEntry) { .func = tira_rt__gets };
    function_table[2] = (FuncEntry) { .func = tira_rt__eval };
    function_table[3] = (FuncEntry) { .func = tira_rt__print };

    for (uint32_t i = 0; i < p->node_count; i++) {
        ParserNode *node = &p->nodes[i];
        bootstrap_codegen_inner(node);
    }
}


