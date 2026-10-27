/* Rollback rewind test: can the game's simulation be saved, rewound and replayed to the same state?
 * (docs/ROLLBACK.md, phase 3: the gate the whole rollback project depends on.)
 *
 * Off unless SSBU_RB_TEST=1 or a file rollback_test.on exists beside the game. On, from
 * SSBU_RB_TEST_AT seconds after start (default 200: in a battle, on a slow PC with the scripted
 * battle of tools/dev/autobattle.sh), it runs five rounds, ten seconds apart, of N = 1, 1, 1, 5 and 10
 * frames:
 *
 *   1. Find the pages: for 120 frames, the alias pages the game writes (the alias area holds the
 *      game's own memory: MapPhysicalMemory) - the "hot set", asked through the renderer's write
 *      watch stamps (gfx_watch_pages, as the rollback probe does). It grows from round to round:
 *      the pages earlier rounds found, and those their saves missed, stay in it.
 *   2. Save: at the start of a frame's simulation, a copy of every hot page.
 *   3. Record N frames: the game's pad reads and system ticks on its main thread, and the guest
 *      context each frame's input step and simulation are called with.
 *   4. Check: at the start of frame N's simulation, the hash of every hot page (the state the game
 *      reached), and how many pages outside the hot set were written meanwhile (the save missed them).
 *      While recording, the pages written while each frame's simulation ran are marked: a difference
 *      there is the simulation's; elsewhere, the rest of the frame's (the drawing, which a replay skips).
 *   5. Keep the state now, then rewind and replay twice: copy the saved pages back, run the recorded
 *      frames again - the simulation, then the next input step, ... - with the recorded pads and
 *      ticks, without drawing; and again from the same save.
 *   6. Compare: replay 1 with replay 2 - equal, the simulation is repeatable from a save, which is
 *      what a rollback needs - and replay 1 with the game's own state; where the replays differ, the
 *      first words that do. Then give the game back the
 *      state it had (step 5), so it carries on as if the test had not been.
 *
 * Before the first save, a census (Windows): for 10 frames, which threads write each hot page, inside
 * and outside the simulation's span. A thread other than the main one that writes outside it goes at
 * its own pace (sound, effects...), and every page it writes is left out of the rewinds from the start.
 *
 * From the second round on, the pages the last round's replays disagreed on are made read-only while
 * replaying (from the put-back to the hashing) and in the game's next frame, to see which threads write
 * them: a page another thread writes is one a rewind cannot undo, and is left out of the rewinds and
 * the comparisons from then on (counted, with how many of them the replaying thread writes too).
 *
 * It saves the game's fibers too (phase 3b): while the test is on, hle_fiber.c runs them on fibers of
 * the runtime's own (net_rbfiber.c) instead of Windows', so each one's slot, guest context and host
 * stack can be saved with the pages and put back with them (fibers_save, fibers_restore). A fiber
 * running at the save (the one the frame's simulation runs on, if any) cannot be, and is counted.
 *
 * What it does not save: the kernel's objects, the renderer's caches, pages outside the hot set -
 * nor what other threads (the game's workers) do. Those are exactly what the test finds out about:
 * a difference between the two replays, or a crash while replaying, says the save was not enough.
 * Every line it writes to the log starts with [rbtest].
 *
 * The frame's steps are the game's own functions, found with the rollback probe and the profiler
 * (docs/ROLLBACK.md): 7103667300 reads the pads, 71037246a0 simulates (and calls itself: only the
 * outermost call, from the main loop, is a frame), both called by the main loop 7103746e90. */
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_RAND_S   /* (rand_s, for the secure random bytes recorded) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_rbtest.h"
#include "net_lockstep.h"

volatile int rbt_state = -1;

GuestFn f_7103667300_orig, f_71037246a0_orig;   /* (the game's own input step and simulation: HLE_WRAP below) */
extern GuestFn f_710375e690_orig, f_71035493e0_orig, f_710354c990_orig;   /* (the main loop's steps after the input step: below) */
#define tail_fn(which) ((which) == 0 ? f_710375e690_orig : (which) == 1 ? f_71035493e0_orig : f_710354c990_orig)

/* ---- the game's allocators (rollback's next method: save the combat's memory, not pages by thread) ----
 * The game takes its memory through nn::mem::StandardAllocator, each one over a region of its own. Every one the
 * game makes is noted here (and in the log while the test is on); the census then says, per region, how many of
 * its pages only the simulation wrote and how many another thread wrote too. A region the simulation alone writes
 * can be saved and put back whole. */
#define MAX_ALLOCS 128
static struct { u64 self, base, size, caller; } allocs[MAX_ALLOCS];
static volatile LONG nallocs;
/* (the sdk's nn::mem::StandardAllocator::Initialize(void *, size_t, bool); the two-argument one ends in it) */
/* signature: f81d0ff5 a9014ff4 a9027bfd 910083fd 39400009 35.....9 8b020029 913ffc2a aa0103e8 9274cd41 9274cd29 2a0303f4 aa0203f5 aa0003f3 eb010122 b4.....8 */
HLE_WRAP(71092e7600) {
    u64 self = c->x[0], base = c->x[1], size = c->x[2], caller = c->x[30];
    f_71092e7600_orig(c);
    LONG i = InterlockedIncrement(&nallocs) - 1;
    if (i < MAX_ALLOCS) { allocs[i].self = self; allocs[i].base = base; allocs[i].size = size; allocs[i].caller = caller; }
    if (rbt_on()) fprintf(stderr, "[rbtest] allocator %ld: %#llx, %#llx-%#llx (%.1f MB), made from main+%#llx\n", (long)i, (unsigned long long)self,
                          (unsigned long long)base, (unsigned long long)(base + size), size / 1048576.0, (unsigned long long)(caller - 0x7100000000ull));
}
static int alloc_of(u64 page) {
    LONG n = nallocs < MAX_ALLOCS ? nallocs : MAX_ALLOCS;
    for (LONG i = 0; i < n; i++) if (page >= allocs[i].base && page < allocs[i].base + allocs[i].size) return (int)i;
    return -1;
}

#ifdef __APPLE__   /* (no Direct3D renderer there to share the alias area's watch with) */
static long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user) {
    (void)base; (void)size; (void)n; (void)seen; (void)each; (void)user;
    return -2;
}
#else
long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user);   /* (gpu_d3d11.c) */
#endif

#define PAGE 0x1000ull
#define MAX_HOT 32768              /* pages saved at most: 128 MB (a heavy stage writes more than 64 MB in 120 frames) */
#define MAX_REGIONS 4096
#define DISCOVER_FRAMES 120
#define CENSUS_FRAMES 10           /* (Windows) frames the census of who writes the pages lasts */
#define MAX_LATE 8192              /* pages found written outside the hot set, added to it for the next round */
#define MAX_N 60
#define MAX_PADS 16                /* pad reads kept per frame */
#define PAD_BYTES 0x34
#define MAX_TICKS 1024             /* tick reads kept per frame */
#define ROUND_GAP_NS 10000000000ull

static u32 round_frames[] = {1, 1, 1, 5, 10};   /* (SSBU_RB_ROUNDS="1,10,30,60": others, at most 60 frames and 5 rounds) */
#define ROUNDS (sizeof round_frames / sizeof *round_frames)

typedef struct RecFrame {
    Cpu input_cpu, sim_cpu;        /* the contexts the input step and the simulation were called with */
    int have_input;
    u8 pad[MAX_PADS][PAD_BYTES];
    u32 pad_size[MAX_PADS], npad;
    u64 ticks[MAX_TICKS];
    u32 nticks;
    u8 rnd[4096];                  /* the secure random bytes asked for, by any thread, in order */
    u32 ntail;                     /* (ring) the main loop's steps after the input step, in order, and their contexts */
    u8 tail_which[4];
    Cpu tail_cpu[4];
    u32 nrnd;
} RecFrame;

enum { IDLE, DISCOVER, CENSUS, RECORD, DONE, RING };
enum { HOOK_OFF, HOOK_RECORD, HOOK_PLAY };

static int phase = IDLE, hook = HOOK_OFF;
static u32 round_i, frames_in_phase, n_frames, cur, rec_next, pad_cursor, tick_cursor;
static u64 next_round_ns, discover_seen, window_seen;
static DWORD sim_thread;
static __declspec(thread) int depth;   /* the simulation calls itself: only the outermost call is a frame */
static RecFrame rec[MAX_N + 1];
static u8 *hot_bits;                   /* one bit an alias page */
static u64 hot[MAX_HOT];
static u32 nhot, hot_overflow;
static u8 *snap;                       /* the saved pages, in hot[] order */
static u8 snap_ok[MAX_HOT];            /* (a page not committed then was not saved) */
static u8 *snap_now;                   /* the game's state at the end of the window, given back after the test */
static u64 sum_real[MAX_HOT], sum_one[MAX_HOT], sum_two[MAX_HOT], sum_three[MAX_HOT];
static u8 *sim_bits;                   /* one bit an alias page: written while the simulation ran, in the window */
static u64 sim_seen;
static u64 late[MAX_LATE];             /* (pages a round's save missed: the next round's hot set has them) */
static u32 nlate;
static u8 *foreign;                    /* one bit an alias page: another thread was seen writing it - left out of the rewinds and the comparisons */
static u32 nforeign, nshared;          /* (shared: the replaying thread writes them too - the simulation's data there is not rewound either) */
static u8 *snap_one;                   /* replay 1's pages, to show how they differ from replay 2's */
static u64 region_base[MAX_REGIONS], region_size[MAX_REGIONS], alias_base[MAX_REGIONS], alias_size[MAX_REGIONS];
static u32 nregions, nalias;
static u64 escaped, pad_replayed, pad_missing, tick_replayed, tick_missing, fibers_at_save;

int rbt_probe(void) {
    static int once;
    if (!__atomic_exchange_n(&once, 1, __ATOMIC_ACQ_REL)) {
        const char *env = getenv("SSBU_RB_TEST");
        int on = env && *env ? atoi(env) != 0 : 0;
        if (!on) {
            FILE *f = fopen("rollback_test.on", "r");
            if (f) { fclose(f); on = 1; }
        }
        const char *at = getenv("SSBU_RB_TEST_AT");
        next_round_ns = (u64)((at && *at ? atof(at) : 200.0) * 1e9);
        if (on) {
            fprintf(stderr, "[rbtest] on: rewind test rounds of");
            for (u32 i = 0; i < ROUNDS; i++) fprintf(stderr, " %u", round_frames[i]);
            fprintf(stderr, " frames, the first %.0f s after start\n", next_round_ns / 1e9);
        }
        __atomic_store_n(&rbt_state, on, __ATOMIC_RELEASE);
    }
    return __atomic_load_n(&rbt_state, __ATOMIC_ACQUIRE) > 0;
}

/* ---- memory --------------------------------------------------------------------------- */
static void read_regions(void) {   /* the guest's committed memory now (the kernel's lock busy: the last list) */
    u32 n = kernel_memory_regions(region_base, region_size, MAX_REGIONS);
    if (!n) return;
    nregions = n;
    nalias = 0;
    for (u32 i = 0; i < nregions; i++)
        if (region_base[i] >= ALIAS_BASE && region_base[i] + region_size[i] <= ALIAS_BASE + ALIAS_SIZE) {
            alias_base[nalias] = region_base[i]; alias_size[nalias] = region_size[i]; nalias++;
        }
}

static int committed(u64 page) {
    for (u32 i = 0; i < nalias; i++)
        if (page >= alias_base[i] && page + PAGE <= alias_base[i] + alias_size[i]) return 1;
    return 0;
}

static int is_hot(u64 page) {
    u64 i = (page - ALIAS_BASE) / PAGE;
    return page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE && hot_bits[i >> 3] >> (i & 7) & 1;
}

static void mark_hot(u64 page, void *user) {
    (void)user;
    if (is_hot(page)) return;
    if (nhot == MAX_HOT) { hot_overflow++; return; }
    u64 i = (page - ALIAS_BASE) / PAGE;
    hot_bits[i >> 3] |= (u8)(1u << (i & 7));
    hot[nhot++] = page;
}

static int is_sim(u64 page) {
    u64 i = (page - ALIAS_BASE) / PAGE;
    return page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE && sim_bits[i >> 3] >> (i & 7) & 1;
}

static void mark_sim(u64 page, void *user) {
    (void)user;
    if (page < ALIAS_BASE || page >= ALIAS_BASE + ALIAS_SIZE) return;
    u64 i = (page - ALIAS_BASE) / PAGE;
    sim_bits[i >> 3] |= (u8)(1u << (i & 7));
}

/* SSBU_RB_FREEZE=1: rewind everything, as an emulator does - every other guest thread held in its wait while the pages
 * are saved and put back (kernel_freeze), and no page left out for another thread writing it (no census). The threads
 * must be idle at the same waits at the put-back as at the save. */
static int freeze_mode = -1;
static u64 freeze_keys[256];
static u32 nfreeze_keys;
static u64 freeze_waited_ns, freeze_tries;
/* (freeze mode) the threads' thread-local regions too (nn::os's TLS slots are there: a per-thread value the game flips every
 * frame was not rewound), saved and put back with the pages */
#define TLR_MAX (1024 * 0x1000)
static u8 *tlr_snap, *tlr_now;
static u64 tlr_size;
static void tlr_copy(u8 *to, int out) {   /* out: from the game into `to`; else from `to` back into the game */
    if (out) { tlr_size = kernel_tlr_end() - KERNEL_BASE; if (tlr_size > TLR_MAX) tlr_size = TLR_MAX; memcpy(to, P(void, KERNEL_BASE), tlr_size); }
    else memcpy(P(void, KERNEL_BASE), to, tlr_size);
}
static int freeze_on(void) {
    if (freeze_mode < 0) freeze_mode = getenv("SSBU_RB_FREEZE") && atoi(getenv("SSBU_RB_FREEZE")) != 0;
    return freeze_mode;
}
/* Every other guest thread held (1), within ms; match: at the waits of the save. 0: not in time (nothing held). */
static int freeze_all(int match, u32 ms, u64 *busy) {
    u64 t0 = rt_now_ns();
    freeze_tries = 0;
    do {
        freeze_tries++;
        if (kernel_freeze(freeze_keys, &nfreeze_keys, 256, match, busy)) { freeze_waited_ns = rt_now_ns() - t0; return 1; }
        SwitchToThread();
    } while (rt_now_ns() - t0 < (u64)ms * 1000000);
    freeze_waited_ns = rt_now_ns() - t0;
    return 0;
}

