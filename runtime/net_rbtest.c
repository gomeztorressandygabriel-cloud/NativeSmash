/* Rollback rewind test: can the game's simulation be saved, rewound and replayed to the same state?
 * (docs/ROLLBACK.md, phase 3: the gate the whole rollback project depends on.)
 *
 * Off unless SSBU_RB_TEST=1 or a file rollback_test.on exists beside the game. On, from
 * SSBU_RB_TEST_AT seconds after start (default 200: in a battle, on a slow PC with the scripted
 * battle of tools/dev/autobattle.sh), it runs four rounds, ten seconds apart, of N = 1, 10, 30 and
 * 60 frames:
 *
 *   1. Find the pages: for 60 frames, the alias pages the game writes (the alias area holds the
 *      game's own memory: MapPhysicalMemory) - the "hot set", asked through the renderer's write
 *      watch stamps (gfx_watch_pages, as the rollback probe does).
 *   2. Save: at the start of a frame's simulation, a copy of every hot page.
 *   3. Record N frames: the game's pad reads and system ticks on its main thread, and the guest
 *      context each frame's input step and simulation are called with.
 *   4. Check: at the start of frame N's simulation, the hash of every hot page (the state the game
 *      reached), and how many pages outside the hot set were written meanwhile (the save missed them).
 *   5. Rewind and replay: copy the saved pages back, then run the recorded frames again - the
 *      simulation, then the next input step, ... - with the recorded pads and ticks, without drawing.
 *   6. Compare: the hash of every hot page again. Equal everywhere: the simulation came back to the
 *      same state. Otherwise, how many pages differ and where.
 *
 * What it does not save: host state - each fiber's host stack and context (hle_fiber.c), the
 * kernel's objects, the renderer's caches - nor what other threads (the game's workers) do. Those
 * are exactly what the test finds out about: a difference, or a crash while replaying, says the
 * save was not enough. The game carries on with the replayed state, which may look odd for a
 * moment. Every line it writes to the log starts with [rbtest].
 *
 * The frame's steps are the game's own functions, found with the rollback probe and the profiler
 * (docs/ROLLBACK.md): 7103667300 reads the pads, 71037246a0 simulates (and calls itself: only the
 * outermost call, from the main loop, is a frame), both called by the main loop 7103746e90. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_rbtest.h"

volatile int rbt_state = -1;

GuestFn f_7103667300_orig, f_71037246a0_orig;   /* (the game's own input step and simulation: HLE_WRAP below) */

#ifdef __APPLE__   /* (no Direct3D renderer there to share the alias area's watch with) */
static long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user) {
    (void)base; (void)size; (void)n; (void)seen; (void)each; (void)user;
    return -2;
}
#else
long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user);   /* (gpu_d3d11.c) */
#endif

#define PAGE 0x1000ull
#define MAX_HOT 16384              /* pages saved at most: 64 MB */
#define MAX_REGIONS 4096
#define DISCOVER_FRAMES 60
#define MAX_N 60
#define MAX_PADS 16                /* pad reads kept per frame */
#define PAD_BYTES 0x34
#define MAX_TICKS 1024             /* tick reads kept per frame */
#define ROUND_GAP_NS 10000000000ull

static const u32 round_frames[] = {1, 10, 30, 60};
#define ROUNDS (sizeof round_frames / sizeof *round_frames)

typedef struct RecFrame {
    Cpu input_cpu, sim_cpu;        /* the contexts the input step and the simulation were called with */
    int have_input;
    u8 pad[MAX_PADS][PAD_BYTES];
    u32 pad_size[MAX_PADS], npad;
    u64 ticks[MAX_TICKS];
    u32 nticks;
} RecFrame;

enum { IDLE, DISCOVER, RECORD, DONE };
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
static u64 sum_before[MAX_HOT];
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
        if (on) fprintf(stderr, "[rbtest] on: rewind test rounds of %u, %u, %u and %u frames, the first %.0f s after start\n",
                        round_frames[0], round_frames[1], round_frames[2], round_frames[3], next_round_ns / 1e9);
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

