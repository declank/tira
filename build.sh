#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

    #src/smol.c \

clang -ggdb -std=c17 -o build/tira -DDEBUG \
    src/main.c src/platform_linux.c src/string.c src/memory.c src/son.c src/print.c src/lexer.c src/ddcg.c src/runtime.c src/semantic.c \
    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
    -no-pie -Wl,--build-id=none -fmerge-all-constants \
    -Wl,-e,_start

#    -Wall -Wextra -Wfloat-conversion -Wdouble-promotion -Wconversion -Wimplicit-fallthrough -pedantic \
#    -Wsizeof-pointer-memaccess \
# -Wl,-N -Wl,-s
#sstrip -z build/tira
# -Wl,-s
# -Wl,-N
# -Wl,-z,max-page-size=0x1000

#     -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion \

#wc -c build/tira
#strip --strip-all build/tira
#wc -c build/tira

