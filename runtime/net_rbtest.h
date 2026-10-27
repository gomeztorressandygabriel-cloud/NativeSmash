/* Rollback rewind test (net_rbtest.c): can the game's simulation be saved, rewound and replayed to the
 * same state? docs/ROLLBACK.md, phase 3. On only with SSBU_RB_TEST=1 (or a file rollback_test.on beside
 * the game). Off, every hook is one load and a branch. */
#pragma once
#include "host.h"

extern volatile int rbt_state;   /* -1: not asked yet, 0 off, 1 on */
int rbt_probe(void);
static inline int rbt_on(void) {
    int s = rbt_state;
    return s > 0 || (s < 0 && rbt_probe());
}

void rbt_pad(u64 out, u32 size);   /* (hle_hid.c) a pad state the game just read: recorded, or replayed */
u64 rbt_ticks(u64 now);
int rbt_random(u8 *out, u64 n);     /* (hle_offline.c) the secure random bytes the game asks for: recorded, or replayed (1: done) */            /* (kernel.c) the system tick the game reads: recorded, or replayed */
