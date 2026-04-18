

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *data;
    size_t len;
} String;

void puts_tira(String str) {
    printf("%.*s\n", (int)str.len, str.data);
}

void print_tira(String str) {
    printf("%.*s", (int)str.len, str.data);
}

char eval_tira_buf[BUFSIZ];

String read_line(void) {
    printf("read_line\n");
    if (fgets(eval_tira_buf, BUFSIZ, stdin) != 0) {
        eval_tira_buf[strcspn(eval_tira_buf, "\n")] = '\0';
        return (String) { eval_tira_buf, strlen(eval_tira_buf)};
    }
    return (String) {0};
}

String eval_tira(String exp) {
    printf("eval_tira\n");
}

void c_testing(void) {
    printf("Hello world\n");
}

//typedef char** ArrayOfStrings;
typedef struct {
    String *data;
    uint64_t len;
} ArrayOfStrings;

void arr_append(ArrayOfStrings arr, String s) {
    printf("arr_append\n");
}