/* While replaying (freeze mode), every guest thread but the replaying one and the simulation's helpers (SSBU_RB_HELPERS,
 * guest ids; 3,4,12,13: the job workers and the two helpers the census found) stays in its wait: a thread at its own pace
 * (sound, effects) running meanwhile changes pages the replays then disagree on. A watchdog lets them go after 3 s (the
 * simulation waiting for one of them would otherwise never end). */
static volatile LONG hold_wanted, holding, held_count;
static volatile u64 hold_since_ns;
static u64 helper_ids[16];
static u32 nhelpers;
static DWORD WINAPI hold_watchdog(void *unused) {
    (void)unused;
    for (;;) {
        Sleep(100);
        if (holding && rt_now_ns() - hold_since_ns > 3000000000ull) {
            fprintf(stderr, "[rbtest] the held threads let go after 3 s: the simulation waited for one of them; the threads then:\n");
            kernel_dump_threads();
            InterlockedExchange(&holding, 0);
            kernel_release();
        }
    }
    return 0;
}
static void hold_others(void) {   /* (frozen) */
    if (!nhelpers) {
        const char *h = getenv("SSBU_RB_HELPERS");
        const char *s = h && *h ? h : "3,4,12,13";
        while (*s && nhelpers < 16) { helper_ids[nhelpers++] = strtoull(s, (char **)&s, 10); while (*s == ',' || *s == ' ') s++; }
        CloseHandle(CreateThread(NULL, 0, hold_watchdog, NULL, 0, NULL));
    }
    held_count = (LONG)kernel_hold(helper_ids, nhelpers);
    hold_since_ns = rt_now_ns();
    InterlockedExchange(&holding, 1);
}

static int is_foreign(u64 page) {
    u64 i = (page - ALIAS_BASE) / PAGE;
    return page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE && foreign[i >> 3] >> (i & 7) & 1;
}

static void mark_foreign(u64 page) {
    if (freeze_on()) return;   /* (everything is rewound) */
    if (page < ALIAS_BASE || page >= ALIAS_BASE + ALIAS_SIZE || is_foreign(page)) return;
    u64 i = (page - ALIAS_BASE) / PAGE;
    foreign[i >> 3] |= (u8)(1u << (i & 7));
    nforeign++;
}

static void count_escaped(u64 page, void *user) {
    (void)user;
    if (is_hot(page)) return;
    escaped++;
    if (nlate < MAX_LATE) late[nlate++] = page;
}

static u64 page_hash(const u8 *p) {
    u64 h = 0x9E3779B97F4A7C15ull;
    for (u32 i = 0; i < PAGE; i += 8) {
        u64 w;
        memcpy(&w, p + i, 8);
        h = (h ^ w) * 0xFF51AFD7ED558CCDull;
        h ^= h >> 32;
    }
    return h;
}

/* The renderer's lock may be busy: ask again for a moment, for a question that cannot wait a frame. */
static long long watch_now(u64 *seen, void (*each)(u64 page, void *user)) {
    for (int tries = 0; tries < 200; tries++) {
        long long n = gfx_watch_pages(alias_base, alias_size, nalias, seen, each, NULL);
        if (n != -1) return n;
        Sleep(1);
    }
    return -1;
}

/* ---- the hooks ------------------------------------------------------------------------ */
static int on_sim_thread(void) { return sim_thread && GetCurrentThreadId() == sim_thread; }

/* (freeze mode) replay 2 with other buttons than the recorded ones - a wrong guess of the other player's pad, as rollback makes:
 * every pad read gets A held and the left stick full left. Replay 3 then goes back to the recorded pads, and must come out as
 * replay 1 did: a rewind after a wrong guess puts the game where the right pads take it. */
static volatile int pad_alter;
static int ring_natural;   /* (saving every frame, below) */
static u64 plan_at;
static u64 pads_altered;
void rbt_pad(u64 out, u32 size) {
    if (hook == HOOK_OFF || !on_sim_thread() || size > PAD_BYTES) return;
    RecFrame *f = &rec[cur];
    if (hook == HOOK_RECORD) {
        if (f->npad < MAX_PADS) { memcpy(f->pad[f->npad], P(void, out), size); f->pad_size[f->npad++] = size; }
        if (ring_natural && plan_at && size >= 0x18) {   /* (the frames before a rollback: played live with a wrong guess) */
            st64(out + 8, ld64(out + 8) | 1);
            st32(out + 0x10, (u32)-32767);
            pads_altered++;
        }
    } else if (pad_cursor < f->npad && f->pad_size[pad_cursor] == size) {
        memcpy(P(void, out), f->pad[pad_cursor++], size);
        pad_replayed++;
        if (pad_alter && size >= 0x18) {   /* (nn::hid npad state: buttons at +8, the left stick's x and y at +0x10) */
            st64(out + 8, ld64(out + 8) | 1);
            st32(out + 0x10, (u32)-32767);
            pads_altered++;
        }
    } else pad_missing++;
}

static volatile u64 tick_last, helper_ticks;   /* (the last tick the replaying thread was given; how often the others asked meanwhile) */
u64 rbt_ticks(u64 now) {
    if (hook == HOOK_OFF) return now;
    if (!on_sim_thread()) {   /* (freeze mode, replaying: the helpers see the replayed clock, not the real one) */
        if (hook == HOOK_PLAY && freeze_on() && tick_last) { helper_ticks++; return tick_last; }
        return now;
    }
    RecFrame *f = &rec[cur];
    if (hook == HOOK_RECORD) {
        if (f->nticks < MAX_TICKS) f->ticks[f->nticks++] = now;
        return tick_last = now;
    }
    if (tick_cursor < f->nticks) { tick_replayed++; return tick_last = f->ticks[tick_cursor++]; }
    tick_missing++;
    return now;
}

/* (freeze mode) every call of 710366f660 - it advances the combat's random generator - while replaying: who called it, on
 * which thread, so that two replays' lists show the call one of them made and the other did not */
#define MAX_RNG_CALLS 4096
static struct { u64 caller, caller2; u32 thread; } rng_calls[3][MAX_RNG_CALLS];
static volatile LONG nrng_calls[3];
static volatile int rng_replay;   /* 1..3 while that replay runs, else 0 */
GuestFn f_710366f660_orig;
/* signature: d102c3ff 6d0423e9 a9056ffc a90667fa a9075ff8 a90857f6 a9094ff4 a90a7bfd 910283fd a9452408 cb080129 b202e7ea f29999aa 9345fd29 9b0a7d29 2a0103f6 */
HLE_WRAP(710366f660) {
    int r = rng_replay;
    if (r >= 1 && r <= 3) {
        LONG i = InterlockedIncrement(&nrng_calls[r - 1]) - 1;
        if (i < MAX_RNG_CALLS) {
            rng_calls[r - 1][i].caller = c->x[30] - 0x7100000000ull;
            rng_calls[r - 1][i].caller2 = c->x[29] ? ld64(c->x[29] + 8) - 0x7100000000ull : 0;   /* (the caller's caller, by the frame record) */
            rng_calls[r - 1][i].thread = GetCurrentThreadId();
        }
    }
    f_710366f660_orig(c);
}
static void rng_compare(int a, int b) {   /* the first call where replays a and b part */
    LONG na = nrng_calls[a - 1] < MAX_RNG_CALLS ? nrng_calls[a - 1] : MAX_RNG_CALLS, nb = nrng_calls[b - 1] < MAX_RNG_CALLS ? nrng_calls[b - 1] : MAX_RNG_CALLS;
    LONG i = 0;
    while (i < na && i < nb && rng_calls[a - 1][i].caller == rng_calls[b - 1][i].caller && rng_calls[a - 1][i].caller2 == rng_calls[b - 1][i].caller2) i++;
    fprintf(stderr, "[rbtest] random draws: replay %d made %ld, replay %d made %ld; the same up to draw %ld", a, (long)nrng_calls[a - 1], b, (long)nrng_calls[b - 1], (long)i);
    for (int k = 0; k < 2; k++) {
        int w = k ? b : a;
        LONG n = k ? nb : na;
        if (i < n) fprintf(stderr, "; replay %d's next: from main+%#llx (in main+%#llx), thread %lu", w, (unsigned long long)rng_calls[w - 1][i].caller,
                           (unsigned long long)rng_calls[w - 1][i].caller2, (unsigned long)rng_calls[w - 1][i].thread);
    }
    fprintf(stderr, "\n");
}

static u32 rnd_cursor;
static u64 rnd_replayed, rnd_missing, rnd_recorded;
static SRWLOCK rnd_lock = SRWLOCK_INIT;
int rbt_random(u8 *out, u64 n) {
    if (hook == HOOK_OFF) return 0;
    AcquireSRWLockExclusive(&rnd_lock);
    RecFrame *f = &rec[cur];
    int done = 0;
    if (hook == HOOK_RECORD) {
        for (u64 i = 0; i < n; i++) { unsigned int r; rand_s(&r); out[i] = (u8)r; if (f->nrnd < sizeof f->rnd) f->rnd[f->nrnd++] = (u8)r; }
        rnd_recorded += n;
        done = 1;
    } else if (rnd_cursor + n <= f->nrnd) {
        memcpy(out, f->rnd + rnd_cursor, n);
        rnd_cursor += (u32)n;
        rnd_replayed += n;
        done = 1;
    } else rnd_missing += n;
    ReleaseSRWLockExclusive(&rnd_lock);
    return done;
}

static void start_frame(u32 k) {   /* the hooks now record into (or replay from) frame k */
    cur = k;
    pad_cursor = tick_cursor = rnd_cursor = 0;
}

/* ---- the rounds ----------------------------------------------------------------------- */
static void read_rounds(void) {
    static int rounds_read;
    if (!rounds_read++ && getenv("SSBU_RB_ROUNDS")) {
        const char *s = getenv("SSBU_RB_ROUNDS");
        for (u32 i = 0; i < ROUNDS && *s; i++) {
            u32 n = (u32)strtoul(s, (char **)&s, 10);
            round_frames[i] = n < 1 ? 1 : n > MAX_N ? MAX_N : n;
            while (*s == ',' || *s == ' ') s++;
        }
    }
}

static void begin_round(void) {
    if (!hot_bits) hot_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!snap) snap = malloc(MAX_HOT * PAGE);
    if (!snap_now) snap_now = malloc(MAX_HOT * PAGE);
    if (!snap_one) snap_one = malloc(MAX_HOT * PAGE);
    if (!foreign) foreign = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!sim_bits) sim_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!hot_bits || !snap || !snap_now || !snap_one || !sim_bits || !foreign) { fprintf(stderr, "[rbtest] out of memory: off\n"); phase = DONE; return; }
    /* the hot set grows from round to round: what the last rounds found, and what their saves missed */
    for (u32 i = 0; i < nlate; i++) mark_hot(late[i], NULL);
    nlate = 0;
    read_regions();
    discover_seen = 0;
    if (watch_now(&discover_seen, NULL) < 0) return;   /* (only to bring the clock to now; tried again next frame) */
    read_rounds();
    n_frames = round_frames[round_i];
    frames_in_phase = 0;
    phase = DISCOVER;
    fprintf(stderr, "[rbtest] round %u: finding the pages the game writes (%u frames)\n", round_i + 1, DISCOVER_FRAMES);
    fflush(stderr);
}

static void cow_begin(void), cow_keep_now(void), cow_put_back(int to_now), cow_end(void);   /* (copy on write: below) */

static void save(Cpu *c) {
    fprintf(stderr, "[rbtest] round %u: saving\n", round_i + 1);
    fflush(stderr);
    u64 t0 = rt_now_ns();
    read_regions();
    u32 saved = 0;
    u64 busy = 0;
    int frozen = freeze_on() && freeze_all(0, 500, &busy);
    if (freeze_on() && !frozen) fprintf(stderr, "[rbtest] round %u: the other threads could not all be held at once in 500 ms (%llu tries; guest thread %llu busy): saved anyway\n",
                                        round_i + 1, (unsigned long long)freeze_tries, (unsigned long long)busy);
    u64 frozen_ns = freeze_waited_ns, frozen_tries = freeze_tries;
    for (u32 i = 0; i < nhot; i++) {
        snap_ok[i] = (u8)committed(hot[i]);
        if (snap_ok[i]) { memcpy(snap + (u64)i * PAGE, P(void, hot[i]), PAGE); saved++; }
    }
    if (freeze_on()) { if (!tlr_snap) tlr_snap = malloc(TLR_MAX); if (!tlr_now) tlr_now = malloc(TLR_MAX); if (tlr_snap) tlr_copy(tlr_snap, 1); cow_begin(); }
    if (frozen) {
        kernel_thaw();
        fprintf(stderr, "[rbtest] round %u: saved with the other %u threads held (held after %.1f ms, %llu tries)\n", round_i + 1, nfreeze_keys,
                frozen_ns / 1e6, (unsigned long long)frozen_tries);
    }
    window_seen = 0;
    watch_now(&window_seen, NULL);   /* (the window's writes are counted from here) */
    memset(sim_bits, 0, ALIAS_SIZE / PAGE / 8);
    u32 slots, finalized;
    fiber_counts(&slots, &finalized, &fibers_at_save);
    u32 fib_running = 0;
    int fib_saved = fibers_save(0, &fib_running);
    memset(rec, 0, sizeof rec);
    escaped = pad_replayed = pad_missing = tick_replayed = tick_missing = 0;
    rec[0].sim_cpu = *c;
    rec_next = 0;
    start_frame(0);
    hook = HOOK_RECORD;
    phase = RECORD;
    fprintf(stderr, "[rbtest] round %u (%u frames): %u pages found written in %u frames (%.1f MB%s), %u saved; fibers saved %d (running, not saved: %u); in %.1f ms\n",
            round_i + 1, n_frames, nhot, DISCOVER_FRAMES, nhot * (double)PAGE / 1048576.0, hot_overflow ? ", more than the most kept" : "", saved,
            fib_saved, fib_running, (rt_now_ns() - t0) / 1e6);
}

