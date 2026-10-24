/* Rollback probe: what rollback netplay would have to save, restore and replay every frame,
 * measured on the real game - for build\play.log (docs/ROLLBACK.md, phase 1).
 *
 * Off unless rollback_probe.on exists in the current directory (beside "Play SSBU.bat"; the file
 * may be empty) or SSBU_RB_PROBE=1. On, kernel_init reserves the heap and the stack areas with
 * their writes watched too (the alias area always is, for the renderer), and at the end of each
 * of the game's frames - when it queues one for the display - the pages written since the last
 * frame are read and the watch reset. That count is what a rollback would copy per frame. Every
 * 5 seconds the log gets:
 *
 *   pages written per frame in the heap and the stack (median, 95th percentile, maximum, and MB);
 *   how long reading and resetting the watch took (the price of knowing them, per frame);
 *   the heap's 1 MB regions written most (where the simulation's state is likely to live);
 *   how often the game reads the pads per frame, and from how many threads;
 *   guest threads alive, and fibers alive (each one a host stack a rollback would have to restore).
 *
 * The alias area is left out: the renderer reads and resets its watch for its own use, and a
 * second reader would hide textures' changes from it. The probe slows the game a little (the
 * watch on the heap, a reset each frame): play normally, not for timing records. */
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

static const struct { u64 base, size; const char *name; } areas[] = {{HEAP_BASE, HEAP_LIMIT, "heap"}, {STACK_BASE, STACK_SIZE, "stack"}};
enum { HEAP, STACK, AREAS };

static volatile LONG busy;
static u64 report_from, report_at;
static u32 written[AREAS][MAX_FRAMES], nframes;
static u32 *region_pages[AREAS];
static u64 watch_ns_sum, watch_ns_max, page_size = 4096;
static volatile LONG pad_reads_frame;
static u64 pad_reads_sum, pad_reads_max;
static volatile DWORD pad_threads[PAD_THREADS];
static volatile LONG npad_threads;

int rbp_probe(void) {
    static int once;
    if (!__atomic_exchange_n(&once, 1, __ATOMIC_ACQ_REL)) {
        const char *env = getenv("SSBU_RB_PROBE");
        int on = env && *env ? atoi(env) != 0 : 0;
        if (!on) {
            FILE *f = fopen("rollback_probe.on", "r");
            if (f) { fclose(f); on = 1; }
        }
        if (on) fprintf(stderr, "[rbprobe] on (rollback_probe.on): the heap's and the stack's writes are watched; a report every %d seconds\n",
                        (int)(REPORT_NS / 1000000000ull));
        __atomic_store_n(&rbp_state, on, __ATOMIC_RELEASE);
    }
    return __atomic_load_n(&rbp_state, __ATOMIC_ACQUIRE) > 0;   /* (another thread still deciding: not yet) */
}

/* The pages of area `a` written since the last call, counted into their regions; the watch is reset. */
static u32 take(int a) {
    static void *pages[BATCH];
    u64 lo = areas[a].base, hi = lo + areas[a].size;
    u32 total = 0;
    for (u64 at = lo; at < hi;) {
        ULONG_PTR count = BATCH;
        ULONG granularity = 0;
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, G2H(at), hi - at, pages, &count, &granularity) != 0 || !count) break;
        if (granularity) page_size = granularity;
        for (ULONG_PTR k = 0; k < count; k++) {
            u64 g = H2G(pages[k]);
            if (g >= lo && g < hi) region_pages[a][(g - lo) >> REGION_SHIFT]++;
        }
        total += (u32)count;
        if (count < BATCH) break;
        at = H2G(pages[count - 1]) + page_size;
    }
    return total;
}

static int cmp_u32(const void *x, const void *y) { u32 a = *(const u32 *)x, b = *(const u32 *)y; return (a > b) - (a < b); }

