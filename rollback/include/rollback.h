/* Rollback core: a small, dependency-free library (C99, no OS calls).
 *
 * The game (or a test game) is seen through four callbacks: save its simulation state into a
 * buffer, load it back, advance it one frame with every player's input, and (optionally) compute
 * a checksum of it. The library keeps a ring of saved states, predicts the inputs it has not
 * received yet (the last one known), notices when a real input differs from the prediction
 * ("late input"), loads the oldest wrong state and re-simulates up to the present, and compares
 * state checksums between players to detect desynchronisation.
 *
 * It does not know about sockets (rb_net.h builds a protocol on a transport abstraction) and
 * nothing here is specific to Super Smash Bros. Ultimate. See docs/ROLLBACK.md. */
#ifndef ROLLBACK_H
#define ROLLBACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RB_MAX_PLAYERS    4
#define RB_STATE_RING     32     /* saved states kept; max_prediction <= RB_STATE_RING - 2 */
#define RB_INPUT_RING     256    /* input frames kept per player */
#define RB_CHECK_SLOTS    64     /* checksum reports kept (one per checksum_interval frames) */

/* One player's input for one frame, packed by the game side (buttons + quantised sticks). */
typedef uint64_t RbInput;

typedef struct RbGame {
    void *user;
    size_t state_size;                                         /* bytes save() writes */
    void (*save)(void *user, void *buf);                       /* simulation state -> buf */
    void (*load)(void *user, const void *buf);                 /* buf -> simulation state */
    /* Simulate one frame. `resimulating` is 1 while replaying frames already shown once: the
     * game must not produce audio, rumble, screen effects or file output then. */
    void (*advance)(void *user, const RbInput *inputs, int resimulating);
    uint64_t (*checksum)(void *user);                          /* of the live state; NULL: hash of save() */
} RbGame;

typedef struct RbConfig {
    int num_players;          /* 2..RB_MAX_PLAYERS */
    int local_player;         /* index of the player this session's tick() input belongs to */
    int max_prediction;       /* frames the simulation may run ahead of the last confirmed input (1..RB_STATE_RING-2) */
    int input_delay;          /* frames the local input is held back (0..max_prediction); 0 = none */
    int checksum_interval;    /* compare a checksum every N frames (0: never) */
} RbConfig;

typedef enum {
    RB_OK = 0,
    RB_STALL = 1,             /* the prediction window is full: wait for remote inputs; nothing consumed */
    RB_ERR_ARG = -1,
    RB_ERR_FAR = -2,          /* remote input too far ahead of the present to keep: do not acknowledge it */
    RB_ERR_OLD = -3           /* remote input older than anything kept (already confirmed: ignore) */
} RbStatus;

typedef struct RbStats {
    uint64_t ticks;           /* rb_tick calls that advanced */
    uint64_t stalls;          /* rb_tick calls refused (window full) */
    uint64_t advances;        /* every call to game.advance, resimulated frames included */
    uint64_t resim_frames;    /* those that replayed a frame already shown */
    uint64_t rollbacks;       /* times a state was loaded */
    uint64_t late_inputs;     /* remote inputs that differed from what was used (prediction wrong) */
    uint64_t conflicts;       /* a remote input differing from one already received for the same frame */
    uint32_t max_depth;       /* deepest rollback in frames */
    uint64_t depth_hist[RB_STATE_RING];   /* rollbacks by depth */
    uint64_t desyncs;         /* checksum mismatches seen */
} RbStats;

typedef struct RbSession RbSession;

RbSession *rb_create(const RbGame *game, const RbConfig *cfg);
void rb_destroy(RbSession *s);

/* One frame of play. Gives the local input (it takes effect at frame current+input_delay),
 * first rolling back and replaying if remote inputs arrived that contradict the prediction.
 * RB_STALL: the window is full; the input was not consumed, call again with a fresh one. */
RbStatus rb_tick(RbSession *s, RbInput local);

/* Remote input for `frame` (idempotent; any order). */
RbStatus rb_add_remote_input(RbSession *s, int player, int32_t frame, RbInput in);

/* Roll back and replay what is pending without advancing (to compare states at a settled frame). */
int rb_resolve(RbSession *s);

/* Accessors for the protocol layer and the tests. */
int rb_local_player(const RbSession *s);
int32_t rb_frame(const RbSession *s);                          /* the frame the next tick simulates */
int32_t rb_confirmed_upto(const RbSession *s, int player);     /* last frame such that every input of `player` up to it is known (-1: none) */
int32_t rb_first_unconfirmed(const RbSession *s);              /* lowest frame lacking some player's input */
int32_t rb_local_ready(const RbSession *s);                    /* highest frame with a local input (-1: none) */
int rb_local_input(const RbSession *s, int32_t frame, RbInput *out);   /* 1 if kept */
const RbStats *rb_stats(const RbSession *s);

/* Checksums. A frame's checksum is final once all inputs before it are confirmed. */
int rb_final_checksum(const RbSession *s, int32_t *frame, uint64_t *sum);   /* newest final local one; 0 if none yet */
void rb_report_remote_checksum(RbSession *s, int player, int32_t frame, uint64_t sum);
int rb_desync(const RbSession *s, int32_t *frame, int *player);            /* 1 if a mismatch was seen (first one) */

uint64_t rb_hash64(const void *data, size_t n);                /* the library's checksum helper (not cryptographic) */

#ifdef __cplusplus
}
#endif
#endif
