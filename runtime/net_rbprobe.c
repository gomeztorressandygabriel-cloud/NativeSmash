/* Rollback probe: what rollback netplay would have to save, restore and replay every frame,
 * measured on the real game - for build\play.log (docs/ROLLBACK.md, phase 1).
 *
 * Off unless rollback_probe.on exists in the current directory (beside "Play SSBU.bat"; the file
 * may be empty) or SSBU_RB_PROBE=1. On, kernel_init reserves the heap and the stack areas with
 * their writes watched too (the alias area always is, for the renderer), and at the end of each
 * of the game's frames - when it queues one for the display - the pages written since the last
 * frame are counted and the watch reset. That count is what a rollback would copy per frame.
 *
 * Only the memory the guest has (the kernel's heap and stack regions) is asked about, not the
 * whole reservations: on Windows a question about gigabytes of reserved memory costs
 * milliseconds even where nothing was written. The alias area - where the game's own memory
 * lives (MapPhysicalMemory) and where the renderer finds the textures the CPU changed - is asked
 * through the renderer (gfx_watch_pages), which stamps every page found written: the probe counts
 * the pages stamped since its last question, whoever took the news, and the renderer loses none.
 * When the renderer's lock is busy the question waits for the next frame.
 *
 * The alias area's pages are told apart (version 3): those inside an nvmap allocation - memory
 * the game handed the GPU (textures, vertex and constant buffers), which a rollback need not save,
 * as drawing makes it again - and the rest, the CPU's own, where the state of the match must be.
 *
 * Every 5 seconds the log gets:
 *   pages written per frame in the alias area (all; the CPU's own; handed to the GPU), the heap and
 *   the stack (median, 95th percentile, maximum, and MB), and how long finding them took;
 *   the alias area's (CPU's own pages only) and the heap's 1 MB regions written most;
 *   how often the game reads the pads per frame, and from how many threads;
 *   fibers alive (each one a host stack a rollback would have to restore);
 *   at each vsync tick, just before the game is woken: how many guest threads were running,
 *   sleeping, in a service request or waiting, and, per thread, how often it was running then;
 *   for the (up to 3) threads running at most ticks, where in the game they are: the guest
 *   functions on their stacks at that moment (the lines after "[rbprobe] thread N ... is in:").
 * The probe slows the game a little: play normally, not for timing records. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_rbprobe.h"

volatile int rbp_state = -1;

#define REPORT_NS 5000000000ull
#define MAX_FRAMES 1024           /* frames kept per report (5 s at 60 a second, and slack) */
#define REGION_SHIFT 20           /* the regions the report ranks: 1 MB */
#define BATCH 65536               /* pages taken from the watch per call */
#define TOP_REGIONS 8
#define PAD_THREADS 8
#define MAX_REGIONS 4096
#define MAX_THREADS 64

#ifdef __APPLE__   /* (no Direct3D renderer there to share the alias area's watch with: not measured) */
static long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user) {
    (void)base; (void)size; (void)n; (void)seen; (void)each; (void)user;
    return -2;
}
#else
long long gfx_watch_pages(const u64 *base, const u64 *size, u32 n, u64 *seen, void (*each)(u64 page, void *user), void *user);   /* (gpu_d3d11.c) */
#endif
u32 gpu_memory_list(u64 *addr, u64 *size, u32 max);   /* (hle_nv.c) */

static const struct { u64 base, size; const char *name; } areas[] = {
    {ALIAS_BASE, ALIAS_SIZE, "alias"}, {HEAP_BASE, HEAP_LIMIT, "heap"}, {STACK_BASE, STACK_SIZE, "stack"}};
enum { ALIAS, HEAP, STACK, AREAS };
enum { ALIAS_CPU = AREAS, ALIAS_GPU, COUNTS };   /* (the alias area's pages, told apart) */
static const char *const count_names[COUNTS] = {"alias", "heap", "stack", "alias CPU's own", "alias handed to the GPU"};

