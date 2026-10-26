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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_rbtest.h"
#include "net_lockstep.h"

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
#define CENSUS_FRAMES 10           /* (Windows) frames the census of who writes the pages lasts */
#define MAX_LATE 8192              /* pages found written outside the hot set, added to it for the next round */
#define MAX_N 60
#define MAX_PADS 16                /* pad reads kept per frame */
#define PAD_BYTES 0x34
#define MAX_TICKS 1024             /* tick reads kept per frame */
#define ROUND_GAP_NS 10000000000ull

static const u32 round_frames[] = {1, 1, 1, 5, 10};
#define ROUNDS (sizeof round_frames / sizeof *round_frames)

typedef struct RecFrame {
    Cpu input_cpu, sim_cpu;        /* the contexts the input step and the simulation were called with */
    int have_input;
    u8 pad[MAX_PADS][PAD_BYTES];
    u32 pad_size[MAX_PADS], npad;
    u64 ticks[MAX_TICKS];
    u32 nticks;
} RecFrame;

enum { IDLE, DISCOVER, CENSUS, RECORD, DONE };
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

static int is_foreign(u64 page) {
    u64 i = (page - ALIAS_BASE) / PAGE;
    return page >= ALIAS_BASE && page < ALIAS_BASE + ALIAS_SIZE && foreign[i >> 3] >> (i & 7) & 1;
}

static void mark_foreign(u64 page) {
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
    if (!foreign) foreign = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!sim_bits) sim_bits = calloc(ALIAS_SIZE / PAGE / 8, 1);
    if (!hot_bits || !snap || !snap_now || !snap_one || !sim_bits || !foreign) { fprintf(stderr, "[rbtest] out of memory: off\n"); phase = DONE; return; }
    /* the hot set grows from round to round: what the last rounds found, and what their saves missed */
    for (u32 i = 0; i < nlate; i++) mark_hot(late[i], NULL);
    nlate = 0;
    read_regions();
    discover_seen = 0;
    if (watch_now(&discover_seen, NULL) < 0) return;   /* (only to bring the clock to now; tried again next frame) */
    n_frames = round_frames[round_i];
    frames_in_phase = 0;
    phase = DISCOVER;
    fprintf(stderr, "[rbtest] round %u: finding the pages the game writes (%u frames)\n", round_i + 1, DISCOVER_FRAMES);
    fflush(stderr);
}

static void save(Cpu *c) {
    fprintf(stderr, "[rbtest] round %u: saving\n", round_i + 1);
    fflush(stderr);
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
        if (snap_ok[i] && committed(hot[i]) && !is_foreign(hot[i])) memcpy(P(void, hot[i]), from + (u64)i * PAGE, PAGE);
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
static int census_possible(void) { return 1; }
#else
static int census_possible(void) { return 0; }
static void census_begin(void) {}
static void census_span(int in_sim) { (void)in_sim; }
static void census_end(void) {}
static void trace_begin(int which) { (void)which; }
static void trace_end(void) {}
static int trace_active(void) { return 0; }
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
    double ms = replay(c);
    u32 during = stage(0);
    hash_pages(sums);
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
    u32 running = 0, skipped, dropped;
    int fib_now = fibers_save(1, &running);
    fprintf(stderr, "[rbtest] round %u: the state now kept (fibers %d, running %u); rewinding and replaying twice\n", round_i + 1, fib_now, running);
    fflush(stderr);

    /* replay 1, from the save */
    nguest = 0;
    kernel_thread_each(note_guest, NULL);
    u64 replay_seen = 0;
    double ms1 = watched_replay(c, 1, sum_one, &replay_seen);
    u64 escaped_before = escaped;
    escaped = 0;
    if (watch_now(&replay_seen, count_escaped) < 0) escaped = ~0ull;
    u64 escaped_replay = escaped;
    escaped = escaped_before;
    for (u32 i = 0; i < nhot; i++)
        if (sum_one[i]) memcpy(snap_one + (u64)i * PAGE, P(void, hot[i]), PAGE);
    u64 pads1 = pad_replayed, pads_missing1 = pad_missing, ticks1 = tick_replayed, ticks_missing1 = tick_missing;

    /* replay 2, from the save again */
    double ms2 = watched_replay(c, 2, sum_two, NULL);

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
    /* and who writes the suspect pages in the game's own next frame (ended at its simulation's start) */
    pick_suspects(sum_one, sum_two);
    if (round_i + 1 < ROUNDS) trace_begin(3);

    phase = IDLE;
    next_round_ns = rt_now_ns() + ROUND_GAP_NS;
    if (++round_i == ROUNDS) { phase = DONE; fprintf(stderr, "[rbtest] done\n"); }
}

/* At the start of a frame's simulation (the outermost call, on the game's main thread). */
static void frame(Cpu *c) {
    if (!sim_thread) sim_thread = GetCurrentThreadId();
    if (!on_sim_thread()) return;
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

/* The game reads the pads (7103667300, called by the main loop once a frame). */
/* signature: fc1a0fe8 a90167fa a9025ff8 a90357f6 a9044ff4 a9057bfd 910143fd .0.....8 91....08 08dffd08 360....8 .0.....8 39....08 34.....8 .0.....8 b9....08 */
HLE_WRAP(7103667300) {
    if (lks_on() && !rbt_on()) lks_input_step();   /* (netplay: the frame's pads) */
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
