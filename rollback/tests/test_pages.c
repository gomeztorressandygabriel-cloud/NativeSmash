/* Page-delta snapshots, alone and driving the rollback core. */
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "rb_pages.h"
#include "rollback.h"

#define PS 4096

static uint64_t rng_state = 88172645463325252ull;
static uint32_t rnd(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return (uint32_t)(rng_state >> 16); }

/* write `n` distinct random pages, listing them in dirty[] */
static size_t scribble(uint8_t *mem, size_t npages, size_t n, uint32_t *dirty, uint8_t *seen) {
    memset(seen, 0, npages);
    size_t k = 0;
    while (k < n) {
        uint32_t p = rnd() % npages;
        if (seen[p]) continue;
        seen[p] = 1;
        dirty[k++] = p;
        for (int j = 0; j < 8; j++) mem[(size_t)p * PS + rnd() % PS] = (uint8_t)rnd();
    }
    return k;
}

static void test_pages_roundtrip(void) {
    enum { NP = 256, RING = 8, FRAMES = 300 };
    uint8_t *mem = calloc(NP, PS), *shadow[RING + 1];
    uint32_t dirty[NP]; uint8_t seen[NP];
    for (int i = 0; i <= RING; i++) shadow[i] = malloc((size_t)NP * PS);
    for (size_t i = 0; i < (size_t)NP * PS; i++) mem[i] = (uint8_t)rnd();
    RbPages *p = rbp_create(mem, NP, PS, RING);
    CHECK(p != NULL, "create");
    size_t nd = 0;
    int32_t frame = 0;
    for (int step = 0; step < FRAMES; step++) {
        CHECK(rbp_save(p, frame, dirty, nd) == 0, "save %d", frame);
        memcpy(shadow[frame % (RING + 1)], mem, (size_t)NP * PS);   /* (the reference: a full copy) */
        frame++;
        nd = scribble(mem, NP, 1 + rnd() % 40, dirty, seen);
        if (step % 17 == 16) {   /* roll back a few frames, then carry on from there */
            int32_t back = frame - 1 - (int32_t)(rnd() % (RING - 1));
            if (back < 0) back = 0;
            CHECK(rbp_load(p, back, dirty, nd) == 0, "load %d", back);
            CHECK(memcmp(mem, shadow[back % (RING + 1)], (size_t)NP * PS) == 0, "memory after loading frame %d differs from the full copy", back);
            frame = back;
            nd = 0;
        }
    }
    const RbPagesStats *st = rbp_stats(p);
    CHECK(st->pages_held <= (size_t)(RING + 1) * 40 + NP, "pages held %zu", st->pages_held);
    rbp_destroy(p);
    free(mem);
    for (int i = 0; i <= RING; i++) free(shadow[i]);
}

static void test_pages_errors(void) {
    uint8_t mem[PS * 4] = {0};
    RbPages *p = rbp_create(mem, 4, PS, 2);
    uint32_t d[1] = {1};
    CHECK(rbp_save(p, 0, NULL, 0) == 0, "first");
    CHECK(rbp_save(p, 0, NULL, 0) == 0, "re-save of the same frame with nothing dirty is a no-op");
    CHECK(rbp_save(p, 0, d, 1) != 0, "re-save with dirty pages is refused");
    CHECK(rbp_save(p, 1, d, 1) == 0, "next");
    CHECK(rbp_save(p, 2, NULL, 0) == 0 && rbp_save(p, 3, NULL, 0) == 0, "ring wraps");
    CHECK(rbp_load(p, 0, NULL, 0) != 0, "frame 0 has expired");
    CHECK(rbp_load(p, 3, NULL, 0) == 0, "frame 3 is kept");
    rbp_destroy(p);
}

/* A game whose whole state is a memory region, saved by pages through the rollback core.
 * The core's save buffer holds only the frame number (the key); the pages hold the rest. */
