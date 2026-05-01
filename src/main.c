
////////////////////////////////////////////////////////////
// TODO notes
//
// Spawn multiple threads and have the lexer run on these
// Introduce precision timers and simple profiling functionality in the base layer
// Run profiling against the lexer just implemented (e.g. 1 thread vs 16 thread test run)
// Changes to the base layer for type defintions used (e.g. s32, s64 etc.)
// Clean up the parser code so that it is clearer
// Write a test case for semantic analysis covering all identifiers in a file
// Move the parser to be multi-threaded
// Move the code emitter to be multi-threaded
// Run the syntehtic benchmarks comparing the threaded approach, weirdly expecting the lexer to be much faster
// Have the REPL:
//   1) do basic expressions "5 + 2 * 6"
//   2) call the C functions "e.g. puts("hello")"
//   3) test assignment "var str = read_line()" and "var x = 5 + 2"
//   4) Be able to re-run "main" from the REPL
//

////////////////////////////////////////////////////////////
// Recently completed
// 
// Rename: countof -> ARR_COUNT, lengthof -> CSTR_LEN
// Basic threading primitives added
// Clean up of general codebase
// Improvements of codegen to allow for comments when viewed in GDB/LLDB
// 



#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "string.h"
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

typedef enum {
    EXEC_MODE_COMPILER,
    EXEC_MODE_HELP,
//    EXEC_MODE_REPL,
} ExecutionMode;

typedef struct {
    String exec_name;
    StringList input_list;
    StringList flags;
} CommandLine;

static Arena arena = {0};

void print_help_message(void) {
    const char help_message[] =
        "OVERVIEW: Tira compiler and REPL.\n"
        "\n"
        "USAGE: tira [options] file..."
        "\n"
        "OPTIONS:\n"
        "  --help:    Show this help message\n"
//        "  --repl:    Start the REPL with the specified input files\n"
        ;
    
    printf("%s", help_message);
}

CommandLine args_to_command_line(int argc, const char *argv[]) {
    CommandLine cmd = {0};
    cmd.exec_name = str_from_cstr(argv[0]);

    // Parse flags and inputs
    for each_count_nz(i, argc) {
        String str = str_from_cstr(argv[i]);

        if (argv[i][0] == '-')  {
            stringlist_push(&arena, &cmd.flags, str);
        } else {
            stringlist_push(&arena, &cmd.input_list, str);
        }
    }

    return cmd;
}

#define make(T, count) new(&arena, T, count)

int main(int argc, const char *argv[]) {
    arena = arena_create(megabytes(4));

    CommandLine cmd = args_to_command_line(argc, argv);
    ExecutionMode mode = EXEC_MODE_COMPILER;

    if (string_in_stringlist(S("--help"), cmd.flags)) {
        print_help_message();
        exit(0);
    } /* else if (cmd_line_has_flag(cmd, S("repl"))) {
        mode = EXEC_MODE_REPL;
    } */

    if (!cmd.input_list.count) {
        tira_error("Error: Input file(s) not specified.\n");
        exit(1);
    }

    int cores = os_get_logical_cores();
    Thread *threads = make(Thread, cores);

    String file_contents;
    for each_node(filename, StringNode, cmd.input_list.first) {
        // for now just append to the one string
        if (!read_entire_file(&file_contents, filename->s.data, &arena)) {
            tira_error("Unable to read file: %S", filename);
        }
    }

    threads_spawn(threads, cores, lexer_thread_entry, &file_contents);
    threads_join(threads, cores);

    

    return 0;
}


