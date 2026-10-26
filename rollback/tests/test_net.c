/* Two sessions over the simulated network: convergence, desync detection, pacing, protocol. */
#include <stdio.h>
#include <string.h>
#include "check.h"
#include "harness.h"

typedef struct { const char *name; double delay, jitter, loss, dup, reorder; int input_delay, maxpred; } Scenario;

static const Scenario scenarios[] = {
    {"lan, no loss",            1,   0,  0.00, 0.00, 0.00, 0,  8},
    {"20 ms",                  20,   3,  0.00, 0.00, 0.00, 0, 10},
    {"40 ms, 1% loss",         40,   5,  0.01, 0.00, 0.02, 1, 10},
    {"60 ms, 3% loss",         60,  10,  0.03, 0.01, 0.05, 2, 12},
    {"100 ms, 5% loss",       100,  20,  0.05, 0.02, 0.10, 2, 16},
    {"150 ms, 10% loss",      150,  30,  0.10, 0.02, 0.10, 3, 20},
    {"250 ms, 5% loss",       250,  40,  0.05, 0.02, 0.10, 3, 24},
};

int g_verbose;

static void print_header(void) {
    printf("    %-20s %6s %6s %7s %7s %6s %9s %9s %6s\n", "scenario", "stalls", "waits", "rolls", "resim", "depth", "avg us", "max us", "B/s");
}
static void print_row(const char *name, const PairResult *r, int frames) {
    const SideResult *a = &r->side[0];
    printf("    %-20s %6llu %6llu %7llu %7llu %6u %9.2f %9.2f %6.0f\n", name,
           (unsigned long long)a->rb.stalls, (unsigned long long)a->waits, (unsigned long long)a->rb.rollbacks,
           (unsigned long long)a->rb.resim_frames, a->rb.max_depth, a->avg_tick_us, a->max_tick_us,
           (double)a->net.bytes_sent * 60.0 / frames);
}

static void test_convergence_matrix(void) {
    print_header();
    for (unsigned i = 0; i < sizeof scenarios / sizeof *scenarios; i++) {
        const Scenario *sc = &scenarios[i];
        for (uint64_t seed = 1; seed <= 3; seed++) {
            PairCfg c = pair_defaults();
            c.net = (SimNetCfg){sc->delay, sc->jitter, sc->loss, sc->dup, sc->reorder, 4 * sc->jitter + 20};
            c.input_delay = sc->input_delay;
            c.max_prediction = sc->maxpred;
            c.seed = seed;
            PairResult r;
            run_pair(&c, &r);
            CHECK(r.finished, "%s seed %llu: did not finish (virtual %.0f ms)", sc->name, (unsigned long long)seed, r.virtual_ms);
            for (int p = 0; p < 2; p++) {
                CHECK(r.side[p].state_hash == r.reference_hash, "%s seed %llu: side %d differs from the no-network match", sc->name, (unsigned long long)seed, p);
                CHECK(!r.side[p].desync, "%s seed %llu: false desync alarm on side %d", sc->name, (unsigned long long)seed, p);
                CHECK(r.side[p].rb.conflicts == 0, "conflicting inputs");
            }
            if (seed == 1) print_row(sc->name, &r, c.frames);
        }
    }
}

static void test_desync_fault_detected(void) {
    for (int delay = 0; delay <= 100; delay += 50) {
        PairCfg c = pair_defaults();
        c.net = (SimNetCfg){delay ? delay : 1, 5, 0.02, 0, 0.05, 40};
        c.input_delay = 1;
        c.bug_b = TOY_BUG_FAULT;
        c.fault_frame = 600;
        PairResult r;
        run_pair(&c, &r);
        for (int p = 0; p < 2; p++) {
            CHECK(r.side[p].desync, "delay %d: side %d did not notice that the match diverged", delay, p);
            CHECK(r.side[p].desync_frame > 600 && r.side[p].desync_frame <= 600 + c.checksum_interval + 1,
                  "delay %d: side %d flagged frame %d (fault at 600, interval %d)", delay, p, r.side[p].desync_frame, c.checksum_interval);
        }
        CHECK(r.side[0].state_hash != r.side[1].state_hash, "the fault should have made the states differ");
        if (r.side[0].desync) printf("    fault at frame 600, one-way %d ms: flagged frame %d at %.0f ms (fault simulated at ~%.0f ms)\n",
                                     delay, r.side[0].desync_frame, r.side[0].desync_at_ms, 600 * 1000.0 / 60);
    }
}

static void test_unsaved_state_detected(void) {
    /* the classic rollback bug: something the simulation reads is not in the saved state */
    PairCfg c = pair_defaults();
    c.net = (SimNetCfg){60, 10, 0.02, 0, 0.05, 50};
    c.input_delay = 1;
    c.bug_b = TOY_BUG_UNSAVED;
    PairResult r;
    run_pair(&c, &r);
    CHECK(r.side[0].desync && r.side[1].desync, "an unsaved counter read by the simulation went unnoticed (a=%d b=%d)", r.side[0].desync, r.side[1].desync);
    CHECK(r.side[0].state_hash != r.side[1].state_hash, "states should have diverged");
}

