#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

# Make the build directory
mkdir -p build/

clang -ggdb -std=c17 -o build/tira_debug -DDEBUG \
    src/main.c src/platform_linux.c \
    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
    -no-pie -Wl,--build-id=none -fmerge-all-constants \
    -Wl,-e,_start

clang -g -fno-omit-frame-pointer -O3 -std=c17 -o build/tira \
    src/main.c src/platform_linux.c \
    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
    -no-pie -Wl,--build-id=none -fmerge-all-constants \
    -Wl,-e,_start


#strip --strip-all build/tira_opt

