/* Fibers of the runtime's own, for Windows (see net_rbfiber.h).
 *
 * A Windows fiber keeps where it stopped inside Windows (its context in an undocumented structure),
 * so a rollback could not put a fiber back where it was. These keep it where the Linux build's do
 * (linux/win32.c): the callee-saved registers pushed on the fiber's own stack, and the stack
 * pointer in the fiber - so the part of the stack in use, from that pointer to the top, is the
 * fiber's whole state, and copying it back (at the same addresses) restores it.
 *
 * What Windows keeps per fiber besides, these keep too: the thread's stack bounds in its TEB
 * (StackBase, StackLimit, DeallocationStack), which the unwinder and the kernel's stack growth go
 * by - saved from the TEB when a fiber is left and put back when it is entered (as Boost.Context
 * does). Each stack is reserved like CreateFiberEx's, with its top committed and a guard page below,
 * so it grows as Windows' own do. Fiber-local storage stays the thread's (as on Linux).
 *
 * The switch, in the x64 Windows calling convention: rbx, rbp, rdi, rsi, r12-r15 and xmm6-xmm15
 * are callee-saved, with MXCSR's and the x87 control word's control bits. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_rbfiber.h"

#ifdef _WIN32
#define TEB_DEALLOCATION_STACK 0x1478   /* (x64 TEB) */
#define COMMIT (256u << 10)             /* committed at first, at the top */
#define HEADROOM 64                     /* zero bytes above the first frame: an unwinder reading past it finds no return address */

typedef struct RbFiber {
    void *sp;                           /* where it stopped (while not running) */
    void *param;                        /* GetFiberData */
    LPFIBER_START_ROUTINE fn;
    u8 *base, *top;                     /* its reservation; the top of its stack (NULL: a thread's own stack) */
    void *tib_base, *tib_limit, *tib_dealloc;   /* its stack bounds, for the TEB (while not running) */
    volatile LONG running;
} RbFiber;
static __declspec(thread) RbFiber *current;

void rbf_switch_asm(void **save_sp, void *load_sp);
void rbf_trampoline(void);
__asm__(
    ".text\n"
    ".globl rbf_switch_asm\n"
    "rbf_switch_asm:\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %rdi\n"
    "    pushq %rsi\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    subq $0xA8, %rsp\n"
    "    movaps %xmm6, 0x00(%rsp)\n"
    "    movaps %xmm7, 0x10(%rsp)\n"
    "    movaps %xmm8, 0x20(%rsp)\n"
    "    movaps %xmm9, 0x30(%rsp)\n"
    "    movaps %xmm10, 0x40(%rsp)\n"
    "    movaps %xmm11, 0x50(%rsp)\n"
    "    movaps %xmm12, 0x60(%rsp)\n"
    "    movaps %xmm13, 0x70(%rsp)\n"
    "    movaps %xmm14, 0x80(%rsp)\n"
    "    movaps %xmm15, 0x90(%rsp)\n"
    "    stmxcsr 0xA0(%rsp)\n"
    "    fnstcw 0xA4(%rsp)\n"
    "    movq %rsp, (%rcx)\n"
    "    movq %rdx, %rsp\n"
    "    movaps 0x00(%rsp), %xmm6\n"
    "    movaps 0x10(%rsp), %xmm7\n"
    "    movaps 0x20(%rsp), %xmm8\n"
    "    movaps 0x30(%rsp), %xmm9\n"
    "    movaps 0x40(%rsp), %xmm10\n"
    "    movaps 0x50(%rsp), %xmm11\n"
    "    movaps 0x60(%rsp), %xmm12\n"
    "    movaps 0x70(%rsp), %xmm13\n"
    "    movaps 0x80(%rsp), %xmm14\n"
    "    movaps 0x90(%rsp), %xmm15\n"
    "    ldmxcsr 0xA0(%rsp)\n"
    "    fldcw 0xA4(%rsp)\n"
    "    addq $0xA8, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rsi\n"
    "    popq %rdi\n"
    "    popq %rbx\n"
    "    popq %rbp\n"
    "    ret\n"
    /* A new fiber's first switch "returns" here, with the fiber in r12. */
    ".globl rbf_trampoline\n"
    "rbf_trampoline:\n"
    "    subq $0x20, %rsp\n"
    "    movq %r12, %rcx\n"
    "    call rbf_entry\n"
    "    ud2\n");

void rbf_entry(RbFiber *f);
void rbf_entry(RbFiber *f) {
    f->fn(f->param);
    fprintf(stderr, "a fiber's start routine returned\n");
    abort();
}

