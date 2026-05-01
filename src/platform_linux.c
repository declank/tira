
#define _GNU_SOURCE // for MAP_ANONYMOUS

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "memory.h"
#include "string.h"

#include "platform.h"

// If you need cross platform syscall numbers refer to:
// https://www.chromium.org/chromium-os/developer-library/reference/linux-constants/syscalls/#cross-arch-numbers

// CAREFUL!! these below are x86_64 specific

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_fstat 5
#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_mprotect 10
#define SYS_sigaction 13
#define SYS_madvise 28
#define SYS_clone 56
#define SYS_exit 60
#define SYS_futex 202
#define SYS_sched_setaffinity 203
#define SYS_sched_getaffinity 204
#define SYS_clock_gettime 228
#define SYS_clone3 435

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7
#define CLOCK_REALTIME_ALARM     8
#define CLOCK_BOOTTIME_ALARM     9
#define CLOCK_SGI_CYCLE         10
#define CLOCK_TAI               11

#define MADV_DONTNEED   4


//FIXASSERT!!!

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

static inline long syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;

    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;

    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
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

void *mmap(void *addr, usize length, int prot, int flags, int fd,
           off_t offset) {
    return (void*)syscall6(SYS_mmap, (long)addr, (long)length, prot, flags, fd, offset);
}

int munmap(void *addr, usize len) {
    return (int)syscall2(SYS_munmap, (long)addr, (long)len);
}

int mprotect(void *addr, usize size, int prot) {
    return syscall3(SYS_mprotect, (long)addr, (long)size, prot);
}

ssize write(int fd, const void *buf, usize count) {
    return syscall3(SYS_write, fd, (long)buf, (long)count);
}

int open(const char *path, int flags, ... /* mode_t mode */) {
    int mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    return (int)syscall3(SYS_open, (long)path, flags, mode);
}

int fstat(int fd, struct stat *statbuf) {
    return (int)syscall2(SYS_fstat, fd, (long)statbuf);
}

int close(int fd) { 
    return (int)syscall1(SYS_close, (long)fd);
}

