/* nn::os fibers, natively.
 *
 * On the console a fiber switch is a hand-written routine, DispatchFiberContext(save, load):
 * it pushes x0, x19-x30 and q8-q15 as a 0xF0-byte frame, stores the stack pointer in *save,
 * loads the stack pointer from *load, pops that frame and returns into it. Recompiled code
 * cannot do that - a guest call is a host call, so the host stack holds the real frames -
 * so each guest fiber runs on a Windows fiber with its own host stack and its own guest
 * register context, and this routine is the only part of the SDK's fiber code replaced.
 *
 * *save receives a token naming the suspended context. A load of anything else is a frame
 * prepared by nn::os::InitializeFiber on the new fiber's stack: a host fiber is created that
 * loads that frame exactly as the original would and jumps to the saved x30.
 *
 * A fiber nn::os::FinalizeFiber is done with gives its slot to the next new one: the game's
 * script coroutines come and go by the thousand in a match. Its host fiber is deleted only
 * then - it stays where the guest fiber last stopped, and is never switched to again.
 *
 * Fibers may resume on a different thread from the one they were suspended on, so the
 * thread's identity (kernel thread, thread-local region, TPIDR_EL0) moves with every switch. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "host.h"
#include "net_rbfiber.h"
#include "net_rbtest.h"

/* The host fibers: Windows' own, or - for the rollback rewind test, which must save and restore them -
 * the runtime's own (net_rbfiber.c), decided once, before the first one. Elsewhere the Win32 calls are
 * already the runtime's own (linux/win32.c, macos/fiber.c). */
#ifdef _WIN32
static int own_fibers(void) {
    static volatile int own = -1;
    if (own < 0) own = rbt_on();
    return own;
}
#define FIB_IS() (own_fibers() ? rbf_is_fiber() : IsThreadAFiber())
#define FIB_DATA() (own_fibers() ? rbf_data() : GetFiberData())
#define FIB_CONVERT(p) (own_fibers() ? rbf_convert(p) : ConvertThreadToFiberEx(p, FIBER_FLAG_FLOAT_SWITCH))
#define FIB_CREATE(fn, p) (own_fibers() ? rbf_create(64 << 20, fn, p) : CreateFiberEx(64 << 10, 64 << 20, FIBER_FLAG_FLOAT_SWITCH, fn, p))
#define FIB_SWITCH(f) (own_fibers() ? rbf_switch(f) : SwitchToFiber(f))
#define FIB_DELETE(f) (own_fibers() ? rbf_delete(f) : DeleteFiber(f))
#else
#define FIB_IS() IsThreadAFiber()
#define FIB_DATA() GetFiberData()
#define FIB_CONVERT(p) ConvertThreadToFiberEx(p, FIBER_FLAG_FLOAT_SWITCH)
#define FIB_CREATE(fn, p) CreateFiberEx(64 << 10, 64 << 20, FIBER_FLAG_FLOAT_SWITCH, fn, p)
#define FIB_SWITCH(f) SwitchToFiber(f)
#define FIB_DELETE(f) DeleteFiber(f)
#endif

#define TOKEN_TAG 0xF1BE000000000000ull
#define TOKEN_USE 0x0000FFFF00000000ull   /* how often the slot was used: an old fiber's token is not the new one's */
#define FRAME 0xF0
#define CONTEXT 0x50                      /* (nn::os::FiberType: where the context is kept) */
#define MAX_FIBERS 16384

typedef struct Fib { LPVOID host; Cpu *cpu; u64 token; u64 guest; int finalized; } Fib;

static Fib *fibs[MAX_FIBERS];
static u32 fib_count;
static Fib *spare[MAX_FIBERS];   /* finalized: their slots go to new fibers */
static u32 nspare;
static u64 started;
static SRWLOCK lock = SRWLOCK_INIT;

static Fib *by_token(u64 token) {
    if ((token & 0xFFFF000000000000ull) != TOKEN_TAG) return NULL;
    u64 i = token & 0xFFFFFFFF;
    Fib *f = i < fib_count ? fibs[i] : NULL;
    return f && f->token == token ? f : NULL;
}

static Fib *new_fib(Cpu *cpu) {
    Fib *f = calloc(1, sizeof *f);
    f->cpu = cpu;
    AcquireSRWLockExclusive(&lock);
    if (fib_count == MAX_FIBERS) rt_fatal("too many fibers");
    f->token = TOKEN_TAG | fib_count;
    fibs[fib_count++] = f;
    ReleaseSRWLockExclusive(&lock);
    return f;
}

void fiber_counts(u32 *slots, u32 *finalized, u64 *made) {
    AcquireSRWLockShared(&lock);
    *slots = fib_count;
    *finalized = nspare;
    *made = started;
    ReleaseSRWLockShared(&lock);
}

