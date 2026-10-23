/* backtrace() for Android: the unwinder of the NDK's libgcc, walking the frames that carry unwind tables (the recompiled game's
 * do: it is built with -funwind-tables). As glibc's, it does not include its own frame. Safe in a signal handler on arm64 (the
 * unwinder reads the signal frame's own tables). */
#include <setjmp.h>
#include <stdint.h>
#include <unwind.h>

#include "execinfo.h"

typedef struct Walk { void **buffer; int size, count, skip; } Walk;

static _Unwind_Reason_Code step(struct _Unwind_Context *context, void *arg) {
    Walk *w = arg;
    uintptr_t pc = _Unwind_GetIP(context);
    if (!pc) return _URC_END_OF_STACK;
    if (w->skip > 0) { w->skip--; return _URC_NO_REASON; }
    if (w->count >= w->size) return _URC_END_OF_STACK;
    w->buffer[w->count++] = (void *)pc;
    return _URC_NO_REASON;
}

/* The unwinder authenticates the return addresses of frames that say they are signed (pointer authentication, which the Pixel 9
 * faults on: SIGILL in libunwind's stepWithDwarf) and a thread caught in a fibre switch or in a driver has frames that are not what
 * their tables say. A walk that falls over (host.c's fault handler calls rt_backtrace_fault) is cut there, and gives what it had. */
static __thread sigjmp_buf *guard;

void rt_backtrace_fault(void) {
    if (guard) siglongjmp(*guard, 1);
}

int backtrace(void **buffer, int size) {
    Walk w = { buffer, size, 0, 1 };   /* (skipping this function's frame) */
    sigjmp_buf here, *outer = guard;
    if (sigsetjmp(here, 1)) { guard = outer; return w.count; }
    guard = &here;
    _Unwind_Backtrace(step, &w);
    guard = outer;
    return w.count;
}