ssize read(int fd, void *buf, usize count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

int console_read(char *buffer, usize bufsz) {
    return read(STDIN_FILENO, buffer, bufsz);
}

int console_error(const char *error, usize length) {
    return write(STDERR_FILENO, error, length);
}

int console_out(const char *output, usize length) {
    return write(STDOUT_FILENO, output, length);
}

void print(String s) {
    // TODO assert(s.len <= ULONG_MAX);
    write(STDOUT_FILENO, s.data, s.len);
}

void print_char(char c) { write(STDOUT_FILENO, &c, 1); }

int open_file(const char *filename) {
    return open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

int write_to_file(int fd, char *buf, usize buf_size) {
    return write(fd, buf, buf_size);
}

int close_file(int fd) {
    return close(fd);
}

int platform_read_entire_file(FileBuf *buf, String path, Arena *arena) {
    //FileBuf result = {0};
    if (buf == NULL) return 0;

    // open(path, O_RDONLY)
    int fd = open(path.data, O_RDONLY);
    if (fd < 0)
        return 0;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return 0;
    }

    usize size = st.st_size;

    uint8_t *buffer = new (arena, uint8_t, size + 1);
    if (!buffer) {
        __builtin_trap();

        close(fd);
        return 0;
    }

    // read entire file
    usize total = 0;
    while (total < size) {
        // ssize n = syscall3(SYS_read, fd, buffer + total, size - total);
        ssize n = read(fd, buffer + total, size - total);

        if (n <= 0)
            break;
        total += n;
    }

    close(fd);

    buffer[size] = 0;
    buf->data = (char*)buffer;
    buf->len = total;
    return 1;
}

// for macOS or other UNIX systems that have this
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

void *mem_alloc(usize size) {
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    return (p != MAP_FAILED) ? p : NULL;
}

void *mem_alloc_code(usize size) {
    // TODO this needs to now swap using mprotect on Linux!
    return mem_alloc(size);
}

int mem_make_executable(void *p, usize size) {
    return mprotect(p, size, PROT_READ | PROT_EXEC);
}

int getchar(void) {
    static char buf[1];
    read(STDIN_FILENO, buf, 1);
    return buf[0];    
}

int madvise(void *addr, usize size, int advice) {
    return syscall3(SYS_madvise, (long)addr, (long)size, (long)advice);
}

int mem_dont_need(void *addr, usize size) {
    return madvise(addr, size, MADV_DONTNEED);
}

//__attribute__((noreturn))
_Noreturn
void __assert_fail(const char *assertion,
                   const char *file,
                   unsigned int line,
                   const char *function) {
    const char prefix[] = "Assertion failed: ";
    console_error(prefix, CSTR_LEN(prefix));
    console_error(assertion, strlen(assertion));

    // File
    const char file_prefix[] = ", in ";
    console_error(file_prefix, CSTR_LEN(file_prefix));
    console_error(file, strlen(file));

    tira_error(":");
    // Line
    tira_error("%d", line);

    // Function
    const char func_prefix[] = ", function ";
    console_error(func_prefix, CSTR_LEN(func_prefix));
    console_error(function, strlen(function));

    console_error("\n", 1);

    exit(1);
    //__builtin_unreachable();
}

// TODO: this needs to be changed to use vDSO instead of a syscall!
Time clock_time_monotonic_raw(void) {
    Time time;

    syscall2(SYS_clock_gettime, CLOCK_MONOTONIC_RAW, (long)&time);
    return time;
}

#define MAX_ARGS 64
int main(int argc, const char *argv[]);

//#define STACK_TRACE_HANDLER

#ifndef TIRA_USE_LIBC
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
#endif





#ifdef STACK_TRACE_HANDLER

#define MAX_BACKTRACE_LINES 64

int tira_error(const char *fmt, ...); // Forward declaration needed referring to print.c

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
		tira_error("print_stacktrace to be implemented\n");
		exit(1); // TODO
	}

    // start at 2 to exclude this function and handler()
	for(uint32_t i = 2; i < (uint32_t) (nptrs-2); ++i) {
		//if addr2line failed, print what we can
		tira_error("[%i] %s\n", nptrs-2-i-1, symbols[i]);
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
            tira_error("SIGTERM: termination request, sent to the program\n");
            break;
        case SIGSEGV:
            tira_error("SIGSEGV: invalid memory access (segmentation fault)\n");
            break;
        case SIGINT:
            tira_error("SIGINT: external interrupt, usually initiated by the user\n");
            break;
        case SIGILL:
            tira_error("SIGILL: invalid program image, such as invalid instruction\n");
            break;
        case SIGABRT:
            tira_error("SIGABRT: abnormal termination condition, as is e.g. initiated by abort()\n");
            break;
        case SIGFPE:
            tira_error("SIGFPE: erroneous arithmetic operation such as divide by zero\n");
            break;
        default:
            tira_error("Another signal triggered, value: %d\n", signal);
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
}
#endif

// EXPERIMENTAL CODE NOT TO BE USED. Exploring vDSO (Linux virtual Dynamic Shared Objects).
// It's a way of accessing some functions present in the kernel that are copied to userspace
// but that can also be called from userspace: e.g. clock_gettimeofday

// When it comes to process startup the Linux kernel adds an "aux vector" (auxv) on the stack above argc/argv
// https://articles.manugarg.com/aboutelfauxiliaryvectors
// https://berthub.eu/articles/posts/on-linux-vdso-and-clockgettime/

