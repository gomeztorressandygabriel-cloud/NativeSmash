/* Input protocol over an unreliable datagram transport (UDP in practice).
 *
 * Every packet carries all the sender's inputs the peer has not acknowledged yet (so a lost
 * packet costs nothing as long as a later one arrives), the highest contiguous input frame the
 * sender holds of the peer (the acknowledgement), the sender's current frame and frame
 * advantage (for pacing) and its newest final state checksum (for desync detection).
 * Packets are idempotent and order-independent. The transport is a pair of callbacks, so the
 * relay / direct path of runtime/net_relay.c can be plugged in later without touching this. */
#ifndef RB_NET_H
#define RB_NET_H

#include "rollback.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RBNET_MAGIC0 'R'
#define RBNET_MAGIC1 'B'
#define RBNET_VERSION 1
#define RBNET_MAX_INPUTS 32            /* inputs a packet carries at most */
#define RBNET_HEADER 40
#define RBNET_WAIT_THRESHOLD 2         /* frames of lead (after halving) before pacing holds a side back */
#define RBNET_MAX_PACKET (RBNET_HEADER + 8 * RBNET_MAX_INPUTS)

typedef struct RbTransport {
    void *user;
    int (*send)(void *user, const void *data, size_t n);       /* best effort; may drop */
    int (*recv)(void *user, void *buf, size_t cap);            /* one datagram: its size, 0 if none, <0 on error */
} RbTransport;

typedef struct RbNetStats {
    uint64_t packets_sent, packets_recv, bytes_sent, bytes_recv;
    uint64_t rejected;                 /* malformed / wrong session / wrong sender */
    uint64_t inputs_new;               /* remote inputs accepted for the first time */
    uint64_t inputs_redundant;         /* ... received again (the price of redundancy) */
} RbNetStats;

typedef struct RbNet RbNet;

/* One RbNet per remote player. `session_id` must be equal on both sides (it keeps stray
 * datagrams of another match out). */
RbNet *rbnet_create(RbSession *s, int remote_player, const RbTransport *t, uint32_t session_id);
void rbnet_destroy(RbNet *n);

int rbnet_pump(RbNet *n);              /* read everything waiting and feed the session; returns packets handled */
void rbnet_send(RbNet *n);             /* send one packet (call once per tick, after rb_tick) */

/* Frames to hold back to let a peer that runs slower catch up (0: run). Both sides compute
 * it from the advantages they exchange, so only the one that is ahead waits. */
int rbnet_wait_frames(const RbNet *n);
int32_t rbnet_peer_ack(const RbNet *n);
const RbNetStats *rbnet_stats(const RbNet *n);

/* Wire format, little-endian:
 *   0  u8 'R'  u8 'B'  u8 version  u8 type(1)
 *   4  u32 session id
 *   8  u8 sender player  u8 count  u16 0
 *  12  i32 first frame carried
 *  16  i32 ack: highest contiguous frame the sender holds of the receiver (-1: none)
 *  20  i32 sender's current frame
 *  24  i32 sender's frame advantage (its frame minus the receiver's last frame it has heard)
 *  28  i32 checksum frame (-1: none)
 *  32  u64 checksum
 *  40  u64 input[count] */

#ifdef __cplusplus
}
#endif
#endif