static void test_pacing(void) {
    /* side 1's clock is 3% slow: it makes 58.2 frames/s where side 0 makes 60 */
    PairResult on, off;
    PairCfg c = pair_defaults();
    c.net = (SimNetCfg){40, 5, 0.01, 0, 0.02, 30};
    c.input_delay = 1; c.max_prediction = 10; c.drift_b = 0.03;
    c.pace = 0; run_pair(&c, &off);
    c.pace = 1; run_pair(&c, &on);
    CHECK(off.finished && on.finished, "finished off=%d on=%d", off.finished, on.finished);
    CHECK(on.side[0].state_hash == on.reference_hash && on.side[1].state_hash == on.reference_hash, "converged (pacing on)");
    CHECK(off.side[0].state_hash == off.reference_hash && off.side[1].state_hash == off.reference_hash, "converged (pacing off)");
    printf("    3%% clock drift, 40 ms: side 0 stalls %llu without pacing, %llu with (waits %llu); max depth %u / %u\n",
           (unsigned long long)off.side[0].rb.stalls, (unsigned long long)on.side[0].rb.stalls,
           (unsigned long long)on.side[0].waits, off.side[0].rb.max_depth, on.side[0].rb.max_depth);
    CHECK(on.side[0].rb.max_depth <= off.side[0].rb.max_depth + 1, "pacing should not deepen rollbacks");
}

/* a transport that feeds the protocol one canned datagram */
typedef struct { const uint8_t *d; size_t n; int given; } Canned;
static int canned_send(void *u, const void *d, size_t n) { (void)u; (void)d; (void)n; return 0; }
static int canned_recv(void *u, void *b, size_t cap) {
    Canned *c = u;
    if (c->given) return 0;
    c->given = 1;
    memcpy(b, c->d, c->n < cap ? c->n : cap);
    return (int)c->n;
}

static void test_protocol_rejects_garbage(void) {
    Toy t; toy_init(&t, 2);
    RbGame g = toy_game(&t);
    RbConfig cfg = {2, 0, 8, 0, 8, 0};
    RbSession *s = rb_create(&g, &cfg);
    uint8_t good[RBNET_HEADER + 8];
    memset(good, 0, sizeof good);
    good[0] = 'R'; good[1] = 'B'; good[2] = RBNET_VERSION; good[3] = 1;
    good[4] = 0xEE; good[5] = 0xFF; good[6] = 0xC0;                 /* session 0x00C0FFEE */
    good[8] = 1; good[9] = 1;                                       /* from player 1, one input */
    good[12] = 0; good[16] = 0xFF; good[17] = 0xFF; good[18] = 0xFF; good[19] = 0xFF;
    good[28] = 0xFF; good[29] = 0xFF; good[30] = 0xFF; good[31] = 0xFF;
    good[RBNET_HEADER] = 5;
    struct { const char *what; size_t len; int idx; uint8_t val; } bad[] = {
        {"wrong magic", sizeof good, 0, 'X'}, {"wrong version", sizeof good, 2, 9}, {"wrong session", sizeof good, 4, 0x11},
        {"wrong sender", sizeof good, 8, 0}, {"count too large", sizeof good, 9, 200}, {"truncated", sizeof good - 3, 0, 'R'},
        {"count disagrees with length", sizeof good, 9, 2},
    };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        uint8_t pkt[sizeof good];
        memcpy(pkt, good, sizeof good);
        pkt[bad[i].idx] = bad[i].val;
        Canned c = {pkt, bad[i].len, 0};
        RbTransport tr = {&c, canned_send, canned_recv};
        RbNet *n = rbnet_create(s, 1, &tr, 0xC0FFEE);
        rbnet_pump(n);
        CHECK(rbnet_stats(n)->rejected == 1 && rbnet_stats(n)->inputs_new == 0, "%s was not rejected", bad[i].what);
        rbnet_destroy(n);
    }
    Canned c = {good, sizeof good, 0};
    RbTransport tr = {&c, canned_send, canned_recv};
    RbNet *n = rbnet_create(s, 1, &tr, 0xC0FFEE);
    rbnet_pump(n);
    CHECK(rbnet_stats(n)->inputs_new == 1 && rb_confirmed_upto(s, 1) == 0, "the good packet should have been accepted");
    rbnet_destroy(n);
    rb_destroy(s);
}

static void test_bandwidth(void) {
    PairCfg c = pair_defaults();
    c.net = (SimNetCfg){60, 10, 0.03, 0, 0.05, 50};
    c.input_delay = 2;
    PairResult r;
    run_pair(&c, &r);
    double bps = (double)r.side[0].net.bytes_sent * 60.0 / c.frames;
    printf("    60 ms, 3%% loss: %.0f bytes/s per direction (%.1f bytes/packet, game's own: ~144)\n", bps,
           (double)r.side[0].net.bytes_sent / (double)r.side[0].net.packets_sent);
    CHECK(bps < 20000, "bandwidth %.0f B/s", bps);
}

void test_net_all(void) {
    RUN(test_protocol_rejects_garbage);
    RUN(test_convergence_matrix);
    RUN(test_desync_fault_detected);
    RUN(test_unsaved_state_detected);
    RUN(test_pacing);
    RUN(test_bandwidth);
}