/* Copy saved pages back: the save at the start of the window (snap) or the game's state at its end (snap_now). */
static void put_back(const u8 *from) {
    u64 busy = 0;
    int frozen = freeze_on() && freeze_all(1, 1000, &busy);
    if (freeze_on() && !frozen) {   /* (at other waits, or one busy: held as they are, if that can be) */
        fprintf(stderr, "[rbtest] put-back: the other threads not idle at the save's waits in 1 s (guest thread %llu): held where they are\n", (unsigned long long)busy);
        u32 n = 0;
        static u64 keys[256];
        u64 t0 = rt_now_ns();
        while (!(frozen = kernel_freeze(keys, &n, 256, 0, &busy)) && rt_now_ns() - t0 < 1000000000ull) SwitchToThread();
        if (!frozen) fprintf(stderr, "[rbtest] put-back: not held at all (guest thread %llu busy): put back anyway\n", (unsigned long long)busy);
    }
    for (u32 i = 0; i < nhot; i++)
        if (snap_ok[i] && committed(hot[i]) && !is_foreign(hot[i])) memcpy(P(void, hot[i]), from + (u64)i * PAGE, PAGE);
    if (freeze_on() && tlr_snap && tlr_now) tlr_copy(from == snap ? tlr_snap : tlr_now, 0);
    if (freeze_on()) cow_put_back(from == snap_now);
    if (frozen && hold_wanted && !holding) hold_others();
    if (frozen) kernel_thaw();
}

/* The recorded frames again, from the start of the window: the simulation of frame 0, then each next
 * frame's input step and simulation, then frame N's input step - back to where the game was when it
 * called this frame's simulation. The drawing and the other steps of the main loop are not run. */
static double replay(Cpu *c) {
    u64 t0 = rt_now_ns();
    Cpu now = *c;
    hook = HOOK_PLAY;
    for (u32 k = 0; k < n_frames; k++) {
        if (k && rec[k].have_input) {   /* (a frame the game simulated without reading the pads first: none to replay) */
            start_frame(k);
            *c = rec[k].input_cpu;
            f_7103667300_orig(c);
        } else if (k) start_frame(k);
        else start_frame(0);   /* (a frame's list holds its input step's reads, then its simulation's: go on from there) */
        *c = rec[k].sim_cpu;
        f_71037246a0_orig(c);
    }
    start_frame(n_frames);
    if (rec[n_frames].have_input) {
        *c = rec[n_frames].input_cpu;
        f_7103667300_orig(c);
    } else fprintf(stderr, "[rbtest] replay: frame %u had no pad read before its simulation: none replayed\n", n_frames);
    hook = HOOK_OFF;
    *c = now;
    return (rt_now_ns() - t0) / 1e6;
}

static void hash_pages(u64 *out) {
    read_regions();
    for (u32 i = 0; i < nhot; i++) out[i] = snap_ok[i] && committed(hot[i]) ? page_hash(P(u8, hot[i])) : 0;
}

/* Pages whose hashes differ, by who wrote them while recording: the simulation (written while it ran on
 * the main thread, which other threads may also have been doing) or only the rest of the frame. */
static int compare(const char *what, const u64 *a, const u64 *b, char *line, size_t room) {
    u32 sim = 0, other = 0, compared = 0, nfirst = 0;
    u64 first[6];
    for (u32 i = 0; i < nhot; i++) {
        if (!a[i] || !b[i] || is_foreign(hot[i])) continue;
        compared++;
        if (a[i] == b[i]) continue;
        if (is_sim(hot[i])) { sim++; if (nfirst < 6) first[nfirst++] = hot[i]; }
        else other++;
    }
    int n = snprintf(line, room, "%s: ", what);
    if (!sim && !other) return n + snprintf(line + n, room - n, "IDENTICAL (%u pages compared)", compared);
    n += snprintf(line + n, room - n, "DIFFERENT in %u of %u pages: %u written by the simulation, %u only by the rest of the frame", sim + other, compared, sim, other);
    if (nfirst) {
        n += snprintf(line + n, room - n, "; the simulation's first:");
        for (u32 i = 0; i < nfirst; i++) n += snprintf(line + n, room - n, " %#llx", (unsigned long long)first[i]);
    }
    return n;
}

/* Where replay 1 and replay 2 differ, in the first pages the simulation wrote: the first words that
 * differ, as offset: replay 1's value / replay 2's (pointers, counters, floats tell different stories).
 * Replay 2's pages are the memory now; replay 1's, the copy in snap_one. */
static void show_words(const u64 *a, const u64 *b) {
    u32 shown = 0;
    for (u32 i = 0; i < nhot && shown < 6; i++) {
        if (!a[i] || !b[i] || a[i] == b[i] || !is_sim(hot[i]) || is_foreign(hot[i])) continue;
        const u8 *one = snap_one + (u64)i * PAGE, *two = P(u8, hot[i]);
        char line[512];
        u32 words = 0, nw = 0;
        int n = snprintf(line, sizeof line, "[rbtest] round %u: page %#llx differs in", round_i + 1, (unsigned long long)hot[i]);
        for (u32 o = 0; o < PAGE; o += 8) {
            u64 x, y;
            memcpy(&x, one + o, 8);
            memcpy(&y, two + o, 8);
            if (x == y) continue;
            words++;
            if (nw < 4) { n += snprintf(line + n, sizeof line - n, " +%#x: %#llx / %#llx", o, (unsigned long long)x, (unsigned long long)y); nw++; }
        }
        fprintf(stderr, "%s (%u words of 512)\n", line, words);
        shown++;
    }
}

/* ---- who writes the pages the replays disagree on ------------------------------------------
 * The pages that differed between the replays in the last round (the simulation's, up to MAX_SUSPECT)
 * are made read-only while this round replays: each write to one stops at the exception handler,
 * which notes the thread and the code, lets that one instruction run with the page writable
 * (single-step), and makes it read-only again. A write by a thread other than the replaying one is
 * a write the rewind cannot undo. (Windows only.) */
#define MAX_SUSPECT 32
#define MAX_WRITERS 8      /* threads noted a page */
typedef struct Writer { DWORD tid; u64 rip; u32 writes, offset; } Writer;
static u64 suspect[MAX_SUSPECT];
static u32 nsuspect;
static DWORD guest_tid[64];
static u64 guest_id[64];
static u32 nguest;

static void note_guest(void *user, u64 id, int priority, int core, int state) {
    (void)user; (void)priority; (void)core; (void)state;
    HANDLE h = kernel_thread_host(id);
    if (h && nguest < 64) { guest_tid[nguest] = GetThreadId(h); guest_id[nguest] = id; nguest++; }
}

static long long guest_of(DWORD tid) {
    for (u32 i = 0; i < nguest; i++) if (guest_tid[i] == tid) return (long long)guest_id[i];
    return -1;
}

#ifdef _WIN32
static Writer writers[MAX_SUSPECT][MAX_WRITERS];
static volatile LONG tracing;
static int replay_no;
static __declspec(thread) int pending = -1;   /* the suspect page this thread is writing (single-stepping) */

static const char *trace_name(int which) { return which == 3 ? "the game's next frame" : which == 2 ? "replay 2" : "replay 1"; }

static void protect_suspects(DWORD prot) {
    for (u32 i = 0; i < nsuspect; i++) {
        DWORD old;
        if (committed(suspect[i])) VirtualProtect(P(void, suspect[i]), PAGE, prot, &old);
    }
}

/* The census, once before the first round's save: for CENSUS_FRAMES frames every hot page is read-only
 * from each frame's simulation start to its end and from its end to the next start; the first write to
 * a page in each of those spans notes the thread (one bit a thread) and makes the page writable again.
 * A thread other than the main one that writes outside the simulation's span goes at its own pace
 * (sound, effects...): every page it writes is left out of the rewinds from the start. */
#define MAX_SLOTS 64
static u64 census_page[MAX_HOT], census_in[MAX_HOT], census_out[MAX_HOT];   /* (sorted pages; who wrote each: in / out of the simulation) */
static u32 ncensus;
static volatile LONG census_on, census_in_sim, nslots;
static volatile DWORD slot_tid[MAX_SLOTS];

static int page_cmp(const void *a, const void *b) {
    u64 x = *(const u64 *)a, y = *(const u64 *)b;
    return x < y ? -1 : x > y;
}

static int census_find(u64 page) {
    u32 lo = 0, hi = ncensus;
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        if (census_page[mid] < page) lo = mid + 1;
        else hi = mid;
    }
    return lo < ncensus && census_page[lo] == page ? (int)lo : -1;
}

static int slot_of(DWORD tid) {
    for (;;) {
        LONG n = nslots;
        for (LONG i = 0; i < n; i++) if (slot_tid[i] == tid) return (int)i;
        if (n == MAX_SLOTS) return MAX_SLOTS - 1;
        if (InterlockedCompareExchange(&nslots, n + 1, n) == n) { slot_tid[n] = tid; return (int)n; }
    }
}

static void census_protect(DWORD prot) {   /* (runs of neighbouring pages in one call) */
    for (u32 i = 0; i < ncensus;) {
        u32 j = i + 1;
        while (j < ncensus && census_page[j] == census_page[j - 1] + PAGE && committed(census_page[j])) j++;
        DWORD old;
        if (committed(census_page[i])) VirtualProtect(P(void, census_page[i]), (j - i) * PAGE, prot, &old);
        i = j;
    }
}

static LONG WINAPI on_write(EXCEPTION_POINTERS *e);
static void *write_handler;

/* ---- copy on write (freeze mode): the pages the save did not take, copied the first time anything writes them ----
 * The saved set is what the game wrote in the 120 frames before; a window with new effects writes pages it had not touched
 * (146 in 10 frames), and those were not rewound. From the save on, every other page of the guest's memory is read-only; the
 * first write to one (any thread, the host's too) copies it as it was and lets the write through. Put back with the others. */
#define MAX_COW 16384
#define MAX_COW_RUNS 65536
static volatile LONG cow_on, ncow;
static LONG ncow_at_now;
static u8 *cow_bits;                           /* one bit an alias page: read-only, not copied yet */
static u64 cow_page[MAX_COW];
static u8 *cow_orig, *cow_now;
static u64 cow_run_base[MAX_COW_RUNS], cow_run_size[MAX_COW_RUNS];
static u32 ncow_runs;
static u64 cow_overflow, cow_unprotected;
static SRWLOCK cow_lock = SRWLOCK_INIT;
/* The modules' global variables too (outside the alias area, never in the saved set: the game's own counters, singletons and
 * random states there were not rewound at all) - each data region with a bitmap of its own. */
#define MAX_DATA_REGIONS 256
static u64 data_base[MAX_DATA_REGIONS], data_size[MAX_DATA_REGIONS];
static u8 *data_bits[MAX_DATA_REGIONS];
static u32 ndata;
static volatile int ring_on;             /* (SSBU_RB_EVERY: a page first written joins the pages saved every frame - below) */
static void ring_promote(u64 page);
static u8 *cow_bit_of(u64 page, u8 *mask) {   /* the byte and bit that say page waits for its copy, or NULL */
    if (page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE) {
        if (!cow_bits) return NULL;
        u64 i = (page - ALIAS_BASE) / PAGE;
        *mask = (u8)(1u << (i & 7));
        return &cow_bits[i >> 3];
    }
    for (u32 r = 0; r < ndata; r++)
        if (page >= data_base[r] && page < data_base[r] + data_size[r] && data_bits[r]) {
            u64 i = (page - data_base[r]) / PAGE;
            *mask = (u8)(1u << (i & 7));
            return &data_bits[r][i >> 3];
        }
    return NULL;
}
static int cow_take(u64 page) {   /* (the write handler) 1: it was a page waiting for its copy - copied, writable now */
    u8 mask, *bit = cow_on ? cow_bit_of(page, &mask) : NULL;
    if (!bit) return 0;
    if (!(*bit & mask)) {   /* (another thread made it writable between this write and now: go through) */
        MEMORY_BASIC_INFORMATION mi;
        return VirtualQuery(P(void, page), &mi, sizeof mi) && mi.Protect == PAGE_READWRITE;
    }
    AcquireSRWLockExclusive(&cow_lock);
    if (*bit & mask) {
        LONG k = ncow;
        if (ring_on) ring_promote(page);
        else if (k < MAX_COW) { cow_page[k] = page; memcpy(cow_orig + (u64)k * PAGE, P(void, page), PAGE); ncow = k + 1; }
        else cow_overflow++;
        DWORD old;
        VirtualProtect(P(void, page), PAGE, PAGE_READWRITE, &old);
        *bit &= (u8)~mask;   /* (after: another thread's write meanwhile still finds its bit, waits for the lock and goes through) */
    }
    ReleaseSRWLockExclusive(&cow_lock);
    return 1;
}
static void cow_begin(void) {   /* (frozen, right after the save) */
    if (!cow_bits) cow_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!cow_orig) cow_orig = malloc(MAX_COW * PAGE);
    if (!cow_now) cow_now = malloc(MAX_COW * PAGE);
    if (!cow_bits || !cow_orig || !cow_now) return;
    if (!write_handler) write_handler = AddVectoredExceptionHandler(1, on_write);
    memset(cow_bits, 0, ALIAS_SIZE / PAGE / 8);
    ncow = 0; ncow_runs = 0; cow_overflow = 0;
    ndata = 0;
    cow_on = 1;   /* (from here: the renderer's threads are not held, and may write a page the moment it is read-only - its bit is set first) */
    read_regions();
    u64 pages = 0;
    for (u32 r = 0; r < nalias; r++) {
        u64 p = alias_base[r], end = alias_base[r] + alias_size[r];
        while (p < end) {
            while (p < end && is_hot(p)) p += PAGE;
            u64 start = p;
            while (p < end && !is_hot(p)) { u64 i = (p - ALIAS_BASE) / PAGE; cow_bits[i >> 3] |= (u8)(1u << (i & 7)); p += PAGE; }
            if (p > start && ncow_runs < MAX_COW_RUNS) {
                DWORD old;
                if (VirtualProtect(P(void, start), p - start, PAGE_READONLY, &old)) {
                    cow_run_base[ncow_runs] = start; cow_run_size[ncow_runs] = p - start; ncow_runs++; pages += (p - start) / PAGE;
                } else   /* (across two reservations: a page at a time) */
                    for (u64 q = start; q < p; q += PAGE) {
                        if (ncow_runs < MAX_COW_RUNS && VirtualProtect(P(void, q), PAGE, PAGE_READONLY, &old)) {
                            cow_run_base[ncow_runs] = q; cow_run_size[ncow_runs] = PAGE; ncow_runs++; pages++;
                        } else { u64 i = (q - ALIAS_BASE) / PAGE; cow_bits[i >> 3] &= (u8)~(1u << (i & 7)); cow_unprotected++; }
                    }
            }
        }
    }
    u64 data_pages = 0;
    static u64 base[MAX_DATA_REGIONS], size[MAX_DATA_REGIONS];
    u32 nd = kernel_data_regions(base, size, MAX_DATA_REGIONS);
    for (u32 r = 0; r < nd; r++) {
        u64 n = size[r] / PAGE;
        free(data_bits[r]);
        data_bits[r] = malloc((n + 7) / 8);
        if (!data_bits[r]) continue;
        memset(data_bits[r], 0xFF, (n + 7) / 8);
        data_base[r] = base[r]; data_size[r] = size[r];
        MemoryBarrier();
        ndata = r + 1;
        DWORD old;
        if (ncow_runs < MAX_COW_RUNS && VirtualProtect(P(void, data_base[r]), data_size[r], PAGE_READONLY, &old)) {
            cow_run_base[ncow_runs] = data_base[r]; cow_run_size[ncow_runs] = data_size[r]; ncow_runs++;
            data_pages += n;
        } else memset(data_bits[r], 0, (n + 7) / 8);
    }
    fprintf(stderr, "[rbtest] copy on write: %llu pages read-only in %u runs, and %llu of the modules' global variables in %u regions; %llu pages could not be made read-only (their writes go unseen)\n",
            (unsigned long long)pages, ncow_runs, (unsigned long long)data_pages, ndata, (unsigned long long)cow_unprotected);
}
static void cow_keep_now(void) {   /* the game's own state at the end of the window, for the pages copied so far */
    ncow_at_now = ncow;
    for (LONG k = 0; k < ncow_at_now; k++) memcpy(cow_now + (u64)k * PAGE, P(void, cow_page[k]), PAGE);
}
static void cow_put_back(int to_now) {   /* (a page first written while replaying was as at the save at the window's end too) */
    for (LONG k = 0; k < ncow; k++) memcpy(P(void, cow_page[k]), (to_now && k < ncow_at_now ? cow_now : cow_orig) + (u64)k * PAGE, PAGE);
}
static void cow_end(void) {
    AcquireSRWLockExclusive(&cow_lock);
    for (u32 r = 0; r < ncow_runs; r++) { DWORD old; VirtualProtect(P(void, cow_run_base[r]), cow_run_size[r], PAGE_READWRITE, &old); }
    cow_on = 0;   /* (only now: a write meanwhile still finds its page's bit) */
    if (cow_bits) memset(cow_bits, 0, ALIAS_SIZE / PAGE / 8);
    for (u32 r = 0; r < ndata; r++) if (data_bits[r]) memset(data_bits[r], 0, (data_size[r] / PAGE + 7) / 8);
    ReleaseSRWLockExclusive(&cow_lock);
    fprintf(stderr, "[rbtest] copy on write: %ld pages copied (first written after the save)%s\n", (long)ncow, cow_overflow ? ", more than the most kept" : "");
}

