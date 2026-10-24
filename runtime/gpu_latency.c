/* Latency counters: where the time between a button press and the picture, and between the game's
 * sends and receives, goes - for build\play.log.
 *
 * Off unless latency.on exists in the current directory (beside "Play SSBU.bat"; the file may be
 * empty) or SSBU_LATENCY=1: then every hook returns after one load. On, a line per histogram is
 * printed every 5 seconds (see docs/LATENCY-AUDIT.md for how to read them). The counters are
 * atomic and lock-free: none of them can hold up the game's, the vsync or the network threads. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "latency.h"

volatile int lat_state = -1;
volatile u64 lat_pad_read_ns;

#define BUCKETS 8
#define REPORT_NS 5000000000ull

typedef struct Hist { const char *name; u32 edges[BUCKETS - 1]; /* us */ volatile u64 bucket[BUCKETS], n, sum, max; } Hist;
#define GAPS {2000, 8000, 14000, 19000, 28000, 40000, 60000}
#define AGES {1000, 2000, 4000, 8000, 16000, 33000, 66000}
#define SHOWN {8000, 16000, 25000, 33000, 50000, 75000, 100000}
#define LATES {100, 500, 1000, 2000, 4000, 8000, 16000}
#define SPANS {500, 1000, 2000, 4000, 8000, 16000, 33000}
static Hist hists[LAT_HISTS] = {
    [LAT_SEND_GAP] = {"datagram sent, gap to the previous one to the same place", GAPS},
    [LAT_RECV_GAP] = {"datagram received, gap to the previous one from the same place", GAPS},
    [LAT_RECV_WAIT] = {"datagram waiting in the game's socket until read", AGES},
    [LAT_PAD_AGE] = {"pad state age when the game reads it", AGES},
    [LAT_PAD_TO_SHOWN] = {"pad read -> frame handed to the display", SHOWN},
    [LAT_QUEUE_TO_SHOWN] = {"frame queued -> handed to the display", SHOWN},
    [LAT_FRAME_GAP] = {"frame queued, gap to the previous one", GAPS},
    [LAT_GAME_WORK] = {"vsync event -> game queues its frame", GAPS},
    [LAT_TICK_GAP] = {"vsync tick, gap to the previous one", GAPS},
    [LAT_TICK_LATE] = {"vsync tick, wake-up after it was due", LATES},
    [LAT_GPU_WAIT] = {"vsync thread waiting for the GPU thread", SPANS},
    [LAT_PRESENT_FRAME] = {"vsync thread in present_frame", SPANS},
    [LAT_PRESENT_CALL] = {"IDXGISwapChain::Present", SPANS},
    [LAT_FRAME_WAIT] = {"vsync thread waiting for a free swap chain slot (present_wait)", SPANS},
    [LAT_LOCK_WAIT] = {"vsync thread waiting for the renderer's lock", SPANS},
};
static const char *const COUNTER_NAMES[LAT_COUNTERS] = {"vsync ticks", "ticks with no new frame", "frames queued", "datagrams sent",
                                                        "datagrams received", "clock restarts (a tick late by half a refresh or more)"};
static volatile u64 counters[LAT_COUNTERS];
static volatile u64 depth[4];   /* frames waiting when the game queued one: 1, 2, 3, 4+ */

int lat_probe(void) {
    static int once;
    if (!__atomic_exchange_n(&once, 1, __ATOMIC_ACQ_REL)) {
        const char *env = getenv("SSBU_LATENCY");
        int on = env && *env ? atoi(env) != 0 : 0;
        if (!on) {
            FILE *f = fopen("latency.on", "r");
            if (f) { fclose(f); on = 1; }
        }
        if (on) fprintf(stderr, "[latency] counters on (latency.on): a report every %d seconds\n", (int)(REPORT_NS / 1000000000ull));
        __atomic_store_n(&lat_state, on, __ATOMIC_RELEASE);
    }
    return __atomic_load_n(&lat_state, __ATOMIC_ACQUIRE) > 0;   /* (another thread still deciding: not yet) */
}

