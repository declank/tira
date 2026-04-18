#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

# Make the build directory
mkdir -p build/

rm -f build/out.asm build/tira_out

clang -ggdb -std=c17 -o build/tira_debug -DDEBUG \
    src/main.c src/platform_linux.c \
    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
    -no-pie -Wl,--build-id=none -fmerge-all-constants \
    -Wl,-e,_start >/dev/null

./build/tira_debug tira_pkgs/repl/repl.tira
#cat build/out.asm

#clang -ggdb -c src/runtime.c -o build/runtime.o >/dev/null 2>&1
#clang -ggdb -c build/out.asm -o build/out.o >/dev/null 2>&1
#clang -ggdb -no-pie build/out.o build/runtime.o -o build/tira_out >/dev/null 2>&1

clang -ggdb -no-pie src/runtime.c build/out.asm -o build/tira_out

build/tira_out





#clang -g -fno-omit-frame-pointer -O3 -std=c17 -o build/tira \
#    src/main.c src/platform_linux.c \
#    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
#    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
#    -no-pie -Wl,--build-id=none -fmerge-all-constants \
#    -Wl,-e,_start


#    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
#    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
#    -no-pie -Wl,--build-id=none -fmerge-all-constants \
#    -Wl,-e,_start


#strip --strip-all build/tira_opt
#nasm -f elf64 -g -F dwarf -o build/out.o build/out.asm
#nasm -g -o build/out.o build/out.asm
#objdump -lSd build/tira_out | grep repl