static volatile LONG busy;
static u64 report_from, report_at, page_size = 4096;
static u32 written[COUNTS][MAX_FRAMES], nframes;
static u32 *region_pages[AREAS];
static u64 find_ns_sum, find_ns_max;
static u64 region_base[MAX_REGIONS], region_size[MAX_REGIONS];
static u32 nregions;
static u64 alias_base[MAX_REGIONS], alias_size[MAX_REGIONS], alias_seen;
#define MAX_GPU_RANGES 16384
static u64 gpu_lo[MAX_GPU_RANGES], gpu_hi[MAX_GPU_RANGES];   /* the nvmap allocations, sorted by address */
static u32 ngpu, gpu_refresh, frame_alias_gpu;
static u32 alias_pending;          /* frames since the alias area was last asked about (its question waited) */
static u32 alias_late;             /* ... frames whose alias count was folded into a later one, this report */
static int alias_unmeasured;       /* no renderer to ask (macOS): the alias area's counts are 0 */
static volatile LONG pad_reads_frame;
static u64 pad_reads_sum, pad_reads_max;
static volatile DWORD pad_threads[PAD_THREADS];
static volatile LONG npad_threads;

/* the vsync ticks (written by the vsync thread only) */
static u32 tick_state[KT_REQUESTING + 1][MAX_FRAMES], nticks, ticks_unread;
typedef struct ThreadSeen { u64 id; int priority, core; u32 ticks, in[KT_REQUESTING + 1]; } ThreadSeen;
static ThreadSeen thread_seen[MAX_THREADS];
static u32 nthread_seen;

int rbp_probe(void) {
    static int once;
    if (!__atomic_exchange_n(&once, 1, __ATOMIC_ACQ_REL)) {
        const char *env = getenv("SSBU_RB_PROBE");
        int on = env && *env ? atoi(env) != 0 : 0;
        if (!on) {
            FILE *f = fopen("rollback_probe.on", "r");
            if (f) { fclose(f); on = 1; }
        }
        if (on) fprintf(stderr, "[rbprobe] on (rollback_probe.on, version 3): the alias area's, the heap's and the stack's writes are counted; a report every %d seconds\n",
                        (int)(REPORT_NS / 1000000000ull));
        __atomic_store_n(&rbp_state, on, __ATOMIC_RELEASE);
    }
    return __atomic_load_n(&rbp_state, __ATOMIC_ACQUIRE) > 0;   /* (another thread still deciding: not yet) */
}

static int area_of(u64 g) {
    for (int a = 0; a < AREAS; a++)
        if (g >= areas[a].base && g < areas[a].base + areas[a].size) return a;
    return -1;
}

/* Is guest page g inside an nvmap allocation (memory handed to the GPU)? */
static int gpu_page(u64 g) {
    u32 lo = 0, hi = ngpu;   /* the last allocation starting at or below g */
    while (lo < hi) { u32 mid = (lo + hi) / 2; if (gpu_lo[mid] <= g) lo = mid + 1; else hi = mid; }
    for (u32 i = lo; i-- > 0 && lo - i <= 64;)   /* (allocations may overlap or nest: look back a little) */
        if (g < gpu_hi[i]) return 1;
    return 0;
}

static void count_page(u64 g, void *user) {
    int a = (int)(intptr_t)user;
    if (g < areas[a].base || g >= areas[a].base + areas[a].size) return;
    if (a == ALIAS && gpu_page(g)) { frame_alias_gpu++; return; }   /* (the regions list the CPU's own pages only) */
    region_pages[a][(g - areas[a].base) >> REGION_SHIFT]++;
}

static void sort_gpu_ranges(void) {   /* (insertion sort: the list comes nearly sorted, in handle order) */
    for (u32 i = 1; i < ngpu; i++) {
        u64 l = gpu_lo[i], h = gpu_hi[i];
        u32 j = i;
        for (; j > 0 && gpu_lo[j - 1] > l; j--) { gpu_lo[j] = gpu_lo[j - 1]; gpu_hi[j] = gpu_hi[j - 1]; }
        gpu_lo[j] = l; gpu_hi[j] = h;
    }
}

static void refresh_gpu_ranges(void) {   /* every second or so: what the game hands the GPU changes slowly */
    if (gpu_refresh++ % 60) return;
    static u64 size[MAX_GPU_RANGES];
    ngpu = gpu_memory_list(gpu_lo, size, MAX_GPU_RANGES);
    for (u32 i = 0; i < ngpu; i++) gpu_hi[i] = gpu_lo[i] + size[i];
    sort_gpu_ranges();
}

