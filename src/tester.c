
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>

#include "common.h"
#include "memory.c"
#include "string.c"
#include "platform.h"
#include "print.c"
#include "lexer.c"
#include "parser.c"
#include "bootstrap_codegen.c"
#include "compiler.c"

typedef struct Test Test;
struct Test {
    Test *next;
    String name;
    StringList output;
    b32 passed;
};

#define MK_VAR_DECL(name, id_idx, rhs_idx) \
    [name] = (ParserNode){ .kind = NODE_VAR_DECL, \
        .const_var_decl = (Node_ConstVarDecl){ \
            .identifier = &NODES_ARR_NAME[id_idx], \
            .rhs        = &NODES_ARR_NAME[rhs_idx] }}

#define MK_IDENTIFIER(name, str) \
    [name] = (ParserNode){ .kind = NODE_IDENTIFIER, .identifier = S(str) }

#define MK_NUMBER(name, val) \
    [name] = (ParserNode){ .kind = NODE_NUMBER, .int_value = val }

#define MK_BINARY_OP(name, op, lhs_idx, rhs_idx) \
    [name] = (ParserNode){ .kind = NODE_BINARY_OP, \
        .binary_op = (Node_BinaryOp){ \
            .lhs  = &NODES_ARR_NAME[lhs_idx], \
            .rhs  = &NODES_ARR_NAME[rhs_idx], \
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
            .identifier = &NODES_ARR_NAME[identifier_], \
            .block = &NODES_ARR_NAME[block_] }}

#define MK_RETURN(name, expr_) \
    [name] = (ParserNode){ .kind = NODE_RETURN, \
        .ret = (Node_Return) { \
            .expr = &NODES_ARR_NAME[expr_]}}


#define TEST_SUCCESS 0
#define TEST_FAILED 1

void parse_source(Compiler *c, char *source) {
}

bool check_parse_nodes_match(ParserNode *expected, size_t expected_count,
                             ParserNode *actual, size_t actual_count)
{
    if (expected_count != actual_count) { return false; }

    for (size_t i = 0; i < expected_count; i++) {
        if (!nodes_match(&expected[i], &actual[i])) return false;
    }

    return true;
}

bool check_bytecode_strings_match(char **expected_bytecode_text, size_t expected_instruction_count,
                                  char **actual_bytecode_text, size_t actual_instruction_count)
{
    return false;
}

///// Start of tests /////

int test_printf(void) {
    // Note this test isn't automated
    printf("Hello world!\n");
    printf("There are %d bottles on the wall\n", 99);
    printf("-5 = %d\n", -5);
    printf("INT_MIN is: %d\n", INT_MIN);
    printf("INT_MAX is: %d\n", INT_MAX);
    printf("UINT64_MAX is: %zu\n", UINT64_MAX);
    return TEST_SUCCESS;
}

int test_parser(void) {
    char source[] = 
        "var width = 1280\n"
        "var height = 720\n\n"
        "main {\n"
        "    return width*height\n"
        "}\n";

#define NODES_ARR_NAME expected
    ParserNode expected[] = {
        MK_BLOCK        (0),
        MK_VAR_DECL     (1, 2, 3),
        MK_IDENTIFIER   (2, "width"),
        MK_NUMBER       (3, 1280),
        MK_VAR_DECL     (4, 5, 6),
        MK_IDENTIFIER   (5, "height"),
        MK_NUMBER       (6, 720),
        MK_FUNC_DECL    (7, 8, 9),
        MK_IDENTIFIER   (8, "main"),
        MK_BLOCK        (9),
        MK_RETURN       (10, 12),
        MK_IDENTIFIER   (11, "width"),
        MK_BINARY_OP    (12, BINOP_MUL, 11, 13),
        MK_IDENTIFIER   (13, "height"),
    };
#undef NODES_ARR_NAME

    Compiler compiler = compiler_init_default();
    parse_source(&compiler, source);

    if (!check_parse_nodes_match(expected, countof(expected), compiler.parser.nodes, compiler.parser.node_count)) {
        compiler_destroy(&compiler);
        return TEST_FAILED;
    }

    compiler_destroy(&compiler);
    return TEST_SUCCESS;
}

int test_bytecode_generation(void) {
    // TODO reference to other array elements within declaration is undefined behaviour!!
    char source[] = 
        "var width = 1280\n"
        "var height = 720\n\n"
        "main {\n"
        "    return width*height\n"
        "}\n";

    char *expected_debug[] = {
        "loadk_int 0 1280",
        "loadk_int 1 720",
        "mul_int 0 0 1",
        "ret"
    };

    char *expected_release[] = {
        "loadk_int 0 921600",
        "ret"
    };

    Compiler compiler = compiler_init_default();
    compile_source_string(&compiler, source, COMPILE_MODE_DEBUG);

    char **disasm = NULL;
    size_t instruction_count;

    bytecode_disasm(compiler.bytecode, &disasm, &instruction_count);
    if (!check_bytecode_strings_match(expected_debug, countof(expected_debug), disasm, instruction_count)) { // @Clean merge with above line (e.g. disassemble bytecode and compare)
        compiler_destroy(&compiler);
        return TEST_FAILED;
    }

    compiler_reset(&compiler);
    compile_source_string(&compiler, source, COMPILE_MODE_RELEASE);
    bytecode_disasm(compiler.bytecode, &disasm, &instruction_count);
    if (!check_bytecode_strings_match(expected_debug, countof(expected_debug), disasm, instruction_count)) { // @Clean merge with above line (e.g. disassemble bytecode and compare)
        compiler_destroy(&compiler);
        return TEST_FAILED;
    }

    compiler_destroy(&compiler);
    return TEST_SUCCESS;
}

///// End of tests /////

#define RUN_TEST(test) do { tests_failed += test; } while(0)

int main(int argc, const char *argv[]) {
    (void) argc;
    (void) argv;

    int tests_failed = 0;
    RUN_TEST(test_printf());
    RUN_TEST(test_parser());
    RUN_TEST(test_bytecode_generation());

    if (tests_failed) {
        error("%d tests failed!!!\n.", tests_failed);
    } else {
        printf("All tests passed.\n");
    }

    return tests_failed;
}