static void WINAPI fiber_main(void *param) {
    Fib *f = param;
    rt_lookup(f->cpu->x[30])(f->cpu);
    rt_fatal("a fiber's entry function returned");
}

/* nn::os::FinalizeFiber(fiber) - the same in 11.0.0 and 13.0.5. */
/* signature: f81e0ff3 a9017bfd 910043fd 39408408 aa0003f3 34.....8 a9438261 f9402662 9....... f9401e68 3900867f f9002268 3900827f a9417bfd f84207f3 d65f03c0 */
HLE_WRAP(710932f130) {
    u64 guest = c->x[0];
    f_710932f130_orig(c);
    Fib *f = by_token(ld64(guest + CONTEXT));   /* (none if it never ran) */
    if (!f || f->guest != guest) return;
    AcquireSRWLockExclusive(&lock);
    if (!f->finalized) {
        f->finalized = 1;
        spare[nspare++] = f;
    }
    ReleaseSRWLockExclusive(&lock);
}

HLE(_ZN2nn2os6detail20DispatchFiberContextEPPNS1_22FiberContextForHorizonES4_) {
    u64 save = c->x[0], load = c->x[1], target = ld64(load);

    /* The context being left: this host fiber (converting the thread on first use). */
    Fib *me;
    if (FIB_IS() && FIB_DATA()) me = FIB_DATA();
    else {
        me = new_fib(c);
        me->host = FIB_CONVERT(me);
        if (!me->host) rt_fatal("cannot make a fiber of a guest thread (error %lu)", (unsigned long)GetLastError());
    }
    me->cpu = c;
    st64(save, me->token);

    Fib *next = by_token(target);
    if (!next) {   /* a fresh fiber: the frame InitializeFiber prepared at `target` - in a finalized one's slot if there is one */
        AcquireSRWLockExclusive(&lock);
        next = nspare && !own_fibers() ? spare[--nspare] : NULL;   /* (the rewind test: never a finalized one's slot - a rewind would
                                                                     * put back a slot a fiber of the window has since taken) */
        if (next == me) next = NULL;   /* (finalized while it runs: a guest mistake - never deleted under itself) */
        if (next) {
            next->token = TOKEN_TAG | ((next->token + (1ull << 32)) & TOKEN_USE) | (next->token & 0xFFFFFFFF);
            next->finalized = 0;
        }
        started++;
        ReleaseSRWLockExclusive(&lock);
        Cpu *n;
        if (next) {
            FIB_DELETE(next->host);
            n = next->cpu;
        } else {
            n = _aligned_malloc(sizeof *n, 64);
            next = new_fib(n);
        }
        memset(n, 0, sizeof *n);
        n->x[0] = ld64(target);
        for (int r = 19; r <= 30; r++) n->x[r] = ld64(target + 8ull * (r - 18));
        for (int q = 8; q <= 15; q++) memcpy(&n->v[q], P(void, target + 0x70 + 16ull * (q - 8)), 16);
        n->sp = target + FRAME;
        next->guest = load - CONTEXT;
        next->host = FIB_CREATE(fiber_main, next);
        if (!next->host) rt_fatal("cannot create a host fiber (error %lu)", (unsigned long)GetLastError());
    } else {
        next->cpu->x[0] = 0;   /* what the frame of a suspended context holds in x0 */
    }
    Cpu *n = next->cpu;
    n->thread = c->thread; n->tpidrro = c->tpidrro; n->tpidr = c->tpidr; n->fpcr = c->fpcr; n->fpsr = c->fpsr;
    if (own_fibers()) kernel_set_fiber(c->thread, next->token, next->cpu);   /* (the rewind test: which fiber each thread is on) */
    FIB_SWITCH(next->host);
    /* Resumed - possibly on another thread, whose identity the switcher handed us. */
}

/* ---- the rollback rewind test (net_rbtest.c): every fiber saved and put back ----------
 * With the runtime's own fibers only (Windows: net_rbfiber.c, chosen for the test): the slots, each
 * fiber's guest context and the part of its host stack in use - in one of FIBER_SAVES saves, each
 * one put back as often as asked. A fiber running then (the thread saving, or one on another thread)
 * is left as it is. Fibers made after the save are dropped from the slots (their host fibers are
 * kept: a test-only leak). Saves 2 and up are the per-frame ring's (SSBU_RB_EVERY): a finalized fiber
 * there keeps only its fields - it never runs again, and its slot is never reused while testing. */
#define FIBER_SAVES (2 + 16)
#ifdef _WIN32
typedef struct SavedFib { Fib *f; Fib fields; Cpu cpu; int has_cpu; RbfSaved host; } SavedFib;
typedef struct FiberSave { SavedFib *fib; u32 n, nspare; Fib **spare; } FiberSave;
static FiberSave saves[FIBER_SAVES];