/* The pages of the heap or the stack in [lo, hi) written since the last call, counted into their
 * regions; the watch is reset. (The alias area goes through the renderer: gfx_watch_pages.) */
static u32 take(int a, u64 lo, u64 hi) {
    static void *pages[BATCH];
    u32 total = 0;
    for (u64 at = lo; at < hi;) {
        ULONG_PTR count = BATCH;
        ULONG granularity = 0;
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, G2H(at), hi - at, pages, &count, &granularity) != 0 || !count) break;
        if (granularity) page_size = granularity;
        for (ULONG_PTR k = 0; k < count; k++) count_page(H2G(pages[k]), (void *)(intptr_t)a);
        total += (u32)count;
        if (count < BATCH) break;
        at = H2G(pages[count - 1]) + page_size;
    }
    return total;
}

/* Every area's pages written since the last call: out[a]; -1 in out[ALIAS] when its question waited. */
static void take_all(long long out[AREAS]) {
    u32 n = kernel_memory_regions(region_base, region_size, MAX_REGIONS);
    if (n) nregions = n;   /* (the kernel's lock busy: the last list) */
    for (int a = 0; a < AREAS; a++) out[a] = 0;
    u32 nalias = 0;
    for (u32 i = 0; i < nregions; i++) {
        u64 lo = region_base[i], hi = lo + region_size[i];
        int a = area_of(lo);
        if (a < 0 || hi > areas[a].base + areas[a].size) continue;
        if (a == ALIAS) { alias_base[nalias] = lo; alias_size[nalias] = region_size[i]; nalias++; }
        else out[a] += take(a, lo, hi);
    }
    /* -1: the renderer busy (the pages stay news: counted with the next frame); -2: not measured here */
    refresh_gpu_ranges();
    frame_alias_gpu = 0;
    out[ALIAS] = gfx_watch_pages(alias_base, alias_size, nalias, &alias_seen, count_page, (void *)(intptr_t)ALIAS);
}

static int cmp_u32(const void *x, const void *y) { u32 a = *(const u32 *)x, b = *(const u32 *)y; return (a > b) - (a < b); }

static void percentiles(const u32 *v, u32 n, u32 *med, u32 *p95, u32 *max) {
    static u32 sorted[MAX_FRAMES];
    memcpy(sorted, v, n * sizeof *sorted);
    qsort(sorted, n, sizeof *sorted, cmp_u32);
    *med = sorted[n / 2]; *p95 = sorted[n * 95 / 100]; *max = sorted[n - 1];
}

static void top_regions(int a, double secs) {
    char line[1024];
    int n = snprintf(line, sizeof line, "[rbprobe] %s regions written most (1 MB each: pages written in these %.0f s%s):", areas[a].name, secs,
                     a == ALIAS ? ", the CPU's own only" : "");
    u32 regions = (u32)(areas[a].size >> REGION_SHIFT);
    for (int t = 0; t < TOP_REGIONS; t++) {
        u32 best = 0, at = 0;
        for (u32 r = 0; r < regions; r++)
            if (region_pages[a][r] > best) { best = region_pages[a][r]; at = r; }
        if (!best) { if (!t) n += snprintf(line + n, sizeof line - n, " none"); break; }
        n += snprintf(line + n, sizeof line - n, "%s %#llx %u", t ? "," : "", (unsigned long long)(areas[a].base + ((u64)at << REGION_SHIFT)), best);
        region_pages[a][at] = 0;
    }
    fprintf(stderr, "%s\n", line);
}

