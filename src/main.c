
#include "string.h"
static int _hits[65536];

#define HIT() (_hits[__LINE__]++)

#include <assert.h>
#include <limits.h>
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
#include "semantic.c"
#include "codegen.c"
#include "compiler.c"
//#include "runtime.c"

void print_arena_usage(Arena arena, Arena nodes_arena, Arena stmts_arena) {
    if (arena.used >= 1024 * 1024) {
        printf("Arena usage: %dMB\n", (int)(arena.used / 1024 / 1024));
    } else if (arena.used >= 1024) {
        printf("Arena usage: %dKB\n", (int)(arena.used / 1024));
    } else {
        printf("Arena usage: %d bytes\n", (int)arena.used);
    }

    if (nodes_arena.used >= 1024 * 1024) {
        printf("Nodes arena usage: %dMB\n", (int)(nodes_arena.used / 1024 / 1024));
    } else if (nodes_arena.used >= 1024) {
        printf("Nodes arena usage: %dKB\n", (int)(nodes_arena.used / 1024));
    } else {
        printf("Nodes arena usage: %d bytes\n", (int)nodes_arena.used);
    }

    if (stmts_arena.used >= 1024 * 1024) {
        printf("Statements arena usage: %dMB\n", (int)(stmts_arena.used / 1024 / 1024));
    } else if (stmts_arena.used >= 1024) {
        printf("Statements arena usage: %dKB\n", (int)(stmts_arena.used / 1024));
    } else {
        printf("Statements arena usage: %d bytes\n", (int)stmts_arena.used);
    }
}

void tests(void) {
    printf("%d\n", 20);
    printf("%d\n", -20);
    printf("======\n");
    printf("%+d\n", 20);
    printf("%+d\n", -20);
    printf("%+d\n", 0);
    printf("%+d\n", INT_MAX);
    printf("%+d\n", INT_MIN);

    exit(0);
}

typedef enum {
    MM_INVALID,
    MM_FUNCTION,
    MM_GLOBAL_VAR,
} ModuleMappingType;

typedef struct {
    ModuleMappingType type;
    const char *name;
    void *ptr;
} ModuleMapping;

typedef struct {
    uint64_t dummy;
} TiraContext;

typedef String TiraString;

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        tira_error("Input file not specified. Exiting.\n");
        return 1;
    }

    Arena arena = arena_create(megabytes(16));
    Arena code_arena = arena_create_code(megabytes(4));
    Arena temp_arena = arena_create(megabytes(16));
    Arena nodes_arena = arena_create(megabytes(16));
    Arena stmts_arena = arena_create(megabytes(16));

    Compiler c = compiler_init(&arena, &code_arena, &temp_arena, &nodes_arena, &stmts_arena);
    FileBuf input_file;

    if (!read_entire_file(&input_file, argv[1], &arena)) {
        tira_error("Unable to read input file\n");
        exit(1);
    }

    //print(input_file);
    compiler_lex_file(&c, &input_file);   
    #ifdef DEBUG
    debug_print_lexer(&c);
    #endif

    compiler_parse(&c);
    #ifdef DEBUG
    debug_print_parser(&c);
    #endif
    compiler_codegen(&c, argv[1]);

    print_arena_usage(arena, nodes_arena, stmts_arena);

    printf("HIT REPORT:\n");
    for (int i = 0; i < 65536; i++) {
        if (_hits[i]) {
            printf("Line %u: %d hits\n");
        }
    }

    return 0;
}


