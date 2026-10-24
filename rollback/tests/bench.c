/* Cost measurements: `make bench`. Numbers depend on the machine; the cloud VM that runs the CI
 * is not a phone, so docs/ROLLBACK.md uses them as ratios and scales them with stated assumptions. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "harness.h"
#include "rb_pages.h"

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static double median(double *v, int n) { qsort(v, (size_t)n, sizeof *v, cmp); return v[n / 2]; }

static uint64_t rs = 0x2545F4914F6CDD1Dull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }

static void bench_full_copy(void) {
    printf("\n== Saving the whole state every frame (memcpy) ==\n%10s %12s %12s %12s\n", "state MB", "copy ms", "GB/s", "hash ms");
    size_t sizes[] = {1, 4, 16, 64, 256, 512};
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        size_t n = sizes[i] << 20;
        uint8_t *a = malloc(n), *b = malloc(n);
        memset(a, 1, n); memset(b, 2, n);
        double t[9], h[9];
        volatile uint64_t sink = 0;
        for (int r = 0; r < 9; r++) {
            double t0 = now_ms();
            memcpy(b, a, n);
            t[r] = now_ms() - t0;
            t0 = now_ms();
            sink += rb_hash64(a, n);
            h[r] = now_ms() - t0;
        }
        double m = median(t, 9);
        printf("%10zu %12.3f %12.1f %12.3f\n", sizes[i], m, (double)n / 1e9 / (m / 1e3), median(h, 9));
        free(a); free(b);
    }
}

static void bench_pages(void) {
    enum { PS = 4096, RING = 12 };
    size_t npages = (256u << 20) / PS;
    printf("\n== Page-delta snapshots over a 256 MB region (ring of %d) ==\n", RING);
    printf("%12s %12s %14s %14s %14s %14s\n", "dirty/frame", "save ms", "load d=1 ms", "load d=4 ms", "load d=8 ms", "held MB");
    size_t counts[] = {50, 200, 1000, 4000, 16000};
    for (unsigned c = 0; c < sizeof counts / sizeof *counts; c++) {
        uint8_t *mem = malloc(npages * PS);
        memset(mem, 7, npages * PS);
        uint32_t *dirty = malloc(counts[c] * sizeof *dirty);
        uint8_t *seen = calloc(npages, 1);
        RbPages *p = rbp_create(mem, npages, PS, RING);
        double save_sum = 0, load_t[3] = {0, 0, 0};
        int saves = 0, loads[3] = {0, 0, 0};
        int32_t frame = 0;
        size_t nd = 0;
        for (int step = 0; step < 120; step++) {
            double t0 = now_ms();
            rbp_save(p, frame, dirty, nd);
            if (step >= RING) { save_sum += now_ms() - t0; saves++; }
            frame++;
            memset(seen, 0, npages);
            for (nd = 0; nd < counts[c];) {   /* the next frame's writes */
                uint32_t pg = rnd() % npages;
                if (seen[pg]) continue;
                seen[pg] = 1; dirty[nd++] = pg;
                mem[(size_t)pg * PS + rnd() % PS] ^= 0x5A;
            }
            if (step >= RING && step % 10 == 9) {
                static const int depth[3] = {1, 4, 8};
                int k = (step / 10) % 3;
                double t1 = now_ms();
                rbp_load(p, frame - depth[k], dirty, nd);
                load_t[k] += now_ms() - t1; loads[k]++;
                frame = frame - depth[k] + 0;
                nd = 0;
                /* re-save the loaded frame, as the core does */
                rbp_save(p, frame, NULL, 0);
                frame++;
            }
        }
        printf("%12zu %12.3f %14.3f %14.3f %14.3f %14.1f\n", counts[c], save_sum / saves,
               loads[0] ? load_t[0] / loads[0] : 0, loads[1] ? load_t[1] / loads[1] : 0, loads[2] ? load_t[2] / loads[2] : 0,
               (double)rbp_stats(p)->pages_held * PS / 1e6);
        rbp_destroy(p);
        free(mem); free(dirty); free(seen);
    }
}

static void bench_network(void) {
    printf("\n== The toy match over the simulated network (1800 frames, 4 KB of state) ==\n");
    printf("%-26s %6s %6s %6s %8s %8s %8s\n", "scenario", "delay", "loss%", "depth", "resim/f", "avg us", "B/s");
    struct { double d, j, loss; int idelay, mp; } sc[] = {
        {1, 0, 0, 0, 8}, {25, 3, 0, 0, 10}, {50, 5, 0.01, 1, 10}, {75, 8, 0.02, 2, 12}, {100, 12, 0.03, 2, 14},
        {150, 20, 0.05, 3, 18}, {200, 30, 0.05, 3, 22},
    };
    for (unsigned i = 0; i < sizeof sc / sizeof *sc; i++) {
        PairCfg c = pair_defaults();
        c.net = (SimNetCfg){sc[i].d, sc[i].j, sc[i].loss, 0.01, 0.05, 4 * sc[i].j + 20};
        c.input_delay = sc[i].idelay; c.max_prediction = sc[i].mp;
        PairResult r;
        run_pair(&c, &r);
        printf("%-26s %6.0f %6.1f %6u %8.3f %8.2f %8.0f %s\n", "one-way delay", sc[i].d, sc[i].loss * 100, r.side[0].rb.max_depth,
               (double)r.side[0].rb.resim_frames / c.frames, r.side[0].avg_tick_us, (double)r.side[0].net.bytes_sent * 60.0 / c.frames,
               r.finished && r.side[0].state_hash == r.reference_hash ? "" : "(DID NOT CONVERGE)");
    }
}

int main(void) {
    bench_full_copy();
    bench_pages();
    bench_network();
    return 0;
}
