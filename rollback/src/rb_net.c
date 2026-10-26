/* Input protocol: see include/rb_net.h. */
#include "rb_net.h"

#include <stdlib.h>
#include <string.h>

struct RbNet {
    RbSession *s;
    int remote;
    RbTransport t;
    uint32_t session_id;
    int32_t peer_ack;            /* the peer holds all our inputs up to here */
    int32_t peer_frame;          /* the peer's newest frame heard of (-1: none) */
    int32_t peer_advantage;      /* what it reported */
    RbNetStats stats;
};

static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *p) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)p[i] << (8 * i); return v; }
static uint64_t get64(const uint8_t *p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i); return v; }

RbNet *rbnet_create(RbSession *s, int remote_player, const RbTransport *t, uint32_t session_id) {
    if (!s || !t || !t->send || !t->recv || remote_player < 0 || remote_player >= RB_MAX_PLAYERS) return NULL;
    RbNet *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    n->s = s; n->remote = remote_player; n->t = *t; n->session_id = session_id;
    n->peer_ack = -1; n->peer_frame = -1;
    return n;
}

void rbnet_destroy(RbNet *n) { free(n); }

static void handle(RbNet *n, const uint8_t *p, size_t len) {
    if (len < RBNET_HEADER || p[0] != RBNET_MAGIC0 || p[1] != RBNET_MAGIC1 || p[2] != RBNET_VERSION || p[3] != 1 ||
        get32(p + 4) != n->session_id || p[8] != n->remote) { n->stats.rejected++; return; }
    unsigned count = p[9];
    if (count > RBNET_MAX_INPUTS || len != RBNET_HEADER + 8 * (size_t)count) { n->stats.rejected++; return; }
    int32_t first = (int32_t)get32(p + 12), ack = (int32_t)get32(p + 16), frame = (int32_t)get32(p + 20);
    int32_t adv = (int32_t)get32(p + 24), cframe = (int32_t)get32(p + 28);
    if (first < 0 && count) { n->stats.rejected++; return; }
    n->stats.packets_recv++; n->stats.bytes_recv += len;
    if (ack > n->peer_ack) n->peer_ack = ack;
    if (frame > n->peer_frame) { n->peer_frame = frame; n->peer_advantage = adv; }
    for (unsigned i = 0; i < count; i++) {
        int32_t f = first + (int32_t)i;
        int known = f <= rb_confirmed_upto(n->s, n->remote);
        RbStatus r = rb_add_remote_input(n->s, n->remote, f, get64(p + RBNET_HEADER + 8 * i));
        if (r == RB_OK) { if (known) n->stats.inputs_redundant++; else n->stats.inputs_new++; }
        else if (r == RB_ERR_FAR) break;   /* (not stored, and not acknowledged either: it will be sent again) */
    }
    if (cframe >= 0) rb_report_remote_checksum(n->s, n->remote, cframe, get64(p + 32));
}

int rbnet_pump(RbNet *n) {
    uint8_t buf[RBNET_MAX_PACKET + 64];
    int handled = 0;
    for (;;) {
        int r = n->t.recv(n->t.user, buf, sizeof buf);
        if (r <= 0) break;
        handle(n, buf, (size_t)r);
        handled++;
    }
    return handled;
}

void rbnet_send(RbNet *n) {
    uint8_t buf[RBNET_MAX_PACKET];
    int32_t first = n->peer_ack + 1, ready = rb_local_ready(n->s);
    unsigned count = ready >= first ? (unsigned)(ready - first + 1) : 0;
    if (count > RBNET_MAX_INPUTS) count = RBNET_MAX_INPUTS;   /* (the rest follows as acknowledgements arrive) */
    memset(buf, 0, RBNET_HEADER);
    buf[0] = RBNET_MAGIC0; buf[1] = RBNET_MAGIC1; buf[2] = RBNET_VERSION; buf[3] = 1;
    put32(buf + 4, n->session_id);
    buf[8] = (uint8_t)rb_local_player(n->s);
    buf[9] = (uint8_t)count;
    put32(buf + 12, (uint32_t)(count ? first : 0));
    put32(buf + 16, (uint32_t)rb_confirmed_upto(n->s, n->remote));
    put32(buf + 20, (uint32_t)rb_frame(n->s));
    put32(buf + 24, (uint32_t)(rb_frame(n->s) - (n->peer_frame < 0 ? 0 : n->peer_frame)));
    int32_t cf = -1;
    uint64_t sum = 0;
    if (!rb_final_checksum(n->s, &cf, &sum)) cf = -1;
    put32(buf + 28, (uint32_t)cf);
    put64(buf + 32, sum);
    unsigned kept = 0;
    for (; kept < count; kept++) {
        RbInput in;
        if (!rb_local_input(n->s, first + (int32_t)kept, &in)) break;   /* (older than the session keeps) */
        put64(buf + RBNET_HEADER + 8 * kept, in);
    }
    buf[9] = (uint8_t)kept;
    size_t len = RBNET_HEADER + 8 * (size_t)kept;
    n->t.send(n->t.user, buf, len);
    n->stats.packets_sent++; n->stats.bytes_sent += len;
}

int rbnet_wait_frames(const RbNet *n) {
    if (n->peer_frame < 0) return 0;
    int mine = rb_frame(n->s) - n->peer_frame, theirs = n->peer_advantage;
    int w = (mine - theirs) / 2;   /* (the one-way delay is in both advantages and cancels) */
    return w >= RBNET_WAIT_THRESHOLD ? w : 0;   /* (jitter and loss make the estimate noisy: act on a clear lead only) */
}

int32_t rbnet_peer_ack(const RbNet *n) { return n->peer_ack; }
const RbNetStats *rbnet_stats(const RbNet *n) { return &n->stats; }
