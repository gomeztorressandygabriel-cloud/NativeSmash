#include "simnet.h"

#include <stdlib.h>
#include <string.h>

typedef struct { double at; size_t len; uint8_t data[RBNET_MAX_PACKET]; } Packet;
typedef struct { Packet *p; size_t n, cap; uint64_t sent, lost; } Queue;

struct SimNet {
    SimNetCfg cfg;
    uint64_t rng;
    double now;
    Queue q[2];                 /* q[i]: packets heading to endpoint i */
    void *h[2][2];              /* the transports' handles: (net, side) */
};

static double frand(SimNet *n) {
    n->rng ^= n->rng << 13; n->rng ^= n->rng >> 7; n->rng ^= n->rng << 17;
    return (double)(n->rng >> 11) / 9007199254740992.0;
}

SimNet *simnet_create(const SimNetCfg *cfg, uint64_t seed) {
    SimNet *n = calloc(1, sizeof *n);
    n->cfg = *cfg;
    n->rng = seed * 0x9E3779B97F4A7C15ull | 1;
    for (int i = 0; i < 8; i++) frand(n);
    return n;
}

void simnet_destroy(SimNet *n) {
    if (!n) return;
    free(n->q[0].p); free(n->q[1].p); free(n);
}

void simnet_set_now(SimNet *n, double now) { n->now = now; }

static void push(Queue *q, const void *data, size_t len, double at) {
    if (q->n == q->cap) { q->cap = q->cap ? q->cap * 2 : 64; q->p = realloc(q->p, q->cap * sizeof *q->p); }
    Packet *p = &q->p[q->n++];
    p->at = at; p->len = len;
    memcpy(p->data, data, len);
}

static int net_send(void *u, const void *data, size_t len) {
    void **c = u;
    SimNet *n = c[0];
    int side = (int)(intptr_t)c[1];
    Queue *q = &n->q[side ^ 1];
    q->sent++;
    if (len > RBNET_MAX_PACKET) return -1;
    if (frand(n) < n->cfg.loss) { q->lost++; return 0; }
    for (int copies = 1 + (frand(n) < n->cfg.dup); copies; copies--) {
        double d = n->cfg.delay_ms + frand(n) * n->cfg.jitter_ms;
        if (frand(n) < n->cfg.reorder) d += frand(n) * n->cfg.reorder_ms;
        push(q, data, len, n->now + d);
    }
    return 0;
}

static int net_recv(void *u, void *buf, size_t cap) {
    void **c = u;
    SimNet *n = c[0];
    Queue *q = &n->q[(intptr_t)c[1]];
    size_t best = (size_t)-1;
    for (size_t i = 0; i < q->n; i++)
        if (q->p[i].at <= n->now && (best == (size_t)-1 || q->p[i].at < q->p[best].at)) best = i;
    if (best == (size_t)-1) return 0;
    size_t len = q->p[best].len < cap ? q->p[best].len : cap;
    memcpy(buf, q->p[best].data, len);
    q->p[best] = q->p[--q->n];
    return (int)len;
}

RbTransport simnet_transport(SimNet *n, int side) {
    n->h[side][0] = n;
    n->h[side][1] = (void *)(intptr_t)side;
    RbTransport t = {n->h[side], net_send, net_recv};
    return t;
}

uint64_t simnet_sent(const SimNet *n, int side) { return n->q[side ^ 1].sent; }
uint64_t simnet_lost(const SimNet *n, int side) { return n->q[side ^ 1].lost; }
