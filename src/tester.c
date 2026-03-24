
#include "string.h"

typedef struct Test Test;
struct Test {
    Test *next;
    String name;
    StringList output;
    b32 passed;
};

int main(int argc, const char *argv[]) {
    (void) argc;
    (void) argv;

    int tests_failed = 0;



    return tests_failed;
}

