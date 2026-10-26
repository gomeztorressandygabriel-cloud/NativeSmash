/* Rollback rewind test: can the game's simulation be saved, rewound and replayed to the same state?
 * (docs/ROLLBACK.md, phase 3: the gate the whole rollback project depends on.)
 *
 * Off unless SSBU_RB_TEST=1 or a file rollback_test.on exists beside the game. On, from
 * SSBU_RB_TEST_AT seconds after start (default 200: in a battle, on a slow PC with the scripted
 * battle of tools/dev/autobattle.sh), it runs six rounds, ten seconds apart, of N = 1, 1, 5, 10, 30
 * and 60 frames:
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
#define DISCOVER_FRAMES 120
#define MAX_LATE 8192              /* pages found written outside the hot set, added to it for the next round */
#define MAX_N 60
#define MAX_PADS 16                /* pad reads kept per frame */
#define PAD_BYTES 0x34
#define MAX_TICKS 1024             /* tick reads kept per frame */
#define ROUND_GAP_NS 10000000000ull

static const u32 round_frames[] = {1, 1, 5, 10, 30, 60};
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
static u8 *snap_now;                   /* the game's state at the end of the window, given back after the test */
static u64 sum_real[MAX_HOT], sum_one[MAX_HOT], sum_two[MAX_HOT];
static u8 *sim_bits;                   /* one bit an alias page: written while the simulation ran, in the window */
static u64 sim_seen;
static u64 late[MAX_LATE];             /* (pages a round's save missed: the next round's hot set has them) */
static u32 nlate;
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
    if (!snap_now) snap_now = malloc(MAX_HOT * PAGE);
    if (!snap_one) snap_one = malloc(MAX_HOT * PAGE);
    if (!sim_bits) sim_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!hot_bits || !snap || !snap_now || !snap_one || !sim_bits) { fprintf(stderr, "[rbtest] out of memory: off\n"); phase = DONE; return; }
    /* the hot set grows from round to round: what the last rounds found, and what their saves missed */
    for (u32 i = 0; i < nlate; i++) mark_hot(late[i], NULL);
    nlate = 0;
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
    for (u32 i = 0; i < nhot; i++)
        if (snap_ok[i] && committed(hot[i])) memcpy(P(void, hot[i]), from + (u64)i * PAGE, PAGE);
}

/* The recorded frames again, from the start of the window: the simulation of frame 0, then each next
 * frame's input step and simulation, then frame N's input step - back to where the game was when it
 * called this frame's simulation. The drawing and the other steps of the main loop are not run. */
static double replay(Cpu *c) {
    u64 t0 = rt_now_ns();
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
        if (!a[i] || !b[i]) continue;
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
        if (!a[i] || !b[i] || a[i] == b[i] || !is_sim(hot[i])) continue;
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
    u32 running = 0, skipped, dropped;
    int fib_now = fibers_save(1, &running);
    fprintf(stderr, "[rbtest] round %u: the state now kept (fibers %d, running %u); rewinding and replaying twice\n", round_i + 1, fib_now, running);
    fflush(stderr);

    /* replay 1, from the save */
    put_back(snap);
    int fib_restored = fibers_restore(0, &skipped, &dropped);
    fprintf(stderr, "[rbtest] round %u: fibers restored %d (running, skipped: %u; started since, dropped: %u)\n", round_i + 1, fib_restored, skipped, dropped);
    fflush(stderr);
    u64 replay_seen = 0;
    watch_now(&replay_seen, NULL);
    double ms1 = replay(c);
    u64 escaped_before = escaped;
    escaped = 0;
    if (watch_now(&replay_seen, count_escaped) < 0) escaped = ~0ull;
    u64 escaped_replay = escaped;
    escaped = escaped_before;
    hash_pages(sum_one);
    for (u32 i = 0; i < nhot; i++)
        if (sum_one[i]) memcpy(snap_one + (u64)i * PAGE, P(void, hot[i]), PAGE);
    u64 pads1 = pad_replayed, pads_missing1 = pad_missing, ticks1 = tick_replayed, ticks_missing1 = tick_missing;

    /* replay 2, from the save again */
    put_back(snap);
    fibers_restore(0, &skipped, &dropped);
    double ms2 = replay(c);
    hash_pages(sum_two);

    /* the game's state back as it was */
    put_back(snap_now);
    int fib_back = fibers_restore(1, &skipped, &dropped);

    char line[1024];
    snprintf(line, sizeof line, "[rbtest] round %u (%u frames): replayed in %.1f and %.1f ms (%.2f ms a frame); pads replayed %llu (missing %llu), ticks replayed %llu (missing %llu); pages written outside the saved ones while replaying: %lld",
             round_i + 1, n_frames, ms1, ms2, ms2 / n_frames, (unsigned long long)pads1, (unsigned long long)pads_missing1,
             (unsigned long long)ticks1, (unsigned long long)ticks_missing1, escaped_replay == ~0ull ? -1ll : (long long)escaped_replay);
    fprintf(stderr, "%s\n", line);
    compare("replay 1 vs replay 2 (the same frames twice from the same save)", sum_one, sum_two, line, sizeof line);
    fprintf(stderr, "[rbtest] round %u RESULT %s\n", round_i + 1, line);
    compare("replay 1 vs the game (which also drew its frames)", sum_one, sum_real, line, sizeof line);
    fprintf(stderr, "[rbtest] round %u RESULT %s\n", round_i + 1, line);
    show_words(sum_one, sum_two);
    fprintf(stderr, "[rbtest] round %u: the game's state given back (fibers %d)\n", round_i + 1, fib_back);

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
    int recording = phase == RECORD && hook == HOOK_RECORD && on_sim_thread();
    if (recording) watch_now(&sim_seen, NULL);       /* (what the simulation writes: from here...) */
    f_71037246a0_orig(c);
    if (recording) watch_now(&sim_seen, mark_sim);   /* (... to here) */
    if (phase == RECORD && on_sim_thread()) rec_next = cur + 1;
    depth--;
}
