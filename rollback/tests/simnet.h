/* A deterministic simulated network between two endpoints: fixed delay plus jitter, loss,
 * duplication and reordering (some packets are held back longer), in virtual milliseconds. */
#ifndef SIMNET_H
#define SIMNET_H

#include <stddef.h>
#include <stdint.h>
#include "rb_net.h"

typedef struct SimNetCfg {
    double delay_ms;            /* one way */
    double jitter_ms;           /* uniform 0..jitter added */
    double loss;                /* probability a packet is dropped */
    double dup;                 /* probability it is delivered twice */
    double reorder;             /* probability it is held back by up to reorder_ms more */
    double reorder_ms;
} SimNetCfg;

typedef struct SimNet SimNet;
SimNet *simnet_create(const SimNetCfg *cfg, uint64_t seed);
void simnet_destroy(SimNet *n);
void simnet_set_now(SimNet *n, double now_ms);
/* A transport for endpoint `side` (0 or 1): it sends to the other one. */
RbTransport simnet_transport(SimNet *n, int side);
uint64_t simnet_sent(const SimNet *n, int side);
uint64_t simnet_lost(const SimNet *n, int side);

#endif