/*

position            content                     size (bytes) + comment
  ------------------------------------------------------------------------
  stack pointer ->  [ argc = number of args ]     4
                    [ argv[0] (pointer) ]         4   (program name)
                    [ argv[1] (pointer) ]         4
                    [ argv[..] (pointer) ]        4 * x
                    [ argv[n - 1] (pointer) ]     4
                    [ argv[n] (pointer) ]         4   (= NULL)

                    [ envp[0] (pointer) ]         4
                    [ envp[1] (pointer) ]         4
                    [ envp[..] (pointer) ]        4
                    [ envp[term] (pointer) ]      4   (= NULL)

                    [ auxv[0] (Elf32_auxv_t) ]    8
                    [ auxv[1] (Elf32_auxv_t) ]    8
                    [ auxv[..] (Elf32_auxv_t) ]   8
                    [ auxv[term] (Elf32_auxv_t) ] 8   (= AT_NULL vector)

                    [ padding ]                   0 - 16

                    [ argument ASCIIZ strings ]   >= 0
                    [ environment ASCIIZ str. ]   >= 0

  (0xbffffffc)      [ end marker ]                4   (= NULL)

  (0xc0000000)      < bottom of stack >           0   (virtual)
  ------------------------------------------------------------------------

*/

// Or simply:
// stack layout at _start:
// rsp+0:  argc
// rsp+8:  argv[0..argc]
// rsp+8*(argc+1): NULL
// then envp[], NULL
// then auxv[]

/*
typedef struct {
    uint64_t type;
    uint64_t value;
} AuxEntry;

#define AT_SYSINFO_EHDR 33  // vDSO base address

void *find_vdso(uint64_t *sp) {
    uint64_t argc = *sp;
    char **argv = (char **)(sp + 1);
    char **envp = argv + argc + 1;
    
    // skip envp
    char **e = envp;
    while (*e++) {}
    
    AuxEntry *aux = (AuxEntry *)e;
    while (aux->type != 0) {
        if (aux->type == AT_SYSINFO_EHDR)
            return (void *)aux->value;
        aux++;
    }
    return NULL;
}

#include <elf.h> // or define the structs yourself

typedef long (*clock_gettime_fn)(int, struct timespec *);

clock_gettime_fn find_clock_gettime(void *vdso_base) {
    Elf64_Ehdr *ehdr = vdso_base;
    Elf64_Shdr *shdrs = vdso_base + ehdr->e_shoff;
    
    Elf64_Shdr *dynsym = NULL;
    Elf64_Shdr *dynstr = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_DYNSYM)
            dynsym = &shdrs[i];
        if (shdrs[i].sh_type == SHT_STRTAB && i != ehdr->e_shstrndx)
            dynstr = &shdrs[i];
    }
    
    if (!dynsym || !dynstr) return NULL;
    
    Elf64_Sym *syms = vdso_base + dynsym->sh_offset;
    char *strtab    = vdso_base + dynstr->sh_offset;
    int nsyms       = dynsym->sh_size / sizeof(Elf64_Sym);
    
    for (int i = 0; i < nsyms; i++) {
        char *name = strtab + syms[i].st_name;
        if (str_eq(name, "__vdso_clock_gettime"))
            return (clock_gettime_fn)(vdso_base + syms[i].st_value);
    }
    return NULL;
}

clock_gettime_fn vdso_clock_gettime = find_clock_gettime(vdso_base);
struct timespec ts;
vdso_clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

*/

typedef struct cpu_set_t { unsigned long __bits[128/sizeof(long)]; } cpu_set_t;

// ref https://github.com/kraj/musl/blob/8cb84492b0245d70b2cd0edd523e2b55c7ad67a9/src/sched/sched_cpucount.c
int CPU_COUNT(cpu_set_t *set) {
    usize i, j, cnt=0;
	const unsigned char *p = (const void *)set;
	for (i=0; i<sizeof(*set); i++) for (j=0; j<8; j++)
		if (p[i] & (1<<j)) cnt++;
	return cnt;
}

// ref https://github.com/kraj/musl/blob/8cb84492b0245d70b2cd0edd523e2b55c7ad67a9/src/sched/affinity.c
int sched_getaffinity(pid_t pid, usize cpusetsize, cpu_set_t *mask) {
    long ret = (long)syscall3(SYS_sched_getaffinity, (long)pid, (long)cpusetsize, (long)mask);
    if (ret < 0) return ret;
    if (ret < cpusetsize) memset((uint8_t*)mask + ret, 0, cpusetsize - ret);
    return 0;
}