static void report(u64 now) {
    double secs = (now - report_from) / 1e9, mb = page_size / 1048576.0;
    char line[1024];
    int n = snprintf(line, sizeof line, "[rbprobe] %.1f s, %u frames. Pages written per frame (%llu KB):", secs, nframes, (unsigned long long)(page_size / 1024));
    for (int a = 0; a < COUNTS && nframes; a++) {
        u32 med, p95, max;
        percentiles(written[a], nframes, &med, &p95, &max);
        n += snprintf(line + n, sizeof line - n, " %s median %u, p95 %u, max %u (%.1f / %.1f / %.1f MB);", count_names[a], med, p95, max, med * mb, p95 * mb, max * mb);
    }
    n += snprintf(line + n, sizeof line - n, " %u nvmap allocations known;", ngpu);
    if (nframes)
        n += snprintf(line + n, sizeof line - n, " finding them: avg %.2f ms, max %.2f ms", find_ns_sum / 1e6 / nframes, find_ns_max / 1e6);
    if (alias_late) n += snprintf(line + n, sizeof line - n, "; %u frames left out, their pages counted with the next (renderer busy)", alias_late);
    if (alias_unmeasured) n += snprintf(line + n, sizeof line - n, "; the alias area is not measured on this system");
    fprintf(stderr, "%s\n", line);
    top_regions(ALIAS, secs);
    top_regions(HEAP, secs);

    n = snprintf(line, sizeof line, "[rbprobe] pad reads per frame: avg %.1f, max %llu; by %ld thread(s):", nframes ? (double)pad_reads_sum / nframes : 0.0,
                 (unsigned long long)pad_reads_max, (long)npad_threads);
    for (LONG i = 0; i < npad_threads && i < PAD_THREADS; i++) n += snprintf(line + n, sizeof line - n, " %lu", (unsigned long)pad_threads[i]);
    fprintf(stderr, "%s\n", line);

    u32 slots, finalized;
    u64 made;
    fiber_counts(&slots, &finalized, &made);
    fprintf(stderr, "[rbprobe] fibers alive: %u (%u slots, %u finalized); %llu started since the game began\n", slots - finalized, slots, finalized, (unsigned long long)made);

    /* the vsync ticks (the vsync thread fills these; a tick racing this read is at worst off by one) */
    u32 ticks = nticks < MAX_FRAMES ? nticks : MAX_FRAMES;
    if (ticks) {
        static const char *const names[] = {"running", "waiting", "sleeping", "in a service request"};
        u32 still = 0;
        for (u32 i = 0; i < ticks; i++) still += tick_state[KT_RUNNING][i] == 0;
        n = snprintf(line, sizeof line, "[rbprobe] at the vsync tick, guest threads");
        for (int s = 0; s <= KT_REQUESTING; s++) {
            u32 med, p95, max;
            percentiles(tick_state[s], ticks, &med, &p95, &max);
            n += snprintf(line + n, sizeof line - n, " %s: median %u, max %u;", names[s], med, max);
        }
        snprintf(line + n, sizeof line - n, " none running at %u of %u ticks (%.0f%%); kernel busy at %u ticks", still, ticks, 100.0 * still / ticks, ticks_unread);
        fprintf(stderr, "%s\n", line);
        /* the threads running most often at the tick */
        n = snprintf(line, sizeof line, "[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %%):");
        u64 busy_id[3];
        double busy_pct[3];
        int nbusy = 0;
        for (int t = 0; t < 8; t++) {
            int best = -1;
            for (u32 i = 0; i < nthread_seen; i++)
                if (thread_seen[i].ticks && (best < 0 || thread_seen[i].in[KT_RUNNING] * (u64)thread_seen[best].ticks > thread_seen[best].in[KT_RUNNING] * (u64)thread_seen[i].ticks))
                    best = (int)i;
            if (best < 0) break;
            ThreadSeen *s = &thread_seen[best];
            n += snprintf(line + n, sizeof line - n, "%s %llu (p%d, c%d: %.0f/%.0f/%.0f/%.0f)", t ? "," : "", (unsigned long long)s->id, s->priority, s->core,
                          100.0 * s->in[KT_RUNNING] / s->ticks, 100.0 * s->in[KT_SLEEPING] / s->ticks, 100.0 * s->in[KT_REQUESTING] / s->ticks,
                          100.0 * s->in[KT_WAITING] / s->ticks);
            if (nbusy < 3 && s->in[KT_RUNNING] * 2 >= s->ticks) { busy_id[nbusy] = s->id; busy_pct[nbusy] = 100.0 * s->in[KT_RUNNING] / s->ticks; nbusy++; }
            s->ticks = 0;   /* (listed: out of the next pick) */
        }
        fprintf(stderr, "%s\n", line);
        /* where the busiest are in the game: their guest frames, at this moment */
        for (int i = 0; i < nbusy; i++) {
            HANDLE h = kernel_thread_host(busy_id[i]);
            if (!h) continue;
            int self = GetThreadId(h) == GetCurrentThreadId();   /* (this thread, queueing a frame: it cannot stop itself to look) */
            fprintf(stderr, "[rbprobe] thread %llu (running at %.0f%% of the ticks) is in%s:\n", (unsigned long long)busy_id[i], busy_pct[i],
                    self ? " (it is the thread writing this report, seen while it queues a frame)" : "");
            if (self) rt_backtrace();
            else rt_backtrace_thread(h, 8);
        }
    }
    nticks = ticks_unread = nthread_seen = 0;

    for (int a = 0; a < AREAS; a++) memset(region_pages[a], 0, (size_t)(areas[a].size >> REGION_SHIFT) * sizeof **region_pages);
    nframes = alias_late = 0;
    find_ns_sum = find_ns_max = 0;
    pad_reads_sum = pad_reads_max = 0;
    report_from = now;
    report_at = now + REPORT_NS;
}

