
@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: build\gc.exe src\gc.c src\platform_win32.c

clang -g -std=c17 -o build\tira.exe src\main.c src\platform_win32.c src\string.c src\memory.c ^
    -Wall -Wextra -Wfloat-conversion -Wdouble-promotion -Wconversion -Wimplicit-fallthrough -pedantic ^
    -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion ^
    -Wsizeof-pointer-memaccess ^
    -nostdlib -ffreestanding -fno-exceptions -fno-rtti ^
    -lkernel32 -lshell32 -luser32 -lgdi32 ^
    -mno-stack-arg-probe ^
    -Xlinker /SUBSYSTEM:CONSOLE -Xlinker /STACK:0x100000,0x100000 -Xlinker /INCREMENTAL:NO

IF %ERRORLEVEL% NEQ 0 (
    echo Build Failed
    exit /b %ERRORLEVEL%
)

build\tira.exe input.tira
::build\gc.exe

IF %ERRORLEVEL% NEQ 0 (
    echo Tira Failed
    exit /b %ERRORLEVEL%
)

