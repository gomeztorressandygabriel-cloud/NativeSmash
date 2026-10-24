#include "harness.h"

#include <string.h>
#include <time.h>

PairCfg pair_defaults(void) {
    PairCfg c;
    memset(&c, 0, sizeof c);
    c.frames = 1800;
    c.max_prediction = 12;
    c.checksum_interval = 8;
    c.seed = 1;
    c.pace = 1;
    return c;
}

static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

typedef struct Side {
    Toy toy;
    RbSession *s;
    RbNet *net;
    double next_tick, period;
    double tick_us_sum, tick_us_max;
    uint64_t waits;
    int desync_found;
    double desync_at;
} Side;

void run_pair(const PairCfg *cfg, PairResult *out) {
    memset(out, 0, sizeof *out);
    const int n = 2;
    Toy ref;
    toy_init(&ref, n);
    for (int f = 0; f < cfg->frames; f++) {
        RbInput in[2];
        for (int p = 0; p < n; p++) in[p] = f < cfg->input_delay ? 0 : toy_script(p, f, cfg->seed);
        toy_game(&ref).advance(&ref, in, 0);
    }
    out->reference_hash = toy_hash(&ref);

    SimNet *net = simnet_create(&cfg->net, cfg->seed + 77);
    Side side[2];
    memset(side, 0, sizeof side);
    for (int i = 0; i < n; i++) {
        toy_init(&side[i].toy, n);
        if (i == 1) { side[i].toy.bug = cfg->bug_b; side[i].toy.fault_frame = cfg->fault_frame; }
        RbGame g = toy_game(&side[i].toy);
        RbConfig rc = {n, i, cfg->max_prediction, cfg->input_delay, cfg->checksum_interval, 0};
        side[i].s = rb_create(&g, &rc);
        RbTransport t = simnet_transport(net, i);
        side[i].net = rbnet_create(side[i].s, 1 - i, &t, 0xC0FFEE);
        side[i].period = 1000.0 / 60.0 * (i == 1 ? 1.0 + cfg->drift_b : 1.0);
        side[i].next_tick = i * 3.0;   /* (the clocks do not start together) */
    }

    double limit = cfg->frames * 1000.0 / 60.0 * 4 + 20000;
    double now = 0;
    for (; now < limit; now += 0.5) {
        simnet_set_now(net, now);
        for (int i = 0; i < n; i++) {
            Side *sd = &side[i];
            if (now < sd->next_tick) continue;
            sd->next_tick += sd->period;
            rbnet_pump(sd->net);
            if (rb_frame(sd->s) < cfg->frames) {
                if (cfg->pace && rbnet_wait_frames(sd->net) > 0) {
                    sd->waits++;
                } else {
                    RbInput in = toy_script(i, rb_frame(sd->s) + cfg->input_delay, cfg->seed);
                    double t0 = now_us();
                    RbStatus r = rb_tick(sd->s, in);
                    double dt = now_us() - t0;
                    sd->tick_us_sum += dt;
                    if (dt > sd->tick_us_max) sd->tick_us_max = dt;
                    (void)r;
                }
            } else {
                rb_resolve(sd->s);
            }
            rbnet_send(sd->net);
            if (!sd->desync_found && rb_desync(sd->s, NULL, NULL)) { sd->desync_found = 1; sd->desync_at = now; }
        }
        /* settled once each side reached `frames` with every input before it confirmed */
        int done = 1;
        for (int i = 0; i < n; i++)
            if (rb_frame(side[i].s) < cfg->frames || rb_first_unconfirmed(side[i].s) < cfg->frames) done = 0;
        if (done) {
            for (int i = 0; i < n; i++) rb_resolve(side[i].s);
            out->finished = 1;
            break;
        }
    }
    out->virtual_ms = now;
    for (int i = 0; i < n; i++) {
        Side *sd = &side[i];
        SideResult *r = &out->side[i];
        r->rb = *rb_stats(sd->s);
        r->net = *rbnet_stats(sd->net);
        r->waits = sd->waits;
        r->avg_tick_us = r->rb.ticks ? sd->tick_us_sum / (double)r->rb.ticks : 0;
        r->max_tick_us = sd->tick_us_max;
        r->state_hash = toy_hash(&sd->toy);
        r->desync = rb_desync(sd->s, &r->desync_frame, NULL);
        r->desync_found = sd->desync_found;
        r->desync_at_ms = sd->desync_at;
        r->sound_events = sd->toy.sound_events;
        r->sound_suppressed = sd->toy.sound_suppressed;
        out->net_sent[i] = simnet_sent(net, i);
        out->net_lost[i] = simnet_lost(net, i);
    }
    for (int i = 0; i < n; i++) { rbnet_destroy(side[i].net); rb_destroy(side[i].s); }
    simnet_destroy(net);
}