int fibers_save(int which, u32 *running) {
    if (!own_fibers() || which < 0 || which >= FIBER_SAVES) return -1;
    FiberSave *v = &saves[which];
    AcquireSRWLockExclusive(&lock);
    for (u32 i = 0; i < v->n; i++) free(v->fib[i].host.stack);
    v->n = 0;
    if (!v->fib) v->fib = calloc(MAX_FIBERS, sizeof *v->fib);
    if (!v->spare) v->spare = calloc(MAX_FIBERS, sizeof *v->spare);
    if (!v->fib || !v->spare) { ReleaseSRWLockExclusive(&lock); return -1; }
    *running = 0;
    for (u32 i = 0; i < fib_count; i++) {
        SavedFib *k = &v->fib[i];
        k->f = fibs[i];
        k->fields = *fibs[i];
        if (which >= 2 && fibs[i]->finalized) { k->has_cpu = 0; memset(&k->host, 0, sizeof k->host); continue; }
        k->has_cpu = fibs[i]->cpu && fibs[i]->host && !rbf_running(fibs[i]->host);
        if (k->has_cpu) k->cpu = *fibs[i]->cpu;
        if (!fibs[i]->host || !rbf_save(fibs[i]->host, &k->host)) memset(&k->host, 0, sizeof k->host);
        if (fibs[i]->host && rbf_running(fibs[i]->host)) (*running)++;
    }
    v->n = fib_count;
    v->nspare = nspare;
    memcpy(v->spare, spare, nspare * sizeof *spare);
    ReleaseSRWLockExclusive(&lock);
    return (int)v->n;
}

/* (the rewind test) every fiber's state as numbers: per fiber, a hash of its slot's fields, of its guest context and of its host
 * stack in use (out[3 * i ...]); how many fibers. A fiber running now: its context and stack as 0 (they are the thread's). */
u32 fibers_hash(u64 *out, u32 max) {
    if (!own_fibers()) return 0;
    AcquireSRWLockShared(&lock);
    u32 n = fib_count < max ? fib_count : max;
    for (u32 i = 0; i < n; i++) {
        Fib *f = fibs[i];
        u64 h = 0xCBF29CE484222325ull;
        h = (h ^ (u64)(uintptr_t)f) * 0x100000001B3ull;
        h = (h ^ f->token) * 0x100000001B3ull;
        h = (h ^ f->guest) * 0x100000001B3ull;
        h = (h ^ (u64)f->finalized) * 0x100000001B3ull;
        h = (h ^ (u64)(uintptr_t)f->host) * 0x100000001B3ull;
        out[3 * i] = h;
        int running = f->host && rbf_running(f->host);
        u64 c = 0, s = 0;
        if (!running && f->cpu && !f->finalized) {
            c = 0xCBF29CE484222325ull;
            for (int r = 0; r < 31; r++) c = (c ^ f->cpu->x[r]) * 0x100000001B3ull;
            c = (c ^ f->cpu->sp) * 0x100000001B3ull;
        }
        if (!running && f->host && !f->finalized) s = rbf_hash(f->host);
        out[3 * i + 1] = c;
        out[3 * i + 2] = s;
    }
    ReleaseSRWLockShared(&lock);
    return n;
}

int fibers_restore(int which, u32 *skipped, u32 *dropped) {
    *skipped = *dropped = 0;
    if (!own_fibers() || which < 0 || which >= FIBER_SAVES || !saves[which].fib) return -1;
    FiberSave *v = &saves[which];
    AcquireSRWLockExclusive(&lock);
    if (fib_count > v->n) *dropped = fib_count - v->n;
    int restored = 0;
    for (u32 i = 0; i < v->n; i++) {
        SavedFib *k = &v->fib[i];
        fibs[i] = k->f;
        if (k->f->host && rbf_running(k->f->host)) { (*skipped)++; continue; }   /* (running now: left as it is) */
        *k->f = k->fields;
        if (k->has_cpu && k->f->cpu) *k->f->cpu = k->cpu;
        if (k->host.fiber && rbf_restore(&k->host)) restored++;
    }
    fib_count = v->n;
    nspare = v->nspare;
    memcpy(spare, v->spare, nspare * sizeof *spare);
    ReleaseSRWLockExclusive(&lock);
    return restored;
}
#else
int fibers_save(int which, u32 *running) { (void)which; *running = 0; return -1; }
int fibers_restore(int which, u32 *skipped, u32 *dropped) { (void)which; *skipped = *dropped = 0; return -1; }
u32 fibers_hash(u64 *out, u32 max) { (void)out; (void)max; return 0; }
#endif