static void report(u64 now) {
    double secs = (now - report_from) / 1e9, mb = page_size / 1048576.0;
    char line[1024];
    int n = snprintf(line, sizeof line, "[rbprobe] %.1f s, %u frames. Pages written per frame (%llu KB):", secs, nframes, (unsigned long long)(page_size / 1024));
    for (int a = 0; a < AREAS && nframes; a++) {
        static u32 sorted[MAX_FRAMES];
        memcpy(sorted, written[a], nframes * sizeof *sorted);
        qsort(sorted, nframes, sizeof *sorted, cmp_u32);
        u32 med = sorted[nframes / 2], p95 = sorted[nframes * 95 / 100], max = sorted[nframes - 1];
        n += snprintf(line + n, sizeof line - n, " %s median %u, p95 %u, max %u (%.1f / %.1f / %.1f MB);", areas[a].name, med, p95, max, med * mb, p95 * mb, max * mb);
    }
    if (nframes)
        snprintf(line + n, sizeof line - n, " reading and resetting the watch: avg %.2f ms, max %.2f ms", watch_ns_sum / 1e6 / nframes, watch_ns_max / 1e6);
    fprintf(stderr, "%s\n", line);

    /* the heap's regions written most */
    n = snprintf(line, sizeof line, "[rbprobe] heap regions written most (1 MB each: pages written in these %.0f s):", secs);
    u32 regions = (u32)(areas[HEAP].size >> REGION_SHIFT);
    for (int t = 0; t < TOP_REGIONS; t++) {
        u32 best = 0, at = 0;
        for (u32 r = 0; r < regions; r++)
            if (region_pages[HEAP][r] > best) { best = region_pages[HEAP][r]; at = r; }
        if (!best) break;
        n += snprintf(line + n, sizeof line - n, " %#llx %u%s", (unsigned long long)(areas[HEAP].base + ((u64)at << REGION_SHIFT)), best, t + 1 < TOP_REGIONS ? "," : "");
        region_pages[HEAP][at] = 0;
    }
    fprintf(stderr, "%s\n", line);

    n = snprintf(line, sizeof line, "[rbprobe] pad reads per frame: avg %.1f, max %llu; by %ld thread(s):", nframes ? (double)pad_reads_sum / nframes : 0.0,
                 (unsigned long long)pad_reads_max, (long)npad_threads);
    for (LONG i = 0; i < npad_threads && i < PAD_THREADS; i++) n += snprintf(line + n, sizeof line - n, " %lu", (unsigned long)pad_threads[i]);
    fprintf(stderr, "%s\n", line);

    u32 slots, finalized, threads = kernel_thread_count();
    u64 made;
    fiber_counts(&slots, &finalized, &made);
    if (threads == ~0u) fprintf(stderr, "[rbprobe] guest threads alive: (kernel busy, not counted);");
    else fprintf(stderr, "[rbprobe] guest threads alive: %u;", threads);
    fprintf(stderr, " fibers alive: %u (%u slots, %u finalized); %llu started since the game began\n", slots - finalized, slots, finalized, (unsigned long long)made);

    for (int a = 0; a < AREAS; a++) memset(region_pages[a], 0, (size_t)(areas[a].size >> REGION_SHIFT) * sizeof **region_pages);
    nframes = 0;
    watch_ns_sum = watch_ns_max = 0;
    pad_reads_sum = pad_reads_max = 0;
    report_from = now;
    report_at = now + REPORT_NS;
}

void rbp_frame_queued(void) {
    if (InterlockedExchange(&busy, 1)) return;   /* (one frame at a time: a second caller just skips) */
    u64 t0 = rt_now_ns();
    if (!report_at) {   /* the first frame: start counting from here */
        for (int a = 0; a < AREAS; a++) region_pages[a] = calloc((size_t)(areas[a].size >> REGION_SHIFT), sizeof **region_pages);
        if (!region_pages[HEAP] || !region_pages[STACK]) { fprintf(stderr, "[rbprobe] out of memory: off\n"); rbp_state = 0; return; }
        for (int a = 0; a < AREAS; a++) take(a);
        for (int a = 0; a < AREAS; a++) memset(region_pages[a], 0, (size_t)(areas[a].size >> REGION_SHIFT) * sizeof **region_pages);
        InterlockedExchange(&pad_reads_frame, 0);
        report_from = t0;
        report_at = t0 + REPORT_NS;
        InterlockedExchange(&busy, 0);
        return;
    }
    u32 h = take(HEAP), s = take(STACK);
    u64 dt = rt_now_ns() - t0;
    if (nframes < MAX_FRAMES) { written[HEAP][nframes] = h; written[STACK][nframes] = s; nframes++; }
    watch_ns_sum += dt;
    if (dt > watch_ns_max) watch_ns_max = dt;
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
