

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "memory.c"
#include "string.c"
#include "platform.h"
#include "print.c"
#include "lexer.c"
#include "parser.c"
#include "semantic_ir.c"
#include "compiler.c"
#include "runtime.c"

int main(int argc, const char *argv[]) {
    (void) argc;
    (void) argv;

    if (argc != 2) {
        error("Input file not specified. Exiting.\n");
        return 1;
    }

    Arena arena = arena_create(megabytes(16));
    Arena code_arena = arena_create_code(megabytes(4));
    Arena temp_arena = arena_create(megabytes(16));
    
    Compiler c = compiler_init(&arena, &code_arena, &temp_arena);
    FileBuf input_file = read_entire_file(argv[1], &arena);

    print(S("==========\n"));
    print(input_file);
    print(S("==========\n"));
    
    compiler_lex_file(&c, &input_file);
    
    #ifdef DEBUG
    debug_print_lexer(&c);
    #endif

    print(S("==========\n"));

    compiler_parse(&c);
    print(S("==========\n"));
    debug_print_parser(&c);
    print(S("==========\n"));


    compiler_semantic(&c);
    print(S("==========\n"));

    return 0;
}


