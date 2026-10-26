/* Rollback probe for build\play.log (net_rbprobe.c): the measurements that decide whether rollback
 * netplay is feasible on this game (docs/ROLLBACK.md, phase 1). On only while a file called
 * rollback_probe.on exists beside the game (or SSBU_RB_PROBE=1). Off, every hook is one load
 * and a branch, and memory is reserved exactly as without it. */
#pragma once
#include "host.h"

extern volatile int rbp_state;   /* -1: not asked yet, 0 off, 1 on */
int rbp_probe(void);
static inline int rbp_on(void) {
    int s = rbp_state;
    return s > 0 || (s < 0 && rbp_probe());
}

void rbp_frame_queued(void);     /* (ipc_vi.c) the game queued a frame: the end of one of its frames */
void rbp_pad_read(void);         /* (hle_hid.c) the game read a pad's state */
void rbp_vsync_tick(void);       /* (ipc_vi.c) the vsync thread is about to wake the game for its next frame */
