/* A tiny deterministic "fighting game" for the tests: up to 4 fighters with position, velocity,
 * damage, a hit cooldown and a shared random generator, moved by packed inputs. Its state also
 * carries a block of words that changes slowly, to stand in for the bulk of a real game's state. */
#ifndef TOYGAME_H
#define TOYGAME_H

#include <stdint.h>
#include "rollback.h"

#define TOY_BLOB_WORDS 1024     /* 4 KB of "other state" */
#define TOY_LEFT   (1u << 0)
#define TOY_RIGHT  (1u << 1)
#define TOY_JUMP   (1u << 2)
#define TOY_ATTACK (1u << 3)

typedef struct ToyFighter {
    int32_t x, y, vx, vy;       /* 24.8 fixed point */
    int32_t damage, cooldown, facing, grounded;
} ToyFighter;

typedef struct ToyState {       /* (no padding: every field is 4 or 8 bytes, in order) */
    uint64_t rng;
    int32_t frame, nplayers;
    ToyFighter f[RB_MAX_PLAYERS];
    uint32_t blob[TOY_BLOB_WORDS];
} ToyState;

typedef enum { TOY_OK, TOY_BUG_UNSAVED, TOY_BUG_FAULT } ToyBug;

typedef struct Toy {
    ToyState s;
    /* things a real game has outside its saved state: */
    uint64_t sound_events;          /* side effects: only produced by frames simulated for the first time */
    uint64_t sound_suppressed;      /* ... the ones a replay would have repeated and did not */
    uint64_t resim_calls;           /* advance() calls with resimulating=1 */
    uint32_t unsaved;               /* TOY_BUG_UNSAVED: a counter nobody saves, which the simulation reads */
    ToyBug bug;
    int32_t fault_frame;            /* TOY_BUG_FAULT: corrupt one value when this frame is simulated */
} Toy;

void toy_init(Toy *t, int nplayers);
RbGame toy_game(Toy *t);
uint64_t toy_hash(const Toy *t);
/* the input player p "presses" at frame f in the scripted match (a pure function) */
RbInput toy_script(int p, int32_t f, uint64_t seed);

#endif