typedef struct PGame {
    uint8_t *mem; size_t np;
    RbPages *pages;
    uint32_t dirty[64]; size_t nd;
    uint32_t frame;
    int full_copy;          /* 1: reference, saves the whole region instead */
} PGame;

static void pg_mark(PGame *g, uint32_t page) {
    for (size_t i = 0; i < g->nd; i++) if (g->dirty[i] == page) return;
    g->dirty[g->nd++] = page;
}
static void pg_save(void *u, void *buf) {
    PGame *g = u;
    if (g->full_copy) { memcpy(buf, g->mem, g->np * PS); g->nd = 0; return; }
    memcpy(buf, &g->frame, 4);
    rbp_save(g->pages, (int32_t)g->frame, g->dirty, g->nd);
    g->nd = 0;
}
static void pg_load(void *u, const void *buf) {
    PGame *g = u;
    if (g->full_copy) { memcpy(g->mem, buf, g->np * PS); memcpy(&g->frame, g->mem, 4); g->nd = 0; return; }
    uint32_t f;
    memcpy(&f, buf, 4);
    rbp_load(g->pages, (int32_t)f, g->dirty, g->nd);
    g->nd = 0;
    g->frame = f;
}
static void pg_advance(void *u, const RbInput *in, int resim) {
    (void)resim;
    PGame *g = u;
    uint64_t h = rb_hash64(in, 16) ^ g->frame;
    for (int k = 0; k < 5; k++) {   /* a few pages change, depending on the inputs and on the frame */
        uint32_t page = 1 + (uint32_t)((h >> (k * 8)) % (g->np - 1));
        uint32_t *w = (uint32_t *)(g->mem + (size_t)page * PS) + (h >> 3) % 8;
        *w = *w * 1664525u + (uint32_t)h + 1013904223u;
        pg_mark(g, page);
    }
    g->frame++;
    memcpy(g->mem, &g->frame, 4);
    pg_mark(g, 0);
}
static uint64_t pg_sum(void *u) { PGame *g = u; return rb_hash64(g->mem, g->np * PS); }

static uint64_t play_pgame(int full, uint64_t *saved_pages) {
    enum { NP = 512 };
    PGame g = {calloc(NP, PS), NP, NULL, {0}, 0, 0, full};
    uint8_t *state_probe = NULL; (void)state_probe;
    if (!full) g.pages = rbp_create(g.mem, NP, PS, 20);
    RbGame game = {&g, full ? (size_t)NP * PS : 4, pg_save, pg_load, pg_advance, pg_sum};
    RbConfig cfg = {2, 0, 12, 0, 0};
    RbSession *s = rb_create(&game, &cfg);
    int lag = 5;
    for (int t = 0; t < 400; t++) {
        int f = t - lag;
        if (f >= 0) rb_add_remote_input(s, 1, f, (f / 3) % 5);
        rb_tick(s, (t / 4) % 7);
    }
    for (int f = 400 - lag; f < 400; f++) rb_add_remote_input(s, 1, f, (f / 3) % 5);
    rb_resolve(s);
    CHECK(rb_stats(s)->rollbacks > 0, "no rollbacks happened");
    uint64_t sum = pg_sum(&g);
    if (saved_pages && g.pages) *saved_pages = rbp_stats(g.pages)->pages_copied;
    rb_destroy(s);
    if (g.pages) rbp_destroy(g.pages);
    free(g.mem);
    return sum;
}

static void test_pages_under_rollback(void) {
    uint64_t pages = 0;
    uint64_t a = play_pgame(1, NULL), b = play_pgame(0, &pages);
    CHECK(a == b, "paged snapshots gave another final memory than whole-region copies");
    CHECK(pages > 0 && pages < 400 * 40, "pages copied %llu", (unsigned long long)pages);
}

void test_pages_all(void) {
    RUN(test_pages_errors);
    RUN(test_pages_roundtrip);
    RUN(test_pages_under_rollback);
}