static void census_begin(void) {
    if (!write_handler) write_handler = AddVectoredExceptionHandler(1, on_write);
    read_regions();
    ncensus = 0;
    for (u32 i = 0; i < nhot; i++) if (committed(hot[i])) census_page[ncensus++] = hot[i];
    qsort(census_page, ncensus, sizeof *census_page, page_cmp);
    memset(census_in, 0, sizeof census_in);
    memset(census_out, 0, sizeof census_out);
    nslots = 0;
    slot_of(sim_thread);   /* (slot 0: the main thread) */
    census_in_sim = 1;
    census_on = 1;
    census_protect(PAGE_READONLY);
}

static void census_span(int in_sim) {   /* a span begins: every page read-only again */
    census_in_sim = in_sim;
    census_protect(PAGE_READONLY);
}

static void census_end(void) {
    census_protect(PAGE_READWRITE);   /* (before the handler stops: a write to a page still read-only now would find no one to let it through) */
    census_on = 0;
    nguest = 0;
    kernel_thread_each(note_guest, NULL);
    u64 foreign_threads = 0;
    for (LONG k = 1; k < nslots; k++) {
        u32 in = 0, out = 0;
        for (u32 i = 0; i < ncensus; i++) { in += census_in[i] >> k & 1; out += census_out[i] >> k & 1; }
        if (out) foreign_threads |= 1ull << k;
        fprintf(stderr, "[rbtest] census: thread %lu (guest %lld) wrote %u pages while the simulation ran, %u outside it%s\n", (unsigned long)slot_tid[k],
                guest_of(slot_tid[k]), in, out, out ? ": its own pace, its pages left out" : "");
    }
    u32 main_in = 0, main_out = 0, mixed = 0, before = nforeign;
    for (u32 i = 0; i < ncensus; i++) {
        main_in += census_in[i] & 1;
        main_out += census_out[i] & 1;
        u64 all = census_in[i] | census_out[i];
        if (all & foreign_threads) {
            mark_foreign(census_page[i]);
            if (all & ~foreign_threads) mixed++;
        }
    }
    nshared += mixed;
    {   /* per allocator: pages written, and how many of them a thread of its own pace wrote too */
        static u32 pages_in[MAX_ALLOCS + 1], pages_foreign[MAX_ALLOCS + 1], pages_sim[MAX_ALLOCS + 1];
        memset(pages_in, 0, sizeof pages_in); memset(pages_foreign, 0, sizeof pages_foreign); memset(pages_sim, 0, sizeof pages_sim);
        for (u32 i = 0; i < ncensus; i++) {
            int a = alloc_of(census_page[i]);
            int k = a < 0 ? MAX_ALLOCS : a;
            u64 all = census_in[i] | census_out[i];
            pages_in[k]++;
            if (all & foreign_threads) pages_foreign[k]++;
            if (census_in[i] && !(all & foreign_threads)) pages_sim[k]++;
        }
        LONG n = nallocs < MAX_ALLOCS ? nallocs : MAX_ALLOCS;
        for (LONG a = 0; a <= n; a++) {
            int k = a == n ? MAX_ALLOCS : (int)a;
            if (!pages_in[k]) continue;
            if (k == MAX_ALLOCS) fprintf(stderr, "[rbtest] census by allocator: outside every allocator: %u pages written, %u by the simulation only, %u by a thread of its own pace\n",
                                         pages_in[k], pages_sim[k], pages_foreign[k]);
            else fprintf(stderr, "[rbtest] census by allocator: %d (%#llx-%#llx, %.1f MB, from main+%#llx): %u pages written, %u by the simulation only, %u by a thread of its own pace\n",
                         k, (unsigned long long)allocs[k].base, (unsigned long long)(allocs[k].base + allocs[k].size), allocs[k].size / 1048576.0,
                         (unsigned long long)(allocs[k].caller - 0x7100000000ull), pages_in[k], pages_sim[k], pages_foreign[k]);
        }
    }
    {   /* by 1 MB region of the address space: whether the combat's pages sit in regions of their own */
        u32 i = 0, shown = 0, clean = 0, foreign_only = 0, both = 0;
        while (i < ncensus) {
            u64 mb = census_page[i] >> 20;
            u32 sim = 0, own = 0, n = 0;
            for (; i < ncensus && census_page[i] >> 20 == mb; i++) {
                u64 all = census_in[i] | census_out[i];
                n++;
                if (all & foreign_threads) own++; else if (census_in[i]) sim++;
            }
            if (sim && !own) clean++; else if (own && !sim) foreign_only++; else if (sim && own) both++;
            if (n >= 4 && shown++ < 200)
                fprintf(stderr, "[rbtest] census by MB: %#llx: %u pages, %u by the simulation only, %u by a thread of its own pace\n", (unsigned long long)(mb << 20), n, sim, own);
        }
        fprintf(stderr, "[rbtest] census by MB: regions with simulation pages only %u, with own-pace pages only %u, with both %u\n", clean, foreign_only, both);
    }
    fprintf(stderr, "[rbtest] census (%u frames, %u pages): the main thread wrote %u pages while the simulation ran, %u outside it; pages left out: %u, %u of them written by the main thread or a helper too\n",
            CENSUS_FRAMES, ncensus, main_in, main_out, nforeign - before, mixed);
}