int os_get_logical_cores(void) {
    cpu_set_t cpuset;
    sched_getaffinity(0, sizeof(cpuset), &cpuset);
    return CPU_COUNT(&cpuset);
}



#define CLONE_VM        0x00000100  // share memory
#define CLONE_FS        0x00000200  // share filesystem
#define CLONE_FILES     0x00000400  // share file descriptors
#define CLONE_SIGHAND   0x00000800  // share signal handlers
#define CLONE_THREAD    0x00010000  // same thread group
#define CLONE_SYSVSEM   0x00040000  // share semaphores
#define CLONE_SETTLS    0x00080000  // set TLS register
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_CHILD_SETTID	 0x01000000

#define FUTEX_WAIT		0
#define FUTEX_WAKE		1
#define FUTEX_FD		2
#define FUTEX_REQUEUE		3
#define FUTEX_CMP_REQUEUE	4
#define FUTEX_WAKE_OP		5
#define FUTEX_LOCK_PI		6
#define FUTEX_UNLOCK_PI		7
#define FUTEX_TRYLOCK_PI	8
#define FUTEX_WAIT_BITSET	9

#define FUTEX_PRIVATE 128

#define FUTEX_CLOCK_REALTIME 256


// NOTE SYSCALL CLONE, not Clib clone() etc.
// The signature for the syscall is different for different platforms
// x86-64 below:
//            long clone(unsigned long flags, void *stack,
//                   int *parent_tid, int *child_tid,
//                   unsigned long tls);
__attribute__((naked)) long clone(unsigned long flags, void *stack,
        int *parent_tid, int *child_tid,
        unsigned long tls) {
    
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "mov eax, 56\n"
        "mov r10, rcx\n"    // child_tid: rcx -> r10
        "syscall\n"
        "ret\n"
    );
}

__attribute__((naked)) void thread_trampoline(thread_fn fn, void *arg) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        "mov rax, [rsp]\n"      // fn
        "mov rdi, [rsp+8]\n"    // arg
        "add rsp, 16\n"
        "call rax\n"
        "mov rax, 60\n"         // SYS_exit
        "xor rdi, rdi\n"
        "syscall\n"
        ::: "rax", "rdi", "memory"
    );
}

int futex(int *uaddr, int op, int val/* , const struct timespec *timeout, int *uaddr2, int val3 */) {
    return syscall3(SYS_futex, (long)uaddr, (long)op, (long)val);
}

#define STACK_SIZE kilobytes(64)



int thread_spawn(Thread *thread, thread_fn fn, void *arg) {
    void *stack = mmap(0, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    // TODO need to review calls to mmap
    // should not be checking for < 0    
    if ((uintptr_t)stack > (uintptr_t)-4096) {
        return -1;
    }

    // TODO another thing to consider is mmaping another region underneath, such that if the
    // stack overflows this will at least try to write into memory that is write-protected
    // from the OS

    thread->stack = stack;

    uintptr_t *sp = (uintptr_t*)((uintptr_t)((uint8_t*) stack + STACK_SIZE) & ~0xF);
    *(--sp) = (uintptr_t)arg;
    *(--sp) = (uintptr_t)fn;
    *(--sp) = (uintptr_t)thread_trampoline;

    unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD
        | CLONE_SYSVSEM | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    int thread_id = clone(flags, sp, &thread->tid, &thread->tid, 0);

    if (thread_id < 0) {
        munmap(stack, STACK_SIZE);
        return -1;
    }

    return thread_id;
}

void thread_join(Thread *t) {
    while (t->tid != 0) {
        int err = futex(&t->tid, FUTEX_WAIT | FUTEX_PRIVATE, t->tid);
    }
    munmap(t->stack, STACK_SIZE);
}

