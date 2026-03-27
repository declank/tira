
#include "parser.h"
#include "string.h"

typedef struct Test Test;
struct Test {
    Test *next;
    String name;
    StringList output;
    b32 passed;
};

// @Cleanup, note that id_idx etc are not in parens!!
// This should be fine if not passing in an expression in the test cases.

#define MK_VAR_DECL(name, id_idx, rhs_idx) \
    [name] = (ParserNode){ .kind = NODE_VAR_DECL, \
        .const_var_decl = (Node_ConstVarDecl){ \
            .identifier = &test_input[id_idx], \
            .rhs        = &test_input[rhs_idx] }}

#define MK_IDENTIFIER(name, str) \
    [name] = (ParserNode){ .kind = NODE_IDENTIFIER, .identifier = S(str) }

#define MK_NUMBER(name, val) \
    [name] = (ParserNode){ .kind = NODE_NUMBER, .int_value = val }

#define MK_BINARY_OP(name, op, lhs_idx, rhs_idx) \
    [name] = (ParserNode){ .kind = NODE_BINARY_OP, \
        .binary_op = (Node_BinaryOp){ \
            .lhs  = &test_input[lhs_idx], \
            .rhs  = &test_input[rhs_idx], \
            .type = op }}

#define MK_BLOCK(name) \
    [name] = (ParserNode){ .kind = NODE_BLOCK, \
        .block = (Node_Block){ \
            .stmts = NULL, .statements_count = 0, \
            .params = NULL, .params_count = 0, }} // TODO these need to be patched

#define MK_FUNC_DECL(name, identifier_, block_) \
    [name] = (ParserNode){ .kind = NODE_FUNC_DECL, \
        .func_decl = (Node_FuncDecl){ \
            .func_data = NULL, \
            .identifier = &test_input[identifier_], \
            .block = &test_input[block_] }}

#define MK_RETURN(name, expr_) \
    [name] = (ParserNode){ .kind = NODE_RETURN, \
        .ret = (Node_Return) { \
            .expr = &test_input[expr_]}}


#define TEST_SUCCESS 0
#define TEST_FAILED 1

/*
Input:
var width = 1280
var height = 720

main {
    return width*height
}

Output:
loadk 0 1280
loadk 1 720
mul 2 0 1
mov 0 2
ret 
*/
int test_bytecode_generation(void) {
    // clang-format off

    // @Cleanup seems a bit excessive writing positionals these out like this
    // May want to instead consider using 0, 1, 2
    enum {
        PROG_BLOCK,
        WIDTH_DECL,
        WIDTH_DECL_ID,
        WIDTH_VAL,
        HEIGHT_DECL,
        HEIGHT_DECL_ID,
        HEIGHT_VAL,
        MAIN_DECL,
        MAIN_ID,
        MAIN_BLOCK,
        RETURN,
        WIDTH_REF,
        MUL,
        HEIGHT_REF,
        PROG_COUNT,
    };

    // TODO reference to other array elements within declaration is undefined behaviour!!
    ParserNode test_input[PROG_COUNT] = {
        MK_BLOCK        (PROG_BLOCK),
        MK_VAR_DECL     (WIDTH_DECL, WIDTH_DECL_ID, WIDTH_VAL),
        MK_IDENTIFIER   (WIDTH_DECL_ID, "width"),
        MK_NUMBER       (WIDTH_VAL, 1280),
        MK_VAR_DECL     (HEIGHT_DECL, HEIGHT_DECL_ID, HEIGHT_VAL),
        MK_IDENTIFIER   (HEIGHT_DECL_ID, "height"),
        MK_NUMBER       (HEIGHT_VAL, 720),
        MK_FUNC_DECL    (MAIN_DECL, MAIN_ID, MAIN_BLOCK),
        MK_IDENTIFIER   (MAIN_ID, "main"),
        MK_BLOCK        (MAIN_BLOCK),
        MK_RETURN       (RETURN, MUL),
        MK_IDENTIFIER   (WIDTH_REF, "width"),
        MK_BINARY_OP    (MUL, BINOP_MUL, WIDTH_REF, HEIGHT_REF),
        MK_IDENTIFIER   (HEIGHT_REF, "height"),
    };

    // TODO due to variable length instructions we will need to write tests
    // with disassembled strings.
    const char *expected[] = {
        "loadk 0 1280",
        "loadk 1 720",
        "mul 2 0 1",
        "mov 0 2",
        "ret",
    };

    // clang-format on

    return TEST_FAILED;
}

int main(int argc, const char *argv[]) {
    (void) argc;
    (void) argv;

    int tests_failed = 0;
    tests_failed += test_bytecode_generation();

    if (tests_failed) {
        error("%d tests failed!!!\n.", tests_failed);
    } else {
        printf("All tests passed.\n");
    }

    return tests_failed;
}