void rbp_frame_queued(void) {
    if (InterlockedExchange(&busy, 1)) return;   /* (one frame at a time: a second caller just skips) */
    u64 t0 = rt_now_ns();
    long long got[AREAS];
    if (!report_at) {   /* the first frame: start counting from here */
        for (int a = 0; a < AREAS; a++) region_pages[a] = calloc((size_t)(areas[a].size >> REGION_SHIFT), sizeof **region_pages);
        if (!region_pages[ALIAS] || !region_pages[HEAP] || !region_pages[STACK]) { fprintf(stderr, "[rbprobe] out of memory: off\n"); rbp_state = 0; return; }
        take_all(got);
        for (int a = 0; a < AREAS; a++) memset(region_pages[a], 0, (size_t)(areas[a].size >> REGION_SHIFT) * sizeof **region_pages);
        InterlockedExchange(&pad_reads_frame, 0);
        report_from = t0;
        report_at = t0 + REPORT_NS;
        InterlockedExchange(&busy, 0);
        return;
    }
    take_all(got);
    u64 dt = rt_now_ns() - t0;
    if (got[ALIAS] == -2) { alias_unmeasured = 1; got[ALIAS] = 0; }
    if (got[ALIAS] == -1) alias_pending++;   /* (left out of the counts: the next frame's covers this one too) */
    else {
        if (alias_pending) { alias_late += alias_pending; alias_pending = 0; }
        if (nframes < MAX_FRAMES) {
            for (int a = 0; a < AREAS; a++) written[a][nframes] = (u32)got[a];
            written[ALIAS_GPU][nframes] = frame_alias_gpu;
            written[ALIAS_CPU][nframes] = (u32)got[ALIAS] - frame_alias_gpu;
            nframes++;
        }
    }
    find_ns_sum += dt;
    if (dt > find_ns_max) find_ns_max = dt;
    u64 reads = (u64)InterlockedExchange(&pad_reads_frame, 0);
    pad_reads_sum += reads;
    if (reads > pad_reads_max) pad_reads_max = reads;
    if (t0 >= report_at) report(t0);
    InterlockedExchange(&busy, 0);
}

void rbp_pad_read(void) {
    InterlockedIncrement(&pad_reads_frame);
    DWORD me = GetCurrentThreadId();
    LONG n = npad_threads;
    for (LONG i = 0; i < n && i < PAD_THREADS; i++) if (pad_threads[i] == me) return;
    LONG slot = InterlockedIncrement(&npad_threads) - 1;   /* (a new reader: rare, so a race only lists one twice) */
    if (slot < PAD_THREADS) pad_threads[slot] = me;
}

static void tick_thread(void *user, u64 id, int priority, int core, int state) {
    u32 *counts = user;
    counts[state]++;
    ThreadSeen *s = NULL;
    for (u32 i = 0; i < nthread_seen; i++) if (thread_seen[i].id == id) { s = &thread_seen[i]; break; }
    if (!s && nthread_seen < MAX_THREADS) { s = &thread_seen[nthread_seen++]; memset(s, 0, sizeof *s); s->id = id; }
    if (!s) return;
    s->priority = priority; s->core = core;
    s->ticks++;
    s->in[state]++;
}

void rbp_vsync_tick(void) {
    u32 counts[KT_REQUESTING + 1] = {0};
    if (!kernel_thread_each(tick_thread, counts)) { ticks_unread++; return; }
    if (nticks < MAX_FRAMES)
        for (int s = 0; s <= KT_REQUESTING; s++) tick_state[s][nticks] = counts[s];
    nticks++;
}