static LONG WINAPI on_write(EXCEPTION_POINTERS *e) {
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (census_on && code == EXCEPTION_ACCESS_VIOLATION && e->ExceptionRecord->NumberParameters >= 2 && e->ExceptionRecord->ExceptionInformation[0] == 1) {
        u64 host = e->ExceptionRecord->ExceptionInformation[1], base = (u64)(uintptr_t)P(u8, ALIAS_BASE);
        if (host >= base && host < base + ALIAS_SIZE) {
            u64 page = ALIAS_BASE + ((host - base) & ~(PAGE - 1));
            int i = census_find(page);
            if (i >= 0) {
                u64 bit = 1ull << slot_of(GetCurrentThreadId());
                if (census_in_sim) census_in[i] |= bit;
                else census_out[i] |= bit;
                DWORD old;
                VirtualProtect(P(void, page), PAGE, PAGE_READWRITE, &old);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    if (code == STATUS_SINGLE_STEP && pending >= 0) {
        DWORD old;
        if (tracing) VirtualProtect(P(void, suspect[pending]), PAGE, PAGE_READONLY, &old);
        pending = -1;
        return EXCEPTION_CONTINUE_EXECUTION;   /* (the trap flag clears itself) */
    }
    if (code != EXCEPTION_ACCESS_VIOLATION || e->ExceptionRecord->NumberParameters < 2 || e->ExceptionRecord->ExceptionInformation[0] != 1)
        return EXCEPTION_CONTINUE_SEARCH;
    if (cow_on) {   /* (guest addresses map to host ones by a fixed offset: G2H) */
        u64 h = e->ExceptionRecord->ExceptionInformation[1], g0 = (u64)(uintptr_t)P(u8, 0);
        if (h >= g0 && cow_take((h - g0) & ~(PAGE - 1))) return EXCEPTION_CONTINUE_EXECUTION;
        static volatile LONG told;
        if (h >= g0 && InterlockedIncrement(&told) <= 3) {   /* (a write the copy on write did not expect: why) */
            u64 page = (h - g0) & ~(PAGE - 1);
            u8 mask = 0, *bit = cow_bit_of(page, &mask);
            MEMORY_BASIC_INFORMATION mi;
            SIZE_T q = VirtualQuery(P(void, page), &mi, sizeof mi);
            fprintf(stderr, "[rbtest] copy on write: a write to %#llx it does not know (in the alias area %d, hot %d, bit %s, protection %#lx, state %#lx, allocation %#llx)\n",
                    (unsigned long long)page, page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE, page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE ? is_hot(page) : -1,
                    bit ? (*bit & mask ? "set" : "clear") : "none", q ? (unsigned long)mi.Protect : 0ul, q ? (unsigned long)mi.State : 0ul,
                    q ? (unsigned long long)((u8 *)mi.AllocationBase - (u8 *)P(u8, 0)) : 0ull);
        }
    }
    u64 host = e->ExceptionRecord->ExceptionInformation[1];
    if (host < (u64)(uintptr_t)P(u8, ALIAS_BASE) || host >= (u64)(uintptr_t)P(u8, ALIAS_BASE) + ALIAS_SIZE) return EXCEPTION_CONTINUE_SEARCH;
    u64 guest = ALIAS_BASE + (host - (u64)(uintptr_t)P(u8, ALIAS_BASE));
    int k = -1;
    for (u32 i = 0; i < nsuspect; i++) if (guest >> 12 == suspect[i] >> 12) { k = (int)i; break; }
    if (k < 0) return EXCEPTION_CONTINUE_SEARCH;
    DWORD tid = GetCurrentThreadId(), old;
    Writer *w = writers[k];
    for (int i = 0; i < MAX_WRITERS; i++) {
        if (w[i].tid == tid) { w[i].writes++; break; }
        if (!w[i].tid) {
            w[i].tid = tid; w[i].rip = e->ContextRecord->Rip; w[i].offset = (u32)(guest & 0xFFF); w[i].writes = 1;
            fprintf(stderr, "[rbtest] %s: page %#llx +%#x first written by thread %lu (guest thread %lld%s), host pc ssbu.exe+%#llx\n",
                    trace_name(replay_no), (unsigned long long)suspect[k], w[i].offset, (unsigned long)tid, guest_of(tid), on_sim_thread() ? ", the replaying one" : "",
                    (unsigned long long)(w[i].rip - (u64)(uintptr_t)GetModuleHandleW(NULL)));
            rt_backtrace();
            break;
        }
    }
    VirtualProtect(P(void, suspect[k]), PAGE, PAGE_READWRITE, &old);
    pending = k;
    e->ContextRecord->EFlags |= 0x100;   /* one instruction, then back to read-only */
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void trace_begin(int which) {
    if (!nsuspect) return;
    if (!write_handler) write_handler = AddVectoredExceptionHandler(1, on_write);
    replay_no = which;
    memset(writers, 0, sizeof writers);
    tracing = 1;
    protect_suspects(PAGE_READONLY);
}

static int trace_active(void) { return tracing; }

static void trace_end(void) {
    if (!nsuspect || !tracing) return;
    tracing = 0;
    protect_suspects(PAGE_READWRITE);
    for (u32 k = 0; k < nsuspect; k++) {
        char line[512];
        int n = snprintf(line, sizeof line, "[rbtest] %s: page %#llx written by", trace_name(replay_no), (unsigned long long)suspect[k]);
        int any = 0;
        for (int i = 0; i < MAX_WRITERS && writers[k][i].tid; i++, any = 1)
            n += snprintf(line + n, sizeof line - n, " thread %lu (guest %lld%s) %u times or more;", (unsigned long)writers[k][i].tid, guest_of(writers[k][i].tid),
                          writers[k][i].tid == sim_thread ? ", the replaying one" : "", writers[k][i].writes);
        if (!any) snprintf(line + n, sizeof line - n, " nobody");
        fprintf(stderr, "%s\n", line);
        int other = 0, own = 0;
        for (int i = 0; i < MAX_WRITERS && writers[k][i].tid; i++) {
            if (writers[k][i].tid != sim_thread) other = 1;
            else own = 1;
        }
        if (other && !is_foreign(suspect[k])) { mark_foreign(suspect[k]); nshared += own; }
    }
}
static int census_possible(void) { return !freeze_on(); }   /* (rewinding everything: no census) */
#else
static int census_possible(void) { return 0; }
static void cow_begin(void) {}
static void cow_keep_now(void) {}
static void cow_put_back(int to_now) { (void)to_now; }
static void cow_end(void) {}
static void census_begin(void) {}
static void census_span(int in_sim) { (void)in_sim; }
static void census_end(void) {}
static void trace_begin(int which) { (void)which; }
static void trace_end(void) {}
static int trace_active(void) { return 0; }
#endif

/* ---- saving every frame (SSBU_RB_EVERY=1, with freeze mode) ----------------------------------------------------------
 * What a real rollback needs: the state at the start of each of the last frames, to go back to any of them. A ring of
 * SSBU_RB_RING saves (default 8, at most 16), one each frame at its simulation's start, with every other guest thread
 * held (as freeze mode's save): the pages, the thread-local regions and the fibers.
 *
 * No discovery window: from the start every page of the game's memory (the alias area and the modules' global variables)
 * is read-only, and the first write to one makes it one of the pages saved - its contents then are what every save
 * taken since had (nothing wrote it before), so they go into those too. The pages saved grow to what the game writes.
 *
 * A frame's record runs from its simulation's start to the next's: the simulation's pad and tick reads, then the next
 * frame's input step's. Every SSBU_RB_GAP seconds (default 5), a rollback of N frames (SSBU_RB_ROUNDS, at most the
 * ring's size less one): back to the save N frames ago, replayed with the recorded pads (1), with other buttons (2) and
 * with the recorded ones again (3); 1 and 3 must come out the same. Then the game goes on from the save of this frame,
 * or (SSBU_RB_CONTINUE=1) from replay 3's state, as a real rollback would. Every 120 frames, what the saving cost. */
#ifdef _WIN32
#define RING_MAX 16
#define RING_GROW 1024                    /* (pages committed at once in each save) */
static int every_mode = -1, ring_n = 8, ring_continue;
static u64 ring_gap_ns = 5000000000ull;
static u8 *ring_pages[RING_MAX];
static u64 ring_frame_of[RING_MAX];       /* the frame saved there, plus one (0: none, or not all held then) */
static u8 *ring_tlr[RING_MAX];
static u64 ring_tlr_size[RING_MAX];
static u8 ring_events[RING_MAX][4096];
static const u64 ring_reg_ids[4] = {3, 4, 12, 13};   /* (the helpers whose registers each save keeps, to say what differs) */
static u64 ring_regs[RING_MAX][4][32];
static u64 ring_waits[RING_MAX][64 * 6];
static u32 ring_nwaits[RING_MAX];
static void show_wait(const char *when, const u64 *w) {
    fprintf(stderr, "[rbtest]   %s: guest thread %llu%s%s%s%s mutex %#llx condition %#llx address %#llx handles %llu fiber %#llx\n", when,
            (unsigned long long)(w[0] & 0xFFFFFFFFFFFFull), w[0] >> 56 & 1 ? " sleeping" : "", w[0] >> 56 & 2 ? " requesting" : "", w[0] >> 56 & 4 ? " parked" : "",
            w[0] >> 56 & 8 ? " held" : "", (unsigned long long)w[1], (unsigned long long)w[2], (unsigned long long)w[3], (unsigned long long)w[4], (unsigned long long)w[5]);
}
static u64 ring_keys[RING_MAX][256];
static u32 ring_nkeys[RING_MAX];
static u32 ring_room;                     /* pages committed in each save */
static u8 hot_ok[MAX_HOT];                /* (a page of the alias area not mapped now is not copied) */
static u64 ring_frame_no, ring_region_sig;
static u64 ring_promoted, ring_overflow, ring_saves, ring_unheld, ring_save_ns, ring_save_max_ns, ring_hold_ns, ring_hold_max_ns;
static u64 ring_pages_ns, ring_tlr_ns, ring_fib_ns, ring_wait_ns;
static u32 ring_fibers, ring_skipped;
static u64 ring_sum[4][MAX_HOT];
static u64 plan_from, plan_at;
/* SSBU_RB_NATURAL=1: a rollback as a real one - the frames before it played live with other buttons (a wrong guess of the other
 * player's), then the save of the first of them put back and the frames played again by the game's own main loop (every step it
 * takes, the drawing too) with the recorded buttons, then on from there */
static int ring_natural;
static u64 natural_until, natural_t0;
static int natural_started;        /* (the next rollback: back to frame plan_from's save at frame plan_at; 0: none planned) */
static u64 plan_expect[MAX_HOT];      /* (that save's pages' hashes when it was taken) */
static u32 plan_n, plan_fib_n;
static u64 plan_fib[3 * 16384];          /* the game's, replay 1's, 2's and 3's */

static int every_on(void) {
    if (every_mode < 0) {
        const char *e = getenv("SSBU_RB_EVERY"), *r = getenv("SSBU_RB_RING"), *g = getenv("SSBU_RB_GAP"), *k = getenv("SSBU_RB_CONTINUE");
        every_mode = e && atoi(e) != 0 && freeze_on();
        if (r && *r) { ring_n = atoi(r); if (ring_n < 2) ring_n = 2; if (ring_n > RING_MAX) ring_n = RING_MAX; }
        if (g && *g) ring_gap_ns = (u64)(atof(g) * 1e9);
        ring_continue = k && atoi(k) != 0;
        ring_natural = getenv("SSBU_RB_NATURAL") && atoi(getenv("SSBU_RB_NATURAL")) != 0;
    }
    return every_mode;
}

static int ring_grow(u32 need) {   /* (cow_lock held) every save has room for `need` pages */
    while (ring_room < need) {
        u32 more = ring_room + RING_GROW > MAX_HOT ? MAX_HOT - ring_room : RING_GROW;
        if (!more) return 0;
        for (int s = 0; s < ring_n; s++)
            if (!VirtualAlloc(ring_pages[s] + (u64)ring_room * PAGE, (u64)more * PAGE, MEM_COMMIT, PAGE_READWRITE)) return 0;
        ring_room += more;
    }
    return 1;
}

/* Only what changes is copied each frame. A saved page is "active" (writable, copied at every save) or "watched" (read-only
 * since frame `since`, not copied: nobody has written it since). Every frame 1/SSBU_RB_IDLE (16) of the active pages become
 * watched, right after this frame's save copied them; the first write to a watched one makes it active again, its contents
 * (as at `since`) going first into the saves taken after `since`, which did not copy it. A page written every frame is
 * watched for one frame in 16; one the game leaves alone stays out of the copying. */
static u32 ring_idle = 16, ring_cursor;
static u8 hot_watched[MAX_HOT];
static DWORD hot_writer[MAX_HOT];   /* (the last thread whose write made the page active: who writes it) */
static u64 hot_since[MAX_HOT];
#define MAP_SIZE (MAX_HOT * 2)
static u64 map_key[MAP_SIZE];             /* page -> its index in hot[] (open addressing; 0: free) */
static u32 map_val[MAP_SIZE];
static u64 ring_woken, ring_demoted, ring_demote_ns;
static u32 ring_active;

static u32 *map_slot(u64 page, int add) {
    u32 h = (u32)((page >> 12) * 0x9E3779B97F4A7C15ull >> 40) & (MAP_SIZE - 1);
    while (map_key[h] && map_key[h] != page) h = (h + 1) & (MAP_SIZE - 1);
    if (!map_key[h]) { if (!add) return NULL; map_key[h] = page; }
    return &map_val[h];
}

/* Pages only the runtime's own threads write (the renderer: the GPU's semaphores and query results in the game's memory) are
 * the PC's progress, not the game's: not saved, put back or compared - a rewind would make the game wait for GPU work done. One
 * the game writes too is the game's. */
static u8 hot_host[MAX_HOT];
static u64 ring_host_turned;   /* (runtime pages the game then wrote: the game's from then on) */
static DWORD guest_tids[256];
static volatile LONG nguest_tids;
static void note_guest_tid(void *user, u64 id, int priority, int core, int state) {
    (void)user; (void)priority; (void)core; (void)state;
    HANDLE h = kernel_thread_host(id);
    if (h && nguest_tids < 256) guest_tids[nguest_tids++] = GetThreadId(h);
}
static int is_guest_tid(DWORD tid) {
    for (LONG i = 0; i < nguest_tids; i++) if (guest_tids[i] == tid) return 1;
    return 0;
}
static void ring_wake(u32 i, int by) {   /* (cow_lock held) a watched page active again: the saves since it was watched get it; by: 1 the
                                          * game's thread wrote it, 0 the runtime's, -1 neither (a put-back) */
    /* (a page the runtime alone had written was in no save: as now in every one, as a page new to them) */
    int to_all = by > 0 && hot_host[i];
    if (to_all) ring_host_turned++;
    for (int s = 0; s < ring_n; s++)
        if (ring_frame_of[s] && (to_all || ring_frame_of[s] - 1 > hot_since[i])) memcpy(ring_pages[s] + (u64)i * PAGE, P(void, hot[i]), PAGE);
    hot_watched[i] = 0;
    if (by >= 0) hot_writer[i] = GetCurrentThreadId();
    if (by > 0) hot_host[i] = 0;
    ring_woken++;
}

static void ring_promote(u64 page) {   /* (the write handler, cow_lock held; the caller then makes the page writable) */
    u32 *known = map_slot(page, 0);
    if (known) { ring_wake(*known, is_guest_tid(GetCurrentThreadId())); return; }
    if (nhot == MAX_HOT || !ring_grow(nhot + 1)) { ring_overflow++; return; }
    u32 i = nhot;
    for (int s = 0; s < ring_n; s++) if (ring_frame_of[s]) memcpy(ring_pages[s] + (u64)i * PAGE, P(void, page), PAGE);
    hot[i] = page;
    hot_ok[i] = 1;
    hot_watched[i] = 0;
    hot_writer[i] = GetCurrentThreadId();
    hot_host[i] = !is_guest_tid(hot_writer[i]);
    *map_slot(page, 1) = i;
    if (page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE) { u64 b = (page - ALIAS_BASE) / PAGE; hot_bits[b >> 3] |= (u8)(1u << (b & 7)); }
    nhot = i + 1;
    ring_promoted++;
}

/* Read-only, [from, to): in one call, or - across two reservations Windows refuses that - a page at a time; a page that cannot be
 * is active again (a write to it would go unseen) */
static u64 ring_protect_failed;
static void ring_protect_run(u64 from, u64 to) {
    DWORD old;
    if (VirtualProtect(P(void, from), to - from, PAGE_READONLY, &old)) return;
    for (u64 p = from; p < to; p += PAGE) {
        if (VirtualProtect(P(void, p), PAGE, PAGE_READONLY, &old)) continue;
        ring_protect_failed++;
        u32 *k = map_slot(p, 0);
        u8 mask, *bit = cow_bit_of(p, &mask);
        if (bit) *bit &= (u8)~mask;
        if (k) hot_watched[*k] = 0;
    }
}
static void ring_demote(u32 n) {   /* (frozen, cow_lock held, right after the save) the next slice of the pages watched */
    u32 per = (n + ring_idle - 1) / ring_idle;
    u64 run = 0, run_end = 0;
    for (u32 j = 0; j < per && n; j++) {
        u32 i = ring_cursor++ % n;
        u8 mask, *bit;
        if (hot_watched[i] || !hot_ok[i] || !(bit = cow_bit_of(hot[i], &mask))) continue;
        hot_watched[i] = 1;
        hot_since[i] = ring_frame_no;
        *bit |= mask;   /* (first: a write the moment it is read-only finds it) */
        ring_demoted++;
        if (hot[i] == run_end) { run_end += PAGE; continue; }
        if (run) ring_protect_run(run, run_end);
        run = hot[i]; run_end = run + PAGE;
    }
    if (run) ring_protect_run(run, run_end);
}

static void ring_wake_now(u32 i) {   /* (cow_lock held) active again without a write: before a put-back writes it */
    u8 mask, *bit = cow_bit_of(hot[i], &mask);
    ring_wake(i, -1);
    DWORD old;
    VirtualProtect(P(void, hot[i]), PAGE, PAGE_READWRITE, &old);
    if (bit) *bit &= (u8)~mask;   /* (after: a write meanwhile still finds its bit, waits for the lock and goes through) */
}

static void ring_check_regions(void) {   /* the alias area's mapping changed: which saved pages can be copied */
    read_regions();
    u64 sig = nalias;
    for (u32 r = 0; r < nalias; r++) sig = sig * 31 + alias_base[r] * 7 + alias_size[r];
    if (sig == ring_region_sig) return;
    ring_region_sig = sig;
    for (u32 i = 0; i < nhot; i++) hot_ok[i] = hot[i] < ALIAS_BASE || hot[i] >= ALIAS_BASE + ALIAS_SIZE || committed(hot[i]);
}

static int ring_begin(void) {
    if (!hot_bits) hot_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    for (int s = 0; s < ring_n; s++) {
        if (!ring_pages[s]) ring_pages[s] = VirtualAlloc(NULL, (u64)MAX_HOT * PAGE, MEM_RESERVE, PAGE_READWRITE);
        if (!ring_tlr[s]) ring_tlr[s] = malloc(TLR_MAX);
        if (!ring_pages[s] || !ring_tlr[s]) return 0;
    }
    if (!hot_bits) return 0;
    read_rounds();
    u64 busy = 0;
    int frozen = freeze_all(0, 500, &busy);
    nhot = 0;
    memset(ring_frame_of, 0, sizeof ring_frame_of);
    memset(map_key, 0, sizeof map_key);
    memset(hot_watched, 0, sizeof hot_watched);
    memset(hot_host, 0, sizeof hot_host);
    nguest_tids = 0;
    kernel_thread_each(note_guest_tid, NULL);
    const char *idle = getenv("SSBU_RB_IDLE");
    if (idle && *idle) { ring_idle = (u32)atoi(idle); if (ring_idle < 2) ring_idle = 2; }
    ring_on = 1;
    cow_begin();   /* (nothing saved yet: everything read-only) */
    if (frozen) kernel_thaw();
    ring_region_sig = 0;
    ring_frame_no = 0;
    fprintf(stderr, "[rbtest] saving every frame: a ring of %d saves; rollbacks of", ring_n);
    for (u32 i = 0; i < ROUNDS; i++) fprintf(stderr, " %u", round_frames[i] < (u32)ring_n ? round_frames[i] : (u32)ring_n - 1);
    fprintf(stderr, " frames, %.0f s apart; afterwards the game goes on from %s; each frame 1/%u of the pages copied are watched instead\n", ring_gap_ns / 1e9,
            ring_continue ? "replay 3 (as a rollback would)" : "its own save", ring_idle);
    fflush(stderr);
    return 1;
}

static void ring_save(Cpu *c, int replaying) {   /* replaying: a frame the main loop plays again - its record is kept */
    u64 t0 = rt_now_ns(), busy = 0;
    int s = (int)(ring_frame_no % (u64)ring_n);
    int frozen = freeze_all(0, 20, &busy);
    u64 held = rt_now_ns() - t0;
    ring_check_regions();
    u64 t1 = rt_now_ns();
    AcquireSRWLockExclusive(&cow_lock);
    u32 n = nhot, active = 0;
    if (ring_grow(n))
        for (u32 i = 0; i < n; i++)
            if (hot_ok[i] && !hot_watched[i] && !hot_host[i]) { memcpy(ring_pages[s] + (u64)i * PAGE, P(void, hot[i]), PAGE); active++; }
    ring_frame_of[s] = frozen ? ring_frame_no + 1 : 0;
    u64 td = rt_now_ns();
    if (frozen) ring_demote(n);   /* (with the threads held: a write the moment a page is read-only must find this save done) */
    ring_demote_ns += rt_now_ns() - td;
    ring_active = active;
    ReleaseSRWLockExclusive(&cow_lock);
    u64 t2 = rt_now_ns();
    tlr_copy(ring_tlr[s], 1);
    ring_tlr_size[s] = tlr_size;
    u64 t3 = rt_now_ns();
    u32 running = 0;
    int f = fibers_save(2 + s, &running);
    ring_fibers = f > 0 ? (u32)f : 0;
    u64 t4 = rt_now_ns();
    ring_pages_ns += t2 - t1; ring_tlr_ns += t3 - t2; ring_fib_ns += t4 - t3;
    memcpy(ring_keys[s], freeze_keys, nfreeze_keys * sizeof *freeze_keys);
    ring_nkeys[s] = nfreeze_keys;
    if (frozen) {
        kernel_events(ring_events[s], 1);
        ring_nwaits[s] = kernel_waits(ring_waits[s], 64);
        for (int h = 0; h < 4; h++) if (!kernel_regs(ring_reg_ids[h], ring_regs[s][h])) memset(ring_regs[s][h], 0, sizeof ring_regs[s][h]);
        if (ring_saves % 60 == 0) {   /* (threads the game made since: theirs too) */
            LONG had = nguest_tids;
            nguest_tids = 0;
            kernel_thread_each(note_guest_tid, NULL);
            if (!nguest_tids) nguest_tids = had;
        }
        /* a rollback this frame: the other threads (but the helpers) held from this save on - one that ran on between the save and
         * the rollback would find what it did undone by the put-back, while it went on */
        if (plan_at && ring_frame_no >= plan_at && !holding) { hold_wanted = 1; hold_others(); }
        if (plan_at && ring_frame_no == plan_from) {   /* (the rollback's save: its pages' hashes now, to check the put-back against) */
            for (u32 i = 0; i < nhot; i++) plan_expect[i] = hot_ok[i] && !hot_host[i] ? page_hash(P(u8, hot[i])) : 0;
            plan_n = nhot;
            plan_fib_n = fibers_hash(plan_fib, 16384);
        }
        kernel_thaw();
    }
    else ring_unheld++;
    if (!replaying) {
        memset(&rec[s], 0, sizeof rec[s]);
        rec[s].sim_cpu = *c;
    }
    start_frame((u32)s);
    hook = replaying ? HOOK_PLAY : HOOK_RECORD;
    u64 took = rt_now_ns() - t0;
    ring_saves++;
    ring_save_ns += took; if (took > ring_save_max_ns) ring_save_max_ns = took;
    ring_hold_ns += held; if (held > ring_hold_max_ns) ring_hold_max_ns = held;
    if (ring_saves % 120 == 0) {
        fprintf(stderr, "[rbtest] every frame: %llu saves, the last 120 in %.2f ms on average (at most %.2f), holding the threads %.2f ms (at most %.2f); %u pages saved (%.1f MB), %llu joined in them%s; %u fibers; not all held: %llu\n",
                (unsigned long long)ring_saves, ring_save_ns / 120 / 1e6, ring_save_max_ns / 1e6, ring_hold_ns / 120 / 1e6, ring_hold_max_ns / 1e6,
                n, n * (double)PAGE / 1048576.0, (unsigned long long)ring_promoted, ring_overflow ? ", more than the most kept" : "", ring_fibers, (unsigned long long)ring_unheld);
        fprintf(stderr, "[rbtest] every frame: of it, on average: copying the pages %.2f ms (the last time %u pages, %.1f MB, of %u; the others not written lately), "
                        "watching %.2f ms; per frame %.0f pages watched, %.0f written again; the thread-local regions %.2f ms (%.0f KB), the fibers %.2f ms\n",
                (ring_pages_ns - ring_demote_ns) / 120 / 1e6, ring_active, ring_active * (double)PAGE / 1048576.0, n, ring_demote_ns / 120 / 1e6,
                ring_demoted / 120.0, ring_woken / 120.0, ring_tlr_ns / 120 / 1e6, tlr_size / 1024.0, ring_fib_ns / 120 / 1e6);
        fflush(stderr);
        ring_save_ns = ring_save_max_ns = ring_hold_ns = ring_hold_max_ns = ring_pages_ns = ring_tlr_ns = ring_fib_ns = 0;
        if (ring_protect_failed || ring_host_turned)
            fprintf(stderr, "[rbtest] every frame: pages that could not be made read-only %llu; the runtime's pages the game then wrote %llu\n",
                    (unsigned long long)ring_protect_failed, (unsigned long long)ring_host_turned);
        ring_demote_ns = ring_demoted = ring_woken = 0;
        ring_promoted = ring_unheld = 0;
    }
}

/* With every other thread at the same place as at save s (the same wait, fiber and registers), within 300 ms; once threads are
 * held (from the first put-back on), their waits are first put back to where they were held (kernel_hold_undo): a wake-up while
 * replaying did not happen in the memory put back. Not in time: put back anyway, and counted. */
static u64 ring_unsafe, ring_tlr_hash;
static u64 fib_before[3 * 16384];
static u32 fib_before_n;
static int keys_same(const u64 *a, u32 na, const u64 *b, u32 nb) {
    if (na != nb) return 0;
    for (u32 i = 0; i < na; i++) {
        u32 j = 0;
        while (j < nb && b[j] != a[i]) j++;
        if (j == nb) return 0;
    }
    return 1;
}
static void ring_put_back(int s) {
    u64 busy = 0, t0 = rt_now_ns();
    int frozen = 0, matched = 0;
    static u64 now_keys[256];
    u32 nnow = 0;
    if (!holding) {
        memcpy(freeze_keys, ring_keys[s], ring_nkeys[s] * sizeof *freeze_keys);
        nfreeze_keys = ring_nkeys[s];
        frozen = matched = freeze_all(1, 300, &busy);
        if (!frozen) frozen = freeze_all(0, 1000, &busy);
        if (frozen) nnow = kernel_keys(now_keys, 256);
    } else
        do {
            if (frozen) { kernel_thaw(); SwitchToThread(); }
            frozen = freeze_all(0, 1000, &busy);
            if (!frozen) break;
            kernel_hold_undo();
            nnow = kernel_keys(now_keys, 256);
            matched = keys_same(now_keys, nnow, ring_keys[s], ring_nkeys[s]);
        } while (!matched && rt_now_ns() - t0 < 300000000ull);
    if (!frozen) fprintf(stderr, "[rbtest] rollback: the other threads not all held in 1 s (guest thread %llu busy): put back anyway\n", (unsigned long long)busy);
    if (!matched) {
        u32 differ = 0;
        for (u32 i = 0; i < nnow; i++) {
            u32 j = 0;
            while (j < ring_nkeys[s] && ring_keys[s][j] != now_keys[i]) j++;
            differ += j == ring_nkeys[s];
        }
        ring_unsafe++;
        fprintf(stderr, "[rbtest] rollback: put back frame %llu's save with %u threads not at the same place as then (wait, fiber or registers)\n",
                (unsigned long long)(ring_frame_of[s] - 1), differ);
    }
    ring_wait_ns += rt_now_ns() - t0;
    ring_check_regions();
    u64 then = ring_frame_of[s] - 1;
    u32 n = nhot;
    for (u32 i = 0; i < n; i++) {
        if (!hot_ok[i] || hot_host[i]) continue;
        if (hot_watched[i]) {
            if (hot_since[i] <= then) continue;   /* (not written since before that save: as it was then) */
            AcquireSRWLockExclusive(&cow_lock);
            if (hot_watched[i]) ring_wake_now(i);
            ReleaseSRWLockExclusive(&cow_lock);
        }
        memcpy(P(void, hot[i]), ring_pages[s] + (u64)i * PAGE, PAGE);   /* (without the lock: a page read-only after all faults to it) */
    }
    tlr_size = ring_tlr_size[s];   /* (a thread made since: its region is past this, and kept) */
    tlr_copy(ring_tlr[s], 0);
    u32 skipped, dropped;
    fibers_restore(2 + s, &skipped, &dropped);
    ring_skipped = skipped;
    if (frozen) kernel_events(ring_events[s], 0);
    if (hold_wanted && !holding) hold_others();
    if (frozen) kernel_thaw();
}

static void ring_hash(u64 *out) {   /* (with every thread held) */
    u64 busy = 0;
    int frozen = freeze_all(0, 1000, &busy);
    for (u32 i = 0; i < nhot; i++) out[i] = hot_ok[i] && !hot_host[i] ? page_hash(P(u8, hot[i])) : 0;
    if (frozen) kernel_thaw();
}

/* Frames from..to-1 again: each one's simulation, then the next one's input step - back to the start of frame to's
 * simulation. */
static double ring_replay(Cpu *c, u64 from, u64 to) {
    u64 t0 = rt_now_ns();
    Cpu now = *c;
    hook = HOOK_PLAY;
    for (u64 k = from; k < to; k++) {
        RecFrame *f = &rec[k % (u64)ring_n];
        start_frame((u32)(k % (u64)ring_n));
        *c = f->sim_cpu;
        f_71037246a0_orig(c);
        if (f->have_input) { *c = f->input_cpu; f_7103667300_orig(c); }
        for (u32 t = 0; t < f->ntail; t++) { *c = f->tail_cpu[t]; tail_fn(f->tail_which[t])(c); }   /* (the job system's steps the main loop takes next) */
    }
    *c = now;
    return (rt_now_ns() - t0) / 1e6;
}

static void ring_compare(u32 round, const char *what, const u64 *a, const u64 *b, u32 n) {
    u32 alias = 0, data = 0, nfirst = 0;
    u64 first[6];
    DWORD first_writer[6];
    for (u32 i = 0; i < n; i++) {
        if (!a[i] || !b[i] || a[i] == b[i]) continue;
        if (hot[i] >= ALIAS_BASE && hot[i] < ALIAS_BASE + ALIAS_SIZE) alias++; else data++;
        if (nfirst < 6) { first[nfirst] = hot[i]; first_writer[nfirst++] = hot_writer[i]; }
    }
    char line[1024];
    int k = snprintf(line, sizeof line, "[rbtest] rollback %u RESULT %s: ", round, what);
    if (!alias && !data) snprintf(line + k, sizeof line - k, "IDENTICAL (%u pages compared)", n);
    else {
        k += snprintf(line + k, sizeof line - k, "DIFFERENT in %u of %u pages (%u of the game's memory, %u of the modules' globals); the first:", alias + data, n, alias, data);
        for (u32 i = 0; i < nfirst; i++)
            k += snprintf(line + k, sizeof line - k, " %#llx (written by thread %lu%s, guest %lld)", (unsigned long long)first[i], (unsigned long)first_writer[i],
                          first_writer[i] == sim_thread ? ": the main one" : "", guest_of(first_writer[i]));
    }
    fprintf(stderr, "%s\n", line);
}

/* A rollback stuck for 5 s: where the main thread is, and every guest thread's state (once) */
static volatile u64 ring_busy_since;
void kernel_thread_states(void (*line)(const char *text));   /* (kernel.c) */
static void watchdog_line(const char *text) { fputs(text, stderr); }
static DWORD WINAPI ring_watchdog(void *unused) {
    (void)unused;
    HANDLE main = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, sim_thread);
    for (int told = 0;;) {
        Sleep(500);
        u64 since = ring_busy_since;
        if (!since || told || rt_now_ns() - since < 5000000000ull) continue;
        told = 1;
        fprintf(stderr, "[rbtest] rollback stuck for 5 s: the main thread is at\n");
        if (main) rt_backtrace_thread(main, 20);
        fprintf(stderr, "[rbtest] and the guest threads (if the kernel's lock is free):\n");
        kernel_thread_states(watchdog_line);
        fflush(stderr);
    }
    return 0;
}

static void ring_round_done(void) {
    next_round_ns = rt_now_ns() + ring_gap_ns;
    if (++round_i == ROUNDS) {
        ring_on = 0;
        cow_end();
        hook = HOOK_OFF;
        phase = DONE;
        fprintf(stderr, "[rbtest] done\n");
    }
}

static int natural_same(const Cpu *then, const Cpu *c) {   /* the main loop at the same place (x20, its clock, let through) */
    if (then->sp != c->sp || then->x[30] != c->x[30]) return 0;
    for (int r = 19; r < 30; r++) if (r != 20 && then->x[r] != c->x[r]) return 0;
    return 1;
}

static void ring_rollback(Cpu *c) {
    if (ring_natural && !natural_same(&rec[plan_from % (u64)ring_n].sim_cpu, c)) {
        /* (the main loop calls the simulation from more than one place - twice in a pass to catch up: back to the nearest save it
         * took from the same place as now instead) */
        u64 best = ~0ull;
        for (u64 k = ring_frame_no > (u64)ring_n - 1 ? ring_frame_no - (u64)ring_n + 1 : 0; k < ring_frame_no; k++)
            if (ring_frame_of[k % (u64)ring_n] == k + 1 && natural_same(&rec[k % (u64)ring_n].sim_cpu, c) &&
                (best == ~0ull || (k > plan_from ? k - plan_from : plan_from - k) < (best > plan_from ? best - plan_from : plan_from - best)))
                best = k;
        if (best != ~0ull && best != plan_from) { plan_from = best; plan_n = 0; plan_fib_n = 0; }
    }
    u64 now = ring_frame_no, from = plan_from;
    u32 d = (u32)(now - from);
    int s_now = (int)(now % (u64)ring_n), s_from = (int)(from % (u64)ring_n);
    if (d > (u32)ring_n - 1 || ring_frame_of[s_from] != from + 1) { plan_at = 0; return; }   /* (gone from the ring, or not all held then: planned again) */
    if (ring_frame_of[s_now] != now + 1) return;   /* (not all held now: the next frame) */
    for (u64 k = from; k < now; k++) if (!rec[k % (u64)ring_n].have_input) return;   /* (a frame without its input step: the next one) */
    {   /* every other thread at the same place as at that save (wait, fiber, registers) - else not safe: the next frame */
        static u32 tries;
        u64 busy = 0;
        memcpy(freeze_keys, ring_keys[s_from], ring_nkeys[s_from] * sizeof *freeze_keys);
        nfreeze_keys = ring_nkeys[s_from];
        if (!freeze_all(1, 5, &busy)) {
            static int shown;
            if (shown < 6 && freeze_all(0, 50, &busy)) {   /* (what differs in the helpers' registers) */
                shown++;
                for (int h = 0; h < 4; h++) {
                    u64 r[32];
                    if (!kernel_regs(ring_reg_ids[h], r)) continue;
                    char line[1024];
                    int k = snprintf(line, sizeof line, "[rbtest] rollback %u: guest thread %llu's registers %u frames after:", round_i + 1, (unsigned long long)ring_reg_ids[h], d);
                    int any = 0;
                    for (int x = 0; x < 32; x++)
                        if (r[x] != ring_regs[s_from][h][x] && k < (int)sizeof line - 64) {
                            k += snprintf(line + k, sizeof line - k, " %s%d %#llx->%#llx", x == 31 ? "sp" : "x", x == 31 ? 0 : x,
                                          (unsigned long long)ring_regs[s_from][h][x], (unsigned long long)r[x]);
                            any = 1;
                        }
                    fprintf(stderr, "%s%s\n", line, any ? "" : " the same");
                }
                static u64 w[64 * 6];
                u32 nw = kernel_waits(w, 64);
                for (u32 i = 0; i < nw; i++)
                    for (u32 j = 0; j < ring_nwaits[s_from]; j++)
                        if ((w[6 * i] & 0xFFFFFFFFFFFFull) == (ring_waits[s_from][6 * j] & 0xFFFFFFFFFFFFull) && memcmp(w + 6 * i + 1, ring_waits[s_from] + 6 * j + 1, 5 * 8)) {
                            show_wait("now", w + 6 * i);
                            show_wait("then", ring_waits[s_from] + 6 * j);
                        }
                kernel_thaw();
            }
            if (++tries < 600) return;
            fprintf(stderr, "[rbtest] rollback %u: never safe in 600 frames (a thread never at the same place as %u frames before; the last: guest thread %llu): skipped\n",
                    round_i + 1, d, (unsigned long long)busy);
            tries = 0;
            plan_at = 0;
            ring_round_done();
            return;
        }
        kernel_thaw();
        if (tries) fprintf(stderr, "[rbtest] rollback %u: safe after %u frames\n", round_i + 1, tries);
        tries = 0;
    }
    ring_unsafe = 0;
    { u32 act[4]; kernel_activity(act); }
    ipc_note = 1;
    static int watching;
    if (!watching++) CloseHandle(CreateThread(NULL, 0, ring_watchdog, NULL, 0, NULL));
    ring_busy_since = rt_now_ns();
    u32 n = nhot;
    if (ring_natural) {
        /* the main loop at the same place as at that save (the registers it keeps across the call, and its stack) - else not safe */
        const Cpu *then = &rec[s_from].sim_cpu;
        int same = then->sp == c->sp && then->x[30] == c->x[30];
        for (int r = 19; r < 30 && same; r++) same = r == 20 || then->x[r] == c->x[r];   /* (x20: the main loop's clock, kept in the PC's
                                                                                           * own variables - it cannot go back; let through) */
        if (!same) {
            static int told;
            if (told++ < 5) {
                fprintf(stderr, "[rbtest] rollback %u: the main loop is not where it was at frame %llu's save:", round_i + 1, (unsigned long long)from);
                for (int r = 19; r < 31; r++) if (then->x[r] != c->x[r]) fprintf(stderr, " x%d %#llx->%#llx", r, (unsigned long long)then->x[r], (unsigned long long)c->x[r]);
                if (then->sp != c->sp) fprintf(stderr, " sp %#llx->%#llx", (unsigned long long)then->sp, (unsigned long long)c->sp);
                fprintf(stderr, "\n");
            }
            ipc_note = 0;
            ring_busy_since = 0;
            return;   /* (the next frame) */
        }
        fprintf(stderr, "[rbtest] rollback %u: going back %u frames, from frame %llu; the main loop plays them again with the right buttons\n", round_i + 1, d,
                (unsigned long long)now);
        fflush(stderr);
        hold_wanted = 1;
        ring_put_back(s_from);
        if (plan_n) {
            static u64 back[MAX_HOT];
            ring_hash(back);
            char what[128];
            snprintf(what, sizeof what, "frame %llu's save put back vs that frame as it was (must be identical)", (unsigned long long)from);
            ring_compare(round_i + 1, what, plan_expect, back, plan_n);
        }
        if (holding) { InterlockedExchange(&holding, 0); kernel_release(); }
        hold_wanted = 0;
        ipc_note = 0;
        *c = *then;
        natural_until = now;
        natural_t0 = rt_now_ns();
        natural_started = 1;
        ring_frame_no = from;
        pad_replayed = pad_missing = tick_replayed = tick_missing = rnd_replayed = rnd_missing = 0;
        hook = HOOK_PLAY;
        start_frame((u32)s_from);
        plan_at = 0;
        ring_busy_since = 0;
        return;
    }
    fprintf(stderr, "[rbtest] rollback %u: going back %u frames, from frame %llu\n", round_i + 1, d, (unsigned long long)now);
    fflush(stderr);
    ring_hash(ring_sum[0]);
    {   /* (the fibers too, with every thread held) */
        u64 busy = 0;
        int fz = freeze_all(0, 1000, &busy);
        fib_before_n = fibers_hash(fib_before, 16384);
        if (fz) kernel_thaw();
    }
    ring_tlr_hash = 0;
    for (u64 i = 0; i < tlr_size; i += 8) ring_tlr_hash = (ring_tlr_hash ^ *P(u64, KERNEL_BASE + i)) * 0x100000001B3ull;
    u64 woken_id;
    kernel_held_woken(&woken_id);
    hold_wanted = 1;
    nguest = 0;
    kernel_thread_each(note_guest, NULL);
    double ms[4] = {0};
    pads_altered = 0;
    ring_wait_ns = 0;
    for (int r = 1; r <= 3; r++) {
        ring_put_back(s_from);
        if (r == 1 && plan_n) {   /* (the save as it was when it was taken: is going back to it exact?) */
            static u64 back[MAX_HOT];
            ring_hash(back);
            char what[128];
            snprintf(what, sizeof what, "frame %llu's save put back vs that frame as it was (must be identical)", (unsigned long long)from);
            ring_compare(round_i + 1, what, plan_expect, back, plan_n);
            static u64 fib_now[3 * 16384];
            u64 busy = 0;
            int fz = freeze_all(0, 1000, &busy);
            u32 nf = fibers_hash(fib_now, 16384), bad = 0;
            if (fz) kernel_thaw();
            for (u32 i = 0; i < nf && i < plan_fib_n; i++)
                for (int k = 0; k < 3; k++)
                    if (fib_now[3 * i + k] != plan_fib[3 * i + k]) {
                        if (bad++ < 8) fprintf(stderr, "[rbtest] rollback %u: fiber %u differs from that save in its %s\n", round_i + 1, i, k == 0 ? "slot" : k == 1 ? "guest context" : "host stack");
                        break;
                    }
            fprintf(stderr, "[rbtest] rollback %u RESULT frame %llu's fibers put back vs as they were: %s (%u then, %u now, %u differ)\n", round_i + 1,
                    (unsigned long long)from, !bad && nf == plan_fib_n ? "IDENTICAL" : "DIFFERENT", plan_fib_n, nf, bad);
        }
        pad_alter = r == 2;
        nrng_calls[r - 1] = 0;
        rng_replay = r;
        ms[r] = ring_replay(c, from, now);
        rng_replay = 0;
        pad_alter = 0;
        ring_hash(ring_sum[r]);
    }
    ipc_note = 0;
    u32 act[4];
    kernel_activity(act);
    fprintf(stderr, "[rbtest] rollback %u: during the replays %u service requests, %u handles made, %u closed (none of it rewound), %u unmappings (kept, cleared)\n", round_i + 1, act[0], act[1], act[2], act[3]);
    if (!ring_continue) {
        ring_put_back(s_now);
        static u64 after[MAX_HOT];   /* (the game's state given back: as it was before the rollback, page by page?) */
        ring_hash(after);
        ring_compare(round_i + 1, "the game before the rollback vs after it (must be identical)", ring_sum[0], after, n);
        u64 tlr_after = 0;
        for (u64 i = 0; i < tlr_size; i += 8) tlr_after = (tlr_after ^ *P(u64, KERNEL_BASE + i)) * 0x100000001B3ull;
        {   /* the fibers: as before? */
            static u64 fib_after[3 * 16384];
            u64 busy = 0;
            int fz = freeze_all(0, 1000, &busy);
            u32 na = fibers_hash(fib_after, 16384), bad = 0;
            if (fz) kernel_thaw();
            for (u32 i = 0; i < na && i < fib_before_n; i++)
                for (int k = 0; k < 3; k++)
                    if (fib_after[3 * i + k] != fib_before[3 * i + k]) {
                        if (bad++ < 8) fprintf(stderr, "[rbtest] rollback %u: fiber %u differs after it in its %s\n", round_i + 1, i, k == 0 ? "slot" : k == 1 ? "guest context" : "host stack");
                        break;
                    }
            fprintf(stderr, "[rbtest] rollback %u RESULT the fibers before the rollback vs after it: %s (%u fibers before, %u after, %u differ)\n", round_i + 1,
                    !bad && na == fib_before_n ? "IDENTICAL" : "DIFFERENT", fib_before_n, na, bad);
        }
        if (tlr_after != ring_tlr_hash) fprintf(stderr, "[rbtest] rollback %u: the thread-local regions differ from before the rollback\n", round_i + 1);
    }
    u32 woken = kernel_held_woken(&woken_id);
    if (woken) fprintf(stderr, "[rbtest] rollback %u: %u wake-ups of held threads while replaying (the last: guest thread %llu): undone at each put-back\n",
                       round_i + 1, woken, (unsigned long long)woken_id);
    if (holding) { InterlockedExchange(&holding, 0); kernel_release(); }
    hold_wanted = 0;
    hook = HOOK_RECORD;
    start_frame((u32)s_now);
    fprintf(stderr, "[rbtest] rollback %u: back %u frames (from frame %llu to %llu), replayed in %.1f, %.1f and %.1f ms (%.2f ms a frame); waiting for the threads at the put-backs %.1f ms; fibers running then (not put back) %u; %llu pad reads changed in replay 2; pads replayed %llu (missing %llu), ticks %llu (missing %llu)\n",
            round_i + 1, d, (unsigned long long)from, (unsigned long long)now, ms[1], ms[2], ms[3], ms[3] / d, ring_wait_ns / 1e6, ring_skipped, (unsigned long long)pads_altered,
            (unsigned long long)pad_replayed, (unsigned long long)pad_missing, (unsigned long long)tick_replayed, (unsigned long long)tick_missing);
    pad_replayed = pad_missing = tick_replayed = tick_missing = 0;
    ring_compare(round_i + 1, "replay 1 vs replay 3 (the recorded pads twice)", ring_sum[1], ring_sum[3], n);
    ring_compare(round_i + 1, "replay 1 vs replay 2 (other buttons)", ring_sum[1], ring_sum[2], n);
    ring_compare(round_i + 1, "replay 1 vs the game (which drew its frames too)", ring_sum[1], ring_sum[0], n);
    rng_compare(1, 3);
    fprintf(stderr, "[rbtest] rollback %u: the game goes on from %s\n", round_i + 1, ring_continue ? "replay 3" : "its own save");
    fflush(stderr);
    if (ring_unsafe) fprintf(stderr, "[rbtest] rollback %u: %llu put-backs with threads not at the same place as at their save\n", round_i + 1, (unsigned long long)ring_unsafe);
    ring_busy_since = 0;
    plan_at = 0;
    ring_round_done();
}

static void ring_frame(Cpu *c) {   /* (at a frame's simulation start, on the main thread) */
    if (phase == IDLE) {
        if (rt_now_ns() < next_round_ns) return;
        if (!ring_begin()) { fprintf(stderr, "[rbtest] saving every frame: out of memory: off\n"); phase = DONE; return; }
        phase = RING;
        next_round_ns = rt_now_ns() + ring_gap_ns;
    }
    if (phase != RING) return;
    if (natural_until) {   /* the main loop playing a rollback's frames again */
        ring_frame_no++;
        if (ring_frame_no < natural_until) { ring_save(c, 1); return; }
        fprintf(stderr, "[rbtest] rollback %u: back at frame %llu, live again; the frames played again in %.1f ms; pads replayed %llu (missing %llu), ticks %llu (missing %llu), random bytes %llu (missing %llu)\n",
                round_i + 1, (unsigned long long)ring_frame_no, (rt_now_ns() - natural_t0) / 1e6, (unsigned long long)pad_replayed, (unsigned long long)pad_missing,
                (unsigned long long)tick_replayed, (unsigned long long)tick_missing, (unsigned long long)rnd_replayed, (unsigned long long)rnd_missing);
        fflush(stderr);
        natural_until = 0;
        ring_round_done();
    }
    if (!plan_at && rt_now_ns() >= next_round_ns) {   /* the next rollback: back to this frame's save, d frames from now */
        u32 d = round_frames[round_i];
        if (d > (u32)ring_n - 1) d = (u32)ring_n - 1;
        plan_from = ring_frame_no;
        plan_at = plan_from + d;
        plan_n = 0;
    }
    ring_save(c, 0);
    if (plan_at && ring_frame_no >= plan_at) ring_rollback(c);
    if (natural_started) { natural_started = 0; return; }   /* (the frame number is the rollback's first now: its simulation runs next) */
    if (holding) { InterlockedExchange(&holding, 0); kernel_release(); }   /* (no rollback after all this frame) */
    hold_wanted = 0;
    ring_frame_no++;
}
#else
static int every_on(void) { return 0; }
static void ring_frame(Cpu *c) { (void)c; }
#endif

/* The pages to watch next round: the simulation's that the replays disagreed on. */
static void pick_suspects(const u64 *a, const u64 *b) {
    nsuspect = 0;
    for (u32 i = 0; i < nhot && nsuspect < MAX_SUSPECT; i++)
        if (a[i] && b[i] && a[i] != b[i] && is_sim(hot[i]) && !is_foreign(hot[i])) suspect[nsuspect++] = hot[i];
}

/* The suspect pages' hashes at a moment of a replay: how many changed since the last moment (stages). */
static u64 stage_sum[MAX_SUSPECT];
static u32 stage(int first) {
    u32 changed = 0;
    for (u32 k = 0; k < nsuspect; k++) {
        u64 h = committed(suspect[k]) ? page_hash(P(u8, suspect[k])) : 0;
        if (!first && h != stage_sum[k]) changed++;
        stage_sum[k] = h;
    }
    return changed;
}

/* A replay from the save, watched: the pages put back, then (if there are suspects) who writes them and
 * when - in the moment before the replay, during it, and after it until they are hashed. */
static double watched_replay(Cpu *c, int which, u64 *sums, u64 *replay_seen) {
    put_back(snap);
    u32 at_save = 0;
    for (u32 k = 0; k < nsuspect; k++)
        for (u32 i = 0; i < nhot; i++)
            if (hot[i] == suspect[k]) { at_save += snap_ok[i] && committed(hot[i]) && page_hash(P(u8, hot[i])) != page_hash(snap + (u64)i * PAGE); break; }
    stage(1);
    trace_begin(which);
    u32 skipped, dropped;
    int fib_restored = fibers_restore(0, &skipped, &dropped);
    if (replay_seen) { *replay_seen = 0; watch_now(replay_seen, NULL); }
    u32 before = stage(0);
    if (which >= 1 && which <= 3) { nrng_calls[which - 1] = 0; rng_replay = which; }
    double ms = replay(c);
    rng_replay = 0;
    u32 during = stage(0);
    /* (freeze mode: hashed once every thread is idle again - the simulation's helpers may still be finishing the jobs
     * it gave them) */
    u64 busy = 0;
    int frozen = freeze_on() && freeze_all(0, 1000, &busy);
    if (freeze_on() && !frozen) fprintf(stderr, "[rbtest] replay %d: the threads not idle in 1 s to hash (guest thread %llu busy): hashed anyway\n", which, (unsigned long long)busy);
    hash_pages(sums);
    if (which == 1)
        for (u32 i = 0; i < nhot; i++)
            if (sums[i]) memcpy(snap_one + (u64)i * PAGE, P(void, hot[i]), PAGE);
    if (frozen) kernel_thaw();
    u32 after = stage(0);
    trace_end();
    fprintf(stderr, "[rbtest] round %u, replay %d: fibers restored %d (running, skipped: %u; started since, dropped: %u); of %u suspect pages, not as saved after the put-back: %u; changed before the replay: %u, during it: %u, after it: %u; pages left out (other threads write them): %u, %u of them written by the replaying thread too\n",
            round_i + 1, which, fib_restored, skipped, dropped, nsuspect, at_save, before, during, after, nforeign, nshared);
    return ms;
}

static void check_and_replay(Cpu *c) {
    hook = HOOK_OFF;
    read_regions();
    if (watch_now(&window_seen, count_escaped) < 0) escaped = ~0ull;
    u32 slots, finalized, sim_pages = 0;
    u64 made;
    fiber_counts(&slots, &finalized, &made);
    for (u32 i = 0; i < nhot; i++) sim_pages += is_sim(hot[i]);
    fprintf(stderr, "[rbtest] round %u: recorded %u frames; pages written outside the saved ones meanwhile: %lld; of the saved ones, by the simulation: %u; fibers started meanwhile: %llu\n",
            round_i + 1, n_frames, escaped == ~0ull ? -1ll : (long long)escaped, sim_pages, (unsigned long long)(made - fibers_at_save));

    /* the game's state now, kept to give it back at the end (the test then leaves no trace) */
    hash_pages(sum_real);
    for (u32 i = 0; i < nhot; i++)
        if (snap_ok[i] && committed(hot[i])) memcpy(snap_now + (u64)i * PAGE, P(void, hot[i]), PAGE);
    if (freeze_on() && tlr_now) tlr_copy(tlr_now, 1);
    if (freeze_on()) cow_keep_now();
    u32 running = 0, skipped, dropped;
    int fib_now = fibers_save(1, &running);
    fprintf(stderr, "[rbtest] round %u: the state now kept (fibers %d, running %u); rewinding and replaying twice\n", round_i + 1, fib_now, running);
    fflush(stderr);

    /* replay 1, from the save */
    if (freeze_on()) hold_wanted = 1;
    nguest = 0;
    kernel_thread_each(note_guest, NULL);
    u64 replay_seen = 0;
    double ms1 = watched_replay(c, 1, sum_one, &replay_seen);
    u64 escaped_before = escaped;
    escaped = 0;
    if (watch_now(&replay_seen, count_escaped) < 0) escaped = ~0ull;
    u64 escaped_replay = escaped;
    escaped = escaped_before;
    u64 pads1 = pad_replayed, pads_missing1 = pad_missing, ticks1 = tick_replayed, ticks_missing1 = tick_missing;

    /* replay 2, from the save again (freeze mode: with other buttons) */
    pad_alter = freeze_on();
    pads_altered = 0;
    double ms2 = watched_replay(c, 2, sum_two, NULL);
    pad_alter = 0;
    if (freeze_on()) fprintf(stderr, "[rbtest] round %u: replay 2 played with other buttons (%llu pad reads changed); replay 3 with the recorded ones again\n",
                             round_i + 1, (unsigned long long)pads_altered);
    /* (freeze mode) and a third: the same as the second (something outside the saved state set once, by the first), or
     * as the first (something flipping every time)? */
    int third = freeze_on();
    if (third) watched_replay(c, 3, sum_three, NULL);

    /* the game's state back as it was */
    put_back(snap_now);
    if (freeze_on()) cow_end();
    if (holding) {
        InterlockedExchange(&holding, 0);
        kernel_release();
        fprintf(stderr, "[rbtest] round %u: %ld other threads were held while replaying\n", round_i + 1, (long)held_count);
    }
    hold_wanted = 0;
    int fib_back = fibers_restore(1, &skipped, &dropped);

    char line[1024];
    snprintf(line, sizeof line, "[rbtest] round %u (%u frames): replayed in %.1f and %.1f ms (%.2f ms a frame); pads replayed %llu (missing %llu), ticks replayed %llu (missing %llu); pages written outside the saved ones while replaying: %lld",
             round_i + 1, n_frames, ms1, ms2, ms2 / n_frames, (unsigned long long)pads1, (unsigned long long)pads_missing1,
             (unsigned long long)ticks1, (unsigned long long)ticks_missing1, escaped_replay == ~0ull ? -1ll : (long long)escaped_replay);
    snprintf(line + strlen(line), sizeof line - strlen(line), "; random bytes recorded %llu, replayed %llu (missing %llu); ticks the helpers read %llu", (unsigned long long)rnd_recorded,
             (unsigned long long)rnd_replayed, (unsigned long long)rnd_missing, (unsigned long long)helper_ticks);
    helper_ticks = 0;
    rnd_recorded = rnd_replayed = rnd_missing = 0;
    fprintf(stderr, "%s\n", line);
    compare("replay 1 vs replay 2 (the same frames twice from the same save)", sum_one, sum_two, line, sizeof line);
    fprintf(stderr, "[rbtest] round %u RESULT %s\n", round_i + 1, line);
    rng_compare(1, 2);
    if (third) {
        rng_compare(2, 3);
        compare("replay 2 vs replay 3", sum_two, sum_three, line, sizeof line);
        fprintf(stderr, "[rbtest] round %u RESULT %s\n", round_i + 1, line);
        compare("replay 1 vs replay 3", sum_one, sum_three, line, sizeof line);
        fprintf(stderr, "[rbtest] round %u RESULT %s\n", round_i + 1, line);
    }
    compare("replay 1 vs the game (which also drew its frames)", sum_one, sum_real, line, sizeof line);
    fprintf(stderr, "[rbtest] round %u RESULT %s\n", round_i + 1, line);
    show_words(sum_one, sum_two);
    fprintf(stderr, "[rbtest] round %u: the game's state given back (fibers %d)\n", round_i + 1, fib_back);
    /* and who writes the suspect pages in the game's own next frame (ended at its simulation's start) */
    pick_suspects(sum_one, sum_two);   /* (freeze mode: traced to see who writes them, but nothing is left out) */
    if (round_i + 1 < ROUNDS) trace_begin(3);

    phase = IDLE;
    next_round_ns = rt_now_ns() + ROUND_GAP_NS;
    if (++round_i == ROUNDS) { phase = DONE; fprintf(stderr, "[rbtest] done\n"); }
}

/* At the start of a frame's simulation (the outermost call, on the game's main thread). */
static void frame(Cpu *c) {
    if (!sim_thread) sim_thread = GetCurrentThreadId();
    if (!on_sim_thread()) return;
    if (every_on()) { ring_frame(c); return; }
    if (trace_active()) trace_end();   /* (the game's frame after a round: watched until here) */
    switch (phase) {
    case IDLE:
        if (rt_now_ns() >= next_round_ns) begin_round();
        break;
    case DISCOVER:
        read_regions();
        if (gfx_watch_pages(alias_base, alias_size, nalias, &discover_seen, mark_hot, NULL) == -2) {
            fprintf(stderr, "[rbtest] the alias area cannot be watched on this system: off\n");
            phase = DONE;
            break;
        }
        if (++frames_in_phase >= DISCOVER_FRAMES) {
            static int census_done;
            if (census_done++ || !census_possible()) save(c);
            else {
                fprintf(stderr, "[rbtest] round %u: census of who writes the pages (%u frames)\n", round_i + 1, CENSUS_FRAMES);
                fflush(stderr);
                census_begin();
                frames_in_phase = 0;
                phase = CENSUS;
            }
        }
        break;
    case CENSUS:
        if (++frames_in_phase >= CENSUS_FRAMES) {
            census_end();
            save(c);
        } else census_span(1);
        break;
    case RECORD:
        if (rec_next == n_frames) check_and_replay(c);   /* (frame N: its input step is recorded, its simulation not run yet) */
        else rec[cur].sim_cpu = *c;
        break;
    }
}

/* The main loop's steps after the input step, before the next simulation (ring mode: replayed with it - the frame's jobs are
 * kicked and closed there; a replay without them left jobs half done): 71035493e0 and 710354c990, in the job system, and
 * 710375e690 (SSBU_RB_TAIL=7: the three; default 6: the two of the job system). Recorded on the main thread, outside the
 * simulation. */
GuestFn f_710375e690_orig, f_71035493e0_orig, f_710354c990_orig;
static GuestFn *tail_fn_of(int which) { return which == 0 ? &f_710375e690_orig : which == 1 ? &f_71035493e0_orig : &f_710354c990_orig; }
static void tail_call(Cpu *c, int which) {
    static int mask = -1;
    if (mask < 0) mask = getenv("SSBU_RB_TAIL") ? atoi(getenv("SSBU_RB_TAIL")) : 6;
    if (rbt_on() && phase == RING && hook == HOOK_RECORD && on_sim_thread() && !depth && (mask >> which & 1)) {
        RecFrame *f = &rec[cur];
        if (f->ntail < 4) { f->tail_which[f->ntail] = (u8)which; f->tail_cpu[f->ntail++] = *c; }
    }
    (*tail_fn_of(which))(c);
}
/* signature: d10283ff 6d022beb 6d0323e9 a9046ffc a90567fa a9065ff8 a90757f6 a9084ff4 a9097bfd 910243fd .0.....8 f9....08 f9....00 9....... .0.....8 f9....08 */
HLE_WRAP(710375e690) { tail_call(c, 0); }
/* signature: a9bb67fa a9015ff8 a90257f6 a9034ff4 a9047bfd 910103fd b9401008 34.....8 a9447bfd a9434ff4 a94257f6 a9415ff8 a8c567fa d65f03c0 2a0303f4 aa0003f3 */
HLE_WRAP(71035493e0) { tail_call(c, 1); }
/* signature: d10183ff a90167fa a9025ff8 a90357f6 a9044ff4 a9057bfd 910143fd aa0103f4 3100041f 54.....0 2a0003e0 9....... aa0003f3 1....... aa1f03f3 b9400288 */
HLE_WRAP(710354c990) { tail_call(c, 2); }

/* The game reads the pads (7103667300, called by the main loop once a frame). */
/* signature: fc1a0fe8 a90167fa a9025ff8 a90357f6 a9044ff4 a9057bfd 910143fd .0.....8 91....08 08dffd08 360....8 .0.....8 39....08 34.....8 .0.....8 b9....08 */
HLE_WRAP(7103667300) {
    if (lks_on() && !rbt_on()) lks_input_step();   /* (netplay: the frame's pads) */
    if (rbt_on() && phase == RECORD && hook == HOOK_RECORD && on_sim_thread() && rec_next <= n_frames) {
        start_frame(rec_next);
        rec[rec_next].input_cpu = *c;
        rec[rec_next].have_input = 1;
    } else if (rbt_on() && phase == RING && hook == HOOK_RECORD && on_sim_thread()) {   /* (the frame's record goes on) */
        rec[cur].input_cpu = *c;
        rec[cur].have_input = 1;
    }
    f_7103667300_orig(c);
}

/* The game simulates a frame (71037246a0, called by the main loop once a frame, and by itself). */
/* signature: d10243ff a9036ffc a90467fa a9055ff8 a90657f6 a9074ff4 a9087bfd 910203fd b9403008 aa0003f3 7100051f 54.....0 7100091f 54.....1 aa1303f4 f8438e80 */
HLE_WRAP(71037246a0) {
    if (lks_on() && !rbt_on() && !depth) {   /* (netplay's check: what the frame's simulation writes) */
        depth++;
        lks_sim_begin();
        f_71037246a0_orig(c);
        lks_sim_end();
        depth--;
        return;
    }
    if (!rbt_on() || phase == DONE || depth) { f_71037246a0_orig(c); return; }
    depth++;
    frame(c);
    int recording = phase == RECORD && hook == HOOK_RECORD && on_sim_thread();
    if (recording) watch_now(&sim_seen, NULL);       /* (what the simulation writes: from here...) */
    f_71037246a0_orig(c);
    if (recording) watch_now(&sim_seen, mark_sim);   /* (... to here) */
    if (phase == CENSUS && on_sim_thread()) census_span(0);
    if (phase == RECORD && on_sim_thread()) rec_next = cur + 1;
    depth--;
}
