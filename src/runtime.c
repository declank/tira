

#include "common.h"
#include "memory.c"
#include "string.c"
#include "print.c"

void puts_tira(String str) {
    printf("%S\n", str);
}

void print_tira(String str) {
    printf("%S", str);
}

String gets_tira(void) {
    return (String) {0};
}

int eval_tira(String exp) {
    return 0;
}

//typedef char** ArrayOfStrings;
typedef struct {
    String *data;
    uint64_t len;
} ArrayOfStrings;

void array_add(ArrayOfStrings arr, String s) {
    printf("array_add: %S\n", s);
}