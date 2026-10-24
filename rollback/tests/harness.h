/* Runs two (or more) sessions of the toy game against each other over the simulated network,
 * in virtual time, and reports what happened. */
#ifndef HARNESS_H
#define HARNESS_H

#include "simnet.h"
#include "toygame.h"

typedef struct PairCfg {
    SimNetCfg net;
    int frames;                 /* simulated frames each side must reach */
    int input_delay;
    int max_prediction;
    int checksum_interval;
    uint64_t seed;
    double drift_b;             /* side 1 runs its frames this much slower (0.02: 2%) */
    int pace;                   /* 1: use rbnet_wait_frames to hold back the side that is ahead */
    ToyBug bug_b;               /* a bug on side 1 only (see toygame.h) */
    int32_t fault_frame;
} PairCfg;

typedef struct SideResult {
    RbStats rb;
    RbNetStats net;
    uint64_t waits;             /* ticks skipped for pacing */
    double avg_tick_us, max_tick_us;
    uint64_t state_hash;        /* the game's state at the frame reached */
    int desync, desync_found;
    int32_t desync_frame;
    double desync_at_ms;        /* virtual time of the detection */
    uint64_t sound_events, sound_suppressed;
} SideResult;

typedef struct PairResult {
    SideResult side[2];
    uint64_t reference_hash;    /* the same match played with no network at all */
    int finished;               /* both reached `frames` and settled before the time limit */
    double virtual_ms;
    uint64_t net_lost[2], net_sent[2];
} PairResult;

void run_pair(const PairCfg *cfg, PairResult *out);
PairCfg pair_defaults(void);

#endif