void lat_add(int hist, u64 ns) {
    if (hist < 0 || hist >= LAT_HISTS) return;
    Hist *h = &hists[hist];
    u32 us = ns / 1000 > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)(ns / 1000);
    int b = 0;
    while (b < BUCKETS - 1 && us >= h->edges[b]) b++;
    __atomic_fetch_add(&h->bucket[b], 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&h->n, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&h->sum, ns, __ATOMIC_RELAXED);
    u64 m = __atomic_load_n(&h->max, __ATOMIC_RELAXED);
    while (ns > m && !__atomic_compare_exchange_n(&h->max, &m, ns, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}

void lat_count(int counter) {
    if (counter >= 0 && counter < LAT_COUNTERS) __atomic_fetch_add(&counters[counter], 1, __ATOMIC_RELAXED);
}

/* The last time per (histogram, key): a small table, an entry claimed by whoever first sees its key. */
void lat_gap(int hist, u64 key) {
    static struct { volatile u64 key, last; } flows[32];
    u64 now = rt_now_ns(), id = (key << 8 | (u64)hist) + 1;   /* (never 0) */
    for (u32 i = 0, at = (u32)((id * 0x9E3779B97F4A7C15ull) >> 59); i < 32; i++, at = (at + 1) & 31) {
        u64 k = __atomic_load_n(&flows[at].key, __ATOMIC_ACQUIRE);
        if (!k && __atomic_compare_exchange_n(&flows[at].key, &k, id, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) k = id;
        if (k != id) continue;
        u64 last = __atomic_exchange_n(&flows[at].last, now, __ATOMIC_RELAXED);
        if (last && now >= last) lat_add(hist, now - last);
        return;
    }
}

void lat_pad_read(u64 published_ns) {
    u64 now = rt_now_ns();
    if (published_ns && now >= published_ns) lat_add(LAT_PAD_AGE, now - published_ns);
    lat_pad_read_ns = now;
}

static volatile u64 last_queued_ns, last_signal_ns;

void lat_frame_queued(u32 pending) {
    u64 now = rt_now_ns(), last = __atomic_exchange_n(&last_queued_ns, now, __ATOMIC_RELAXED), sig = last_signal_ns;
    if (last && now >= last) lat_add(LAT_FRAME_GAP, now - last);
    if (sig && now >= sig && (!last || sig > last)) lat_add(LAT_GAME_WORK, now - sig);   /* (the first frame after a signal) */
    lat_count(LAT_FRAMES);
    __atomic_fetch_add(&depth[pending < 1 ? 0 : pending > 4 ? 3 : pending - 1], 1, __ATOMIC_RELAXED);
}

void lat_frame_shown(u64 queued_ns, u64 pad_ns) {
    u64 now = rt_now_ns();
    if (queued_ns && now >= queued_ns) lat_add(LAT_QUEUE_TO_SHOWN, now - queued_ns);
    if (pad_ns && now >= pad_ns) lat_add(LAT_PAD_TO_SHOWN, now - pad_ns);
}

void lat_tick_signalled(void) {
    u64 now = rt_now_ns(), last = __atomic_exchange_n(&last_signal_ns, now, __ATOMIC_RELAXED);
    if (last && now >= last) lat_add(LAT_TICK_GAP, now - last);
    lat_count(LAT_TICKS);
}

void lat_report(void) {
    static u64 since;
    u64 now = rt_now_ns();
    if (!since) since = now;
    if (now - since < REPORT_NS) return;
    double secs = (double)(now - since) / 1e9;
    since = now;
    char line[600];
    int n = snprintf(line, sizeof line, "[latency] %.1f s:", secs);
    for (int i = 0; i < LAT_COUNTERS; i++) {
        u64 c = __atomic_exchange_n(&counters[i], 0, __ATOMIC_RELAXED);
        n += snprintf(line + n, sizeof line - (size_t)n, " %s %llu (%.1f/s);", COUNTER_NAMES[i], (unsigned long long)c, (double)c / secs);
    }
    fprintf(stderr, "%s\n", line);
    u64 d[4];
    for (int i = 0; i < 4; i++) d[i] = __atomic_exchange_n(&depth[i], 0, __ATOMIC_RELAXED);
    if (d[0] + d[1] + d[2] + d[3])
        fprintf(stderr, "[latency] frames waiting for the display when the game queued one: 1: %llu  2: %llu  3: %llu  4+: %llu\n",
                (unsigned long long)d[0], (unsigned long long)d[1], (unsigned long long)d[2], (unsigned long long)d[3]);
    for (int i = 0; i < LAT_HISTS; i++) {
        Hist *h = &hists[i];
        u64 count = __atomic_exchange_n(&h->n, 0, __ATOMIC_RELAXED);
        u64 sum = __atomic_exchange_n(&h->sum, 0, __ATOMIC_RELAXED), max = __atomic_exchange_n(&h->max, 0, __ATOMIC_RELAXED);
        u64 b[BUCKETS];
        for (int k = 0; k < BUCKETS; k++) b[k] = __atomic_exchange_n(&h->bucket[k], 0, __ATOMIC_RELAXED);
        if (!count) continue;
        n = snprintf(line, sizeof line, "[latency] %s (ms; n %llu, avg %.2f, max %.2f): ", h->name, (unsigned long long)count,
                     (double)sum / (double)count / 1e6, (double)max / 1e6);
        for (int k = 0; k < BUCKETS; k++) {
            char range[40];
            if (k == 0) snprintf(range, sizeof range, "<%g", h->edges[0] / 1000.0);
            else if (k == BUCKETS - 1) snprintf(range, sizeof range, ">=%g", h->edges[k - 1] / 1000.0);
            else snprintf(range, sizeof range, "%g-%g", h->edges[k - 1] / 1000.0, h->edges[k] / 1000.0);
            n += snprintf(line + n, sizeof line - (size_t)n, "%s%s: %llu", k ? "  " : "", range, (unsigned long long)b[k]);
        }
        fprintf(stderr, "%s\n", line);
    }
}
