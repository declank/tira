
int _fltused = 0;

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include "platform.h"
#include "memory.h"
#include "string.h"
//#include "tira.h"

int main(int argc, const char* argv[]);

static HANDLE console_input_handle = NULL;
static HANDLE console_output_handle = NULL;
static HANDLE console_error_handle = NULL;

#if 1

_Noreturn
void abort(void) {
#ifdef _DEBUG
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#endif
    TerminateProcess(GetCurrentProcess(), 3);
    __builtin_unreachable();
}

// _wassert implementation Windows
//__declspec(noreturn)
_Noreturn
void __cdecl _wassert(const wchar_t* expr, const wchar_t* file, uint32_t line) {
    wchar_t buffer[512];
    wsprintfW(buffer, L"Assert failed: %s in %s:%u\n", expr, file, line);
    OutputDebugStringW(buffer);
    DWORD bytes_written;
    WriteConsoleW(console_error_handle, buffer, (DWORD)lstrlenW(buffer), &bytes_written, NULL);
    __debugbreak();
    TerminateProcess(GetCurrentProcess(), 3);
#if defined(_MSC_VER)
    __assume(0);
#else
    __builtin_unreachable();
#endif
}

#endif 

#define MAX_ARGS 64
#define MAX_ARG_LEN 260

void *mem_alloc(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void *mem_alloc_code(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

void print(String s) {
    DWORD bytes_written;
    assert(s.len <= ULONG_MAX);
    WriteConsoleA(console_output_handle, s.data, (DWORD)s.len, &bytes_written, NULL);
}

void print_cstr(const char *str) {
  print((String) { .data = str, .len = strlen(str) }); 
}

void print_char(char c) {
    DWORD bytes_written;
    WriteConsoleA(console_output_handle, &c, 1, &bytes_written, NULL);
}

int console_out(const char* output, size_t length) {
    DWORD bytes_written;
    assert(length <= ULONG_MAX);
    WriteConsoleA(console_output_handle, output, (DWORD)length, &bytes_written, NULL);
    return bytes_written;
}

int console_error(const char* error, size_t length) {
    DWORD bytes_written;
    assert(length <= ULONG_MAX);
    WriteConsoleA(console_error_handle, error, (DWORD)length, &bytes_written, NULL);
    return bytes_written;
}

void newline(void) {
  DWORD bytes_written;
  WriteConsoleA(console_output_handle, "\n", 1, &bytes_written, NULL);
}

BOOL init_console_handles(void) {
    console_input_handle = GetStdHandle(STD_INPUT_HANDLE);
    console_output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    console_error_handle = GetStdHandle(STD_ERROR_HANDLE);

    if (console_input_handle == INVALID_HANDLE_VALUE ||
        console_output_handle == INVALID_HANDLE_VALUE ||
        console_error_handle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    return TRUE;
}

void flush_cpu_cache(void* mem, size_t code_size) {
    FlushInstructionCache(GetCurrentProcess(), mem, code_size);
}

void mem_release(void* mem) {
    VirtualFree(mem, 0, MEM_RELEASE);
}

typedef enum {
    FILE_MODE_READ = 0x0001,
} FileMode;

HANDLE open_file(const char* path, int mode) {
    // TODO cross platform INVALID_HANDLE_VALUE
    HANDLE h = INVALID_HANDLE_VALUE;
    
    if (mode == FILE_MODE_READ) {
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    
    return h;
    
}

#if 1
FileBuf platform_read_entire_file(String path, Arena *arena) {
    // TODO note here that the buffer size is max 32bit even though I am using LARGE_INTEGER here for calculating file size
    // Need to handle this error
    // Also consider using CreateFileMapping + MapViewOfFile for large files (might be useful utilities for later)

    FileBuf buf = {0};
    assert(path.len <= (INT_MAX-1));
    
    // TODO conversions to UTF-16
    WCHAR path_w[MAX_PATH];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)path.data, (int)path.len, path_w, MAX_PATH);

    if (wlen <= 0) return buf;
    path_w[wlen] = '\0';

    HANDLE h = CreateFileW(path_w, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return buf;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(h, &file_size)) {
        CloseHandle(h);
        return buf;
    }

    if ((uint64_t)file_size.QuadPart > (arena->size - arena->used)) {
        CloseHandle(h);
        return buf;
    }

    void *data = new(arena, uint8_t, file_size.QuadPart);
    if (!data) {
        CloseHandle(h);
        return buf;
    }

    DWORD bytes_read;
    assert(file_size.QuadPart <= ULONG_MAX);
    if (!ReadFile(h, data, (DWORD)file_size.QuadPart, &bytes_read, NULL)) {
        CloseHandle(h);
        return buf;
    }

    CloseHandle(h);
    buf.data = data;
    buf.len = bytes_read;
    return buf;

    /*

    if (file_size.HighPart > 0) {
        // This case it is larger than 32 bits so won't fit, error out
        CloseHandle(h);
        return false;
    }

    DWORD bytes_read;
    if (!ReadFile(h, output, (DWORD)file_size.QuadPart, &bytes_read, NULL) || bytes_read != (DWORD)file_size.QuadPart) {
        CloseHandle(h);
        return false;
    } */
}

#endif // 0

bool close_file(HANDLE file) {
    return CloseHandle(file);
}

bool get_file_size(const char* path, size_t* size) {
    return false;
}

/*bool console_read_line(char* line, size_t length) {
    DWORD bytes_read;
    assert(length <= ULONG_MAX);
    return ReadConsoleA(console_input_handle, line, (DWORD)length, &bytes_read, NULL);
}*/

//static char *arg_storage[MAX_ARGS][MAX_ARG_LEN] = {0};

int mainCRTStartup(void) {
    // Init console handles
    if (!init_console_handles()) {
        return 1;
    }
    
    // Extend console mode
/*     DWORD in_mode = 0; GetConsoleMode(console_input_handle, &in_mode);
    in_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    SetConsoleMode(console_input_handle, in_mode); */
    
    // Pass Ctrl+C to program
    //SetConsoleCtrlHandler(ctrl_handler, TRUE);
    
    // Parse command line
    
    int argc = 0;

    LPWSTR *argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv_w) return -1;

    if (argc > MAX_ARGS) argc = MAX_ARGS; // TODO

    static char argbuf[4096];
    static const char *argv[MAX_ARGS];
    int argbuf_off = 0;

    for (int i = 0; i < argc; i++) {
        int needed = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, NULL, 0, NULL, NULL);
        argv[i] = &argbuf[argbuf_off];
        WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, &argbuf[argbuf_off], needed, NULL, NULL);
    }

    LocalFree(argv_w);

    int ret = main(argc, argv);
    ExitProcess(ret);
}
