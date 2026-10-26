/* Netplay without rollback, and the check that makes it possible (net_lockstep.c; docs/NETPLAY.md):
 * the game's local VS with the other player's pad coming over the network, a few frames late on both
 * sides, both games advancing frame by frame together. Off unless SSBU_NETPLAY or SSBU_LOCKSTEP is set.
 * Off, every hook is one load and a branch. */
#pragma once
#include "host.h"

extern volatile int lks_state;   /* -1: not asked yet, 0 off, 1 on */
int lks_probe(void);
static inline int lks_on(void) {
    int s = lks_state;
    return s > 0 || (s < 0 && lks_probe());
}

typedef struct LksInput { u64 buttons; i32 lx, ly, rx, ry; } LksInput;   /* one player's pad for a frame */

void lks_input_step(void);       /* (net_rbtest.c) the game's input step begins: the next frame's pads */
void lks_sim_begin(void);        /* (net_rbtest.c) a frame's simulation begins on the main thread ... */
void lks_sim_end(void);          /* ... and ends */
int lks_pad(u32 id, LksInput *in, u64 *sampling);   /* (hle_hid.c) player id's pad now; 0: not ours */
int lks_style(u32 id, u32 *style);                  /* (hle_hid.c) whether player id is connected, in netplay; 0: not ours */
u64 lks_ticks(u64 now);          /* (kernel.c) the system tick the game's main thread reads */
int lks_picture_due(u32 *frame); /* (gpu_d3d11.c) a picture of the frame now, for comparing two runs: SSBU_LOCKSTEP_PICTURES frames apart */

void hid_local_input(LksInput *in);
/* (hle_net.c) A datagram for this player's virtual sockets: 1 if it is the VS netplay's own (through the online lobby), taken here. */
int lks_deliver(u32 src_ip, u16 src_port, u16 dst_port, const void *data, u32 len);   /* (hle_hid.c) this PC's player: its pad, keyboard and script, as player 1's */