static NT_TIB *tib(void) { return (NT_TIB *)NtCurrentTeb(); }
static void **tib_dealloc(void) { return (void **)((u8 *)NtCurrentTeb() + TEB_DEALLOCATION_STACK); }

void *rbf_convert(void *param) {
    if (current) return current;
    RbFiber *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    f->param = param;
    f->running = 1;
    current = f;
    return f;
}

void *rbf_create(SIZE_T reserve, LPFIBER_START_ROUTINE fn, void *param) {
    SIZE_T size = (reserve ? reserve : (SIZE_T)1 << 20) + 0xFFFF;
    size &= ~(SIZE_T)0xFFFF;
    u8 *base = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    if (!base) return NULL;
    u8 *top = base + size, *low = top - COMMIT;
    if (!VirtualAlloc(low, COMMIT, MEM_COMMIT, PAGE_READWRITE) || !VirtualAlloc(low - 0x1000, 0x1000, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD)) {
        VirtualFree(base, 0, MEM_RELEASE);
        return NULL;
    }
    RbFiber *f = calloc(1, sizeof *f);
    if (!f) { VirtualFree(base, 0, MEM_RELEASE); return NULL; }
    f->fn = fn;
    f->param = param;
    f->base = base;
    f->top = top - HEADROOM;
    f->tib_base = f->top;
    f->tib_limit = low;
    f->tib_dealloc = base;
    /* the first frame, as rbf_switch_asm leaves one: the trampoline as the return address (16-byte
     * aligned + 8, as after a call), then the 8 registers (the fiber in r12), then the vector
     * registers and the control words */
    u64 *sp = (u64 *)f->top;
    *--sp = (u64)(uintptr_t)rbf_trampoline;
    for (int r = 0; r < 8; r++) *--sp = r == 4 ? (u64)(uintptr_t)f : 0;   /* (rbp, rbx, rdi, rsi, r12, r13, r14, r15: as pushed) */
    u8 *frame = (u8 *)sp - 0xA8;
    memset(frame, 0, 0xA8);
    *(u32 *)(frame + 0xA0) = 0x1F80;   /* MXCSR: all exceptions masked, round to nearest */
    *(u16 *)(frame + 0xA4) = 0x027F;   /* x87: all masked, double precision, round to nearest */
    f->sp = frame;
    return f;
}

void rbf_switch(void *to_) {
    RbFiber *to = to_, *me = current;
    if (!me || to == me) return;
    NT_TIB *t = tib();
    me->tib_base = t->StackBase;
    me->tib_limit = t->StackLimit;
    me->tib_dealloc = *tib_dealloc();
    t->StackBase = to->tib_base;
    t->StackLimit = to->tib_limit;
    *tib_dealloc() = to->tib_dealloc;
    current = to;
    InterlockedExchange(&to->running, 1);
    InterlockedExchange(&me->running, 0);
    rbf_switch_asm(&me->sp, to->sp);
    /* (back on this fiber, possibly on another thread: the switcher set that thread's TEB) */
}

void rbf_delete(void *fiber) { (void)fiber; }   /* (kept for a rewind: a test-only mode leaks them) */
void *rbf_data(void) { return current ? current->param : NULL; }
int rbf_is_fiber(void) { return current != NULL; }
int rbf_running(void *fiber) { return ((RbFiber *)fiber)->running != 0; }

int rbf_save(void *fiber, RbfSaved *out) {
    RbFiber *f = fiber;
    memset(out, 0, sizeof *out);
    if (!f->top || f->running) return 0;   /* (a thread's own stack, or running now) */
    size_t bytes = (size_t)(f->top - (u8 *)f->sp);
    u8 *copy = malloc(bytes);
    if (!copy) return 0;
    memcpy(copy, f->sp, bytes);
    *out = (RbfSaved){f, f->sp, f->tib_base, f->tib_limit, f->tib_dealloc, copy, bytes};
    return 1;
}

int rbf_restore(const RbfSaved *s) {
    RbFiber *f = s->fiber;
    if (!f || !s->stack || f->running) return 0;
    /* (the stack may have grown below since: what was committed then still is - pages are not given back) */
    memcpy(s->sp, s->stack, s->bytes);
    f->sp = s->sp;
    f->tib_base = s->tib_base;
    f->tib_limit = s->tib_limit;
    f->tib_dealloc = s->tib_dealloc;
    return 1;
}
#endif
