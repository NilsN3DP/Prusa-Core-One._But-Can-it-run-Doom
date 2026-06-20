/* Newlib syscall stubs for bare-metal. Provides a real _sbrk so malloc works
 * (Doom uses the heap). Other syscalls are stubbed. */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/times.h>
#include <errno.h>
#include <stdint.h>

extern char end;          /* set by linker — start of heap */
extern uint32_t _estack;  /* top of stack */

#undef errno
extern int errno;

static char *heap_ptr = 0;

void *_sbrk(ptrdiff_t incr) {
    char *prev, *next;
    register char *sp __asm__("sp");
    if (heap_ptr == 0) {
        heap_ptr = &end;
    }
    prev = heap_ptr;
    next = heap_ptr + incr;
    /* keep a 4 KB guard below the current stack pointer */
    if (next > sp - 4096) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_ptr = next;
    return prev;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }
int _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status) { (void)status; for (;;) { } }
clock_t _times(struct tms *buf) { (void)buf; return (clock_t)-1; }
