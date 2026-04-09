
#define _GNU_SOURCE // for MAP_ANONYMOUS

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "memory.h"
#include "string.h"

#include "platform.h"

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_fstat 5
#define SYS_mmap 9
#define SYS_mprotect 10
#define SYS_sigaction 13
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

static inline long syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
                     : "rcx", "r11", "memory");
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

_Noreturn
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

int console_read(char *buffer, size_t bufsz) {
    return read(STDIN_FILENO, buffer, bufsz);
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

// for macOS or other UNIX systems that have this
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

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

#define STACK_TRACE_HANDLER

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "xor  rbp, rbp\n"
        "mov  rdi, [rsp]\n"
        "lea  rsi, [rsp + 8]\n"
        "and  rsp, -16\n"
#ifdef STACK_TRACE_HANDLER
// We need to push argc/argv onto the stack and pop later before call to main
        "push rdi\n"
        "push rsi\n"
        "call set_signal_handler\n"
        "pop  rsi\n"
        "pop  rdi\n"
#endif
        "call main\n"
        "mov  edi, eax\n"
        "call exit\n"
    );
}





#ifdef STACK_TRACE_HANDLER

#define MAX_BACKTRACE_LINES 64

int error(const char *fmt, ...); // Forward declaration needed referring to print.c

int backtrace(void **buffer, int size) {
    return 0; // TODO
}

char **backtrace_symbols(void **buffer, int size) {
    return NULL;
}

void print_stacktrace(void) {
    void *buffer[MAX_BACKTRACE_LINES];
    char **symbols;

	int nptrs = backtrace(buffer, MAX_BACKTRACE_LINES);
	symbols = backtrace_symbols(buffer, nptrs);
	if(symbols == NULL)	{
		error("backtrace_symbols\n");
		exit(1);
	}

    // start at 2 to exclude this function and handler()
	for(uint32_t i = 2; i < (uint32_t) (nptrs-2); ++i) {
		//if addr2line failed, print what we can
		error("[%i] %s\n", nptrs-2-i-1, symbols[i]);
	}

    // TODO free symbols
}

#define SIGHUP           1
#define SIGINT           2
#define SIGQUIT          3
#define SIGILL           4
#define SIGTRAP          5
#define SIGABRT          6
#define SIGIOT           6
#define SIGBUS           7
#define SIGFPE           8
#define SIGKILL          9
#define SIGUSR1         10
#define SIGSEGV         11
#define SIGUSR2         12
#define SIGPIPE         13
#define SIGALRM         14
#define SIGTERM         15
#define SIGSTKFLT       16
#define SIGCHLD         17
#define SIGCONT         18
#define SIGSTOP         19
#define SIGTSTP         20
#define SIGTTIN         21
#define SIGTTOU         22
#define SIGURG          23
#define SIGXCPU         24
#define SIGXFSZ         25
#define SIGVTALRM       26
#define SIGPROF         27
#define SIGWINCH        28
#define SIGIO           29
#define SIGPOLL         SIGIO
/*
#define SIGLOST         29
*/
#define SIGPWR          30
#define SIGSYS          31
#define SIGUNUSED       31


void handler(int signal) {
    print_stacktrace();

    switch (signal) {
        case SIGTERM: 
            error("SIGTERM: termination request, sent to the program\n");
            break;
        case SIGSEGV:
            error("SIGSEGV: invalid memory access (segmentation fault)\n");
            break;
        case SIGINT:
            error("SIGINT: external interrupt, usually initiated by the user\n");
            break;
        case SIGILL:
            error("SIGILL: invalid program image, such as invalid instruction\n");
            break;
        case SIGABRT:
            error("SIGABRT: abnormal termination condition, as is e.g. initiated by abort()\n");
            break;
        case SIGFPE:
            error("SIGFPE: erroneous arithmetic operation such as divide by zero\n");
            break;
        default:
            error("Another signal triggered, value: %d\n", signal);
            break;
    }

    exit(1);
}

__attribute__((used, naked)) // needed so that it is not compiled out and prologue/epilogue is correct
static void sig_restorer(void) {
    // syscall: rt_sigreturn = 15
    __asm__ volatile (
        "mov $15, %%rax\n"
        "syscall\n"
        ::: "rax"
    );
}

/* static int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    int ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(13),          // rt_sigaction = 13
          "D"(signum),
          "S"(act),
          "d"(oldact),
          "r"((uint64_t)8)  // sigsetsize = 8
        : "rcx", "r11", "memory"
    );
    return ret;
} */
typedef void (*__sighandler_t) (int);

typedef struct {
    __sighandler_t sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    uint64_t sa_mask;
} KernelSigaction;

int sigaction(int sig, KernelSigaction *act, KernelSigaction *oldact) {
    uint64_t sigsetsize = 8;
    return (int)syscall4(SYS_sigaction, (long)sig, (long)act, (long)oldact, sigsetsize);
}

#define SA_RESTORER	0x04000000 // TODO should use something else or import, base image addr?

void set_signal_handler(void) {
    KernelSigaction sa = {
        .sa_handler  = handler,
        .sa_flags    = SA_RESTORER,
        .sa_restorer = sig_restorer,
        .sa_mask     = 0,
    };

    int ret; // for debugging

    ret |= sigaction(SIGTERM, &sa, 0);
    ret |= sigaction(SIGSEGV, &sa, 0);
    ret |= sigaction(SIGINT, &sa, 0);
    ret |= sigaction(SIGILL, &sa, 0);
    ret |= sigaction(SIGABRT, &sa, 0);
    ret |= sigaction(SIGFPE, &sa, 0);
    ret |= sigaction(SIGTERM, &sa, 0);
    ret |= sigaction(SIGTERM, &sa, 0);
    ret |= sigaction(SIGTERM, &sa, 0);

}
#endif