static void count_escaped(u64 page, void *user) {
    (void)user;
    if (!is_hot(page)) escaped++;
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

void rbt_pad(u64 out, u32 size) {
    if (hook == HOOK_OFF || !on_sim_thread() || size > PAD_BYTES) return;
    RecFrame *f = &rec[cur];
    if (hook == HOOK_RECORD) {
        if (f->npad < MAX_PADS) { memcpy(f->pad[f->npad], P(void, out), size); f->pad_size[f->npad++] = size; }
    } else if (pad_cursor < f->npad && f->pad_size[pad_cursor] == size) {
        memcpy(P(void, out), f->pad[pad_cursor++], size);
        pad_replayed++;
    } else pad_missing++;
}

u64 rbt_ticks(u64 now) {
    if (hook == HOOK_OFF || !on_sim_thread()) return now;
    RecFrame *f = &rec[cur];
    if (hook == HOOK_RECORD) {
        if (f->nticks < MAX_TICKS) f->ticks[f->nticks++] = now;
        return now;
    }
    if (tick_cursor < f->nticks) { tick_replayed++; return f->ticks[tick_cursor++]; }
    tick_missing++;
    return now;
}

static void start_frame(u32 k) {   /* the hooks now record into (or replay from) frame k */
    cur = k;
    pad_cursor = tick_cursor = 0;
}

/* ---- the rounds ----------------------------------------------------------------------- */
static void begin_round(void) {
    if (!hot_bits) hot_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!snap) snap = malloc(MAX_HOT * PAGE);
    if (!hot_bits || !snap) { fprintf(stderr, "[rbtest] out of memory: off\n"); phase = DONE; return; }
    memset(hot_bits, 0, ALIAS_SIZE / PAGE / 8);
    nhot = hot_overflow = 0;
    read_regions();
    discover_seen = 0;
    if (watch_now(&discover_seen, NULL) < 0) return;   /* (only to bring the clock to now; tried again next frame) */
    n_frames = round_frames[round_i];
    frames_in_phase = 0;
    phase = DISCOVER;
}

static void save(Cpu *c) {
    u64 t0 = rt_now_ns();
    read_regions();
    u32 saved = 0;
    for (u32 i = 0; i < nhot; i++) {
        snap_ok[i] = (u8)committed(hot[i]);
        if (snap_ok[i]) { memcpy(snap + (u64)i * PAGE, P(void, hot[i]), PAGE); saved++; }
    }
    window_seen = 0;
    watch_now(&window_seen, NULL);   /* (the window's writes are counted from here) */
    u32 slots, finalized;
    fiber_counts(&slots, &finalized, &fibers_at_save);
    memset(rec, 0, sizeof rec);
    escaped = pad_replayed = pad_missing = tick_replayed = tick_missing = 0;
    rec[0].sim_cpu = *c;
    rec_next = 0;
    start_frame(0);
    hook = HOOK_RECORD;
    phase = RECORD;
    fprintf(stderr, "[rbtest] round %u (%u frames): %u pages found written in %u frames (%.1f MB%s), %u saved in %.1f ms\n", round_i + 1, n_frames,
            nhot, DISCOVER_FRAMES, nhot * (double)PAGE / 1048576.0, hot_overflow ? ", more than the most kept" : "", saved, (rt_now_ns() - t0) / 1e6);
}

