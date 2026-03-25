
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "memory.h"
#include "platform.h"
#include "string.h"

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_fstat 5
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_exit 60

static inline long syscall1(long n, long a1) {
    long ret;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a1)
                     : "rcx", "r11", "memory");

    return ret;
}

static inline long syscall2(long n, long a1, long a2) {
    long ret;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a1), "S"(a2)
                     : "rcx", "r11", "memory");

    return ret;
}

static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;

    __asm__ volatile("syscall"
                     : "=a"(ret)              // return value in rax
                     : "a"(n),                // rax = syscall number
                       "D"(a1),               // rdi
                       "S"(a2),               // rsi
                       "d"(a3)                // rdx
                     : "rcx", "r11", "memory" // clobbered by syscall
    );

    return ret;
}

static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5,
                            long a6) {
    long ret;

    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");

    return ret;
}

// Is there a potential issue here with casting or do value ranges have to be asserted?

[[noreturn]]
void exit(int status) {
    // Note there is no implementation of atexit and on_exit

    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset) {
    return (void*)syscall6(SYS_mmap, (long)addr, (long)length, prot, flags, fd, offset);
}

int mprotect(void *addr, size_t size, int prot) {
    return syscall3(SYS_mprotect, (long)addr, (long)size, prot);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, fd, (long)buf, (long)count);
}

int open(const char *path, int flags, ... /* mode_t mode */) {
    return (int)syscall3(SYS_open, (long)path, flags, 0);
}

int fstat(int fd, struct stat *statbuf) {
    return (int)syscall2(SYS_fstat, fd, (long)statbuf);
}

int close(int fd) { 
    return (int)syscall1(SYS_close, (long)fd);
}

ssize_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

int console_error(const char *error, size_t length) {
    return write(STDERR_FILENO, error, length);
}

int console_out(const char *output, size_t length) {
    return write(STDOUT_FILENO, output, length);
}

void print(String s) {
    // TODO assert(s.len <= ULONG_MAX);
    write(STDOUT_FILENO, s.data, s.len);
}

void print_char(char c) { write(STDOUT_FILENO, &c, 1); }

FileBuf platform_read_entire_file(String path, Arena *arena) {
    FileBuf result = {0};

    // open(path, O_RDONLY)
    int fd = open(path.data, O_RDONLY);
    if (fd < 0)
        return result;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return result;
    }

    size_t size = st.st_size;

    uint8_t *buffer = new (arena, uint8_t, size + 1);
    if (!buffer) {
        __builtin_trap();

        close(fd);
        return result;
    }

    // read entire file
    size_t total = 0;
    while (total < size) {
        // ssize_t n = syscall3(SYS_read, fd, buffer + total, size - total);
        ssize_t n = read(fd, buffer + total, size - total);

        if (n <= 0)
            break;
        total += n;
    }

    close(fd);

    buffer[size] = 0;
    result.data = (char*)buffer;
    result.len = total;
    return result;
}

void *mem_alloc(size_t size) {
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    return (p != MAP_FAILED) ? p : NULL;
}

void *mem_alloc_code(size_t size) {
    // TODO this needs to now swap using mprotect on Linux!
    return mem_alloc(size);
}

int mem_make_executable(void *p, size_t size) {
    return mprotect(p, size, PROT_READ | PROT_EXEC);
}

int getchar(void) {
    static char buf[1];
    read(STDIN_FILENO, buf, 1);
    return buf[0];    
}

//__attribute__((noreturn))
_Noreturn
void __assert_fail(const char *assertion,
                   const char *file,
                   unsigned int line,
                   const char *function) {
    const char prefix[] = "Assertion failed: ";
    console_error(prefix, lengthof(prefix));
    console_error(assertion, strlen(assertion));

    // File
    const char file_prefix[] = ", file ";
    console_error(file_prefix, lengthof(file_prefix));
    console_error(file, strlen(file));

    // Function
    const char func_prefix[] = ", function ";
    console_error(func_prefix, lengthof(func_prefix));
    console_error(function, strlen(function));

    console_error("\n", 1);

    exit(1);
    //__builtin_unreachable();
}

#define MAX_ARGS 64
int main(int argc, const char *argv[]);

/* void _start(void) {
    register uint64_t *stack __asm__("rsp");

    int argc = (int)stack[0];
    const char **argv = (const char **)&stack[1];
    const char **envp = (const char **)&stack[argc + 2];

    __asm__ volatile("andq $-16, %%rsp" ::: "rsp");
    
    int ret = main(argc, argv);
    exit(ret);
} */

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "xor  rbp, rbp\n"
        "mov  rdi, [rsp]\n"
        "lea  rsi, [rsp + 8]\n"
        "and  rsp, -16\n"
        "call main\n"
        "mov  edi, eax\n"
        "call exit\n"
    );
}

