#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

clang -ggdb -std=c17 -o build/tira -DDEBUG \
    src/main.c src/platform_linux.c \
    -nostdlib -ffreestanding -fno-exceptions -fno-rtti \
    -mno-stack-arg-probe -fno-stack-protector -fno-jump-tables -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
    -no-pie -Wl,--build-id=none -fmerge-all-constants \
    -Wl,-e,_start