static void check_and_replay(Cpu *c) {
    hook = HOOK_OFF;
    read_regions();
    if (watch_now(&window_seen, count_escaped) < 0) escaped = ~0ull;
    for (u32 i = 0; i < nhot; i++) sum_before[i] = snap_ok[i] && committed(hot[i]) ? page_hash(P(u8, hot[i])) : 0;
    u32 slots, finalized;
    u64 made;
    fiber_counts(&slots, &finalized, &made);
    fprintf(stderr, "[rbtest] round %u: recorded %u frames; pages written outside the saved ones meanwhile: %lld; fibers started meanwhile: %llu; rewinding and replaying\n",
            round_i + 1, n_frames, escaped == ~0ull ? -1ll : (long long)escaped, (unsigned long long)(made - fibers_at_save));
    fflush(stderr);

    /* rewind */
    u64 t0 = rt_now_ns();
    for (u32 i = 0; i < nhot; i++)
        if (snap_ok[i] && committed(hot[i])) memcpy(P(void, hot[i]), snap + (u64)i * PAGE, PAGE);
    /* replay: the simulation of frame 0, then each next frame's input step and simulation, then
     * frame N's input step - back to where the game was when it called this frame's simulation */
    Cpu now = *c;
    hook = HOOK_PLAY;
    for (u32 k = 0; k < n_frames; k++) {
        if (k) {
            start_frame(k);
            *c = rec[k].input_cpu;
            f_7103667300_orig(c);
        }
        else start_frame(0);   /* (a frame's list holds its input step's reads, then its simulation's: go on from there) */
        *c = rec[k].sim_cpu;
        f_71037246a0_orig(c);
    }
    start_frame(n_frames);
    *c = rec[n_frames].input_cpu;
    f_7103667300_orig(c);
    hook = HOOK_OFF;
    *c = now;
    double replay_ms = (rt_now_ns() - t0) / 1e6;

    /* compare */
    read_regions();
    u32 differ = 0, compared = 0;
    u64 first[8];
    u32 nfirst = 0;
    for (u32 i = 0; i < nhot; i++) {
        if (!sum_before[i] || !committed(hot[i])) continue;
        compared++;
        if (page_hash(P(u8, hot[i])) != sum_before[i]) {
            if (nfirst < 8) first[nfirst++] = hot[i];
            differ++;
        }
    }
    char line[1024];
    int n = snprintf(line, sizeof line, "[rbtest] round %u (%u frames): replayed in %.1f ms (%.2f ms a frame); pads replayed %llu (missing %llu), ticks replayed %llu (missing %llu); RESULT: ",
                     round_i + 1, n_frames, replay_ms, replay_ms / n_frames, (unsigned long long)pad_replayed, (unsigned long long)pad_missing,
                     (unsigned long long)tick_replayed, (unsigned long long)tick_missing);
    if (!differ) n += snprintf(line + n, sizeof line - n, "IDENTICAL (%u pages compared)", compared);
    else {
        n += snprintf(line + n, sizeof line - n, "DIFFERENT in %u of %u pages, the first:", differ, compared);
        for (u32 i = 0; i < nfirst; i++) n += snprintf(line + n, sizeof line - n, " %#llx", (unsigned long long)first[i]);
    }
    fprintf(stderr, "%s\n", line);

    phase = IDLE;
    next_round_ns = rt_now_ns() + ROUND_GAP_NS;
    if (++round_i == ROUNDS) { phase = DONE; fprintf(stderr, "[rbtest] done\n"); }
}

/* At the start of a frame's simulation (the outermost call, on the game's main thread). */
static void frame(Cpu *c) {
    if (!sim_thread) sim_thread = GetCurrentThreadId();
    if (!on_sim_thread()) return;
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
        if (++frames_in_phase >= DISCOVER_FRAMES) save(c);
        break;
    case RECORD:
        if (rec_next == n_frames) check_and_replay(c);   /* (frame N: its input step is recorded, its simulation not run yet) */
        else rec[cur].sim_cpu = *c;
        break;
    }
}

/* The game reads the pads (7103667300, called by the main loop once a frame). */
/* signature: fc1a0fe8 a90167fa a9025ff8 a90357f6 a9044ff4 a9057bfd 910143fd .0.....8 91....08 08dffd08 360....8 .0.....8 39....08 34.....8 .0.....8 b9....08 */
HLE_WRAP(7103667300) {
    if (rbt_on() && phase == RECORD && hook == HOOK_RECORD && on_sim_thread() && rec_next <= n_frames) {
        start_frame(rec_next);
        rec[rec_next].input_cpu = *c;
        rec[rec_next].have_input = 1;
    }
    f_7103667300_orig(c);
}

/* The game simulates a frame (71037246a0, called by the main loop once a frame, and by itself). */
/* signature: d10243ff a9036ffc a90467fa a9055ff8 a90657f6 a9074ff4 a9087bfd 910203fd b9403008 aa0003f3 7100051f 54.....0 7100091f 54.....1 aa1303f4 f8438e80 */
HLE_WRAP(71037246a0) {
    if (!rbt_on() || phase == DONE || depth) { f_71037246a0_orig(c); return; }
    depth++;
    frame(c);
    f_71037246a0_orig(c);
    if (phase == RECORD && on_sim_thread()) rec_next = cur + 1;
    depth--;
}
