/* Rollback core: see include/rollback.h and docs/ROLLBACK.md. */
#include "rollback.h"

#include <stdlib.h>
#include <string.h>

#define NONE INT32_MAX

typedef struct { int32_t frame; uint64_t sum; int valid; } Final;

struct RbSession {
    RbGame game;
    RbConfig cfg;
    int32_t cur;                              /* the frame the next tick simulates */
    int32_t rollback_to;                      /* oldest frame whose used input was wrong (NONE: no rollback pending) */

    uint8_t *state;                           /* RB_STATE_RING slots of game.state_size bytes */
    uint8_t *scratch;                         /* one more, for checksums of buffers */
    int32_t state_frame[RB_STATE_RING];       /* the frame whose pre-state a slot holds (-1: none) */
    uint64_t state_sum[RB_STATE_RING];
    RbInput used[RB_STATE_RING][RB_MAX_PLAYERS];   /* the inputs the slot's frame was simulated with */

    RbInput in[RB_MAX_PLAYERS][RB_INPUT_RING];
    int32_t in_frame[RB_MAX_PLAYERS][RB_INPUT_RING];   /* the frame an input slot holds (-1: none) */
    int32_t conf_upto[RB_MAX_PLAYERS];        /* contiguous prefix of known inputs */
    int32_t local_ready;

    int32_t next_final;                       /* checksums: first frame not yet considered */
    Final local_final[RB_CHECK_SLOTS];
    Final remote_final[RB_MAX_PLAYERS][RB_CHECK_SLOTS];
    Final last_final;
    int desynced;
    int32_t desync_frame;
    int desync_player;

    RbStats stats;
};

uint64_t rb_hash64(const void *data, size_t n) {
    const uint8_t *p = data;
    uint64_t h = 0x9E3779B97F4A7C15ull ^ n;
    while (n >= 8) {
        uint64_t w;
        memcpy(&w, p, 8);
        h = (h ^ w) * 0xFF51AFD7ED558CCDull;
        h ^= h >> 32;
        p += 8; n -= 8;
    }
    uint64_t w = 0;
    memcpy(&w, p, n);
    h = (h ^ w) * 0xC4CEB9FE1A85EC53ull;
    h ^= h >> 29;
    return h;
}

RbSession *rb_create(const RbGame *game, const RbConfig *cfg) {
    if (!game || !cfg || !game->save || !game->load || !game->advance || !game->state_size) return NULL;
    if (cfg->num_players < 2 || cfg->num_players > RB_MAX_PLAYERS) return NULL;
    if (cfg->local_player < 0 || cfg->local_player >= cfg->num_players) return NULL;
    if (cfg->max_prediction < 1 || cfg->max_prediction > RB_STATE_RING - 2) return NULL;
    if (cfg->input_delay < 0 || cfg->input_delay > cfg->max_prediction) return NULL;
    if (cfg->checksum_interval < 0 || cfg->synctest_depth < 0 || cfg->synctest_depth > cfg->max_prediction) return NULL;
    RbSession *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->state = malloc((size_t)(RB_STATE_RING + 1) * game->state_size);
    if (!s->state) { free(s); return NULL; }
    s->scratch = s->state + (size_t)RB_STATE_RING * game->state_size;
    s->game = *game;
    s->cfg = *cfg;
    s->rollback_to = NONE;
    for (int i = 0; i < RB_STATE_RING; i++) s->state_frame[i] = -1;
    for (int p = 0; p < RB_MAX_PLAYERS; p++) {
        s->conf_upto[p] = -1;
        for (int i = 0; i < RB_INPUT_RING; i++) s->in_frame[p][i] = -1;
    }
    /* the frames before the first one the local input reaches are neutral */
    s->local_ready = -1;
    for (int f = 0; f < cfg->input_delay; f++) {
        s->in[cfg->local_player][f % RB_INPUT_RING] = 0;
        s->in_frame[cfg->local_player][f % RB_INPUT_RING] = f;
        s->local_ready = f;
    }
    s->conf_upto[cfg->local_player] = s->local_ready;
    return s;
}

void rb_destroy(RbSession *s) {
    if (!s) return;
    free(s->state);
    free(s);
}

static int input_known(const RbSession *s, int p, int32_t f) {
    return f >= 0 && s->in_frame[p][f % RB_INPUT_RING] == f;
}

/* The input to simulate `f` with: the real one if it is known, else the last one known. */
static RbInput input_for(const RbSession *s, int p, int32_t f) {
    if (input_known(s, p, f)) return s->in[p][f % RB_INPUT_RING];
    int32_t c = s->conf_upto[p];
    return input_known(s, p, c) ? s->in[p][c % RB_INPUT_RING] : 0;
}

static int32_t first_unconfirmed(const RbSession *s) {
    int32_t m = NONE;
    for (int p = 0; p < s->cfg.num_players; p++)
        if (s->conf_upto[p] + 1 < m) m = s->conf_upto[p] + 1;
    return m;
}

/* Save the state before frame f, then simulate it. */
static void sim_frame(RbSession *s, int32_t f, int resim) {
    int slot = f % RB_STATE_RING;
    uint8_t *buf = s->state + (size_t)slot * s->game.state_size;
    s->game.save(s->game.user, buf);
    s->state_frame[slot] = f;
    if (s->cfg.checksum_interval && f % s->cfg.checksum_interval == 0)
        s->state_sum[slot] = s->game.checksum ? s->game.checksum(s->game.user) : rb_hash64(buf, s->game.state_size);
    RbInput in[RB_MAX_PLAYERS];
    for (int p = 0; p < s->cfg.num_players; p++) {
        in[p] = input_for(s, p, f);
        s->used[slot][p] = in[p];
    }
    s->game.advance(s->game.user, in, resim);
    s->stats.advances++;
    if (resim) s->stats.resim_frames++;
}

static void compare_final(RbSession *s, int p, const Final *local, const Final *remote) {
    if (!local->valid || !remote->valid || local->frame != remote->frame) return;
    if (local->sum == remote->sum) return;
    s->stats.desyncs++;
    if (!s->desynced) { s->desynced = 1; s->desync_frame = local->frame; s->desync_player = p; }
}

static void update_finals(RbSession *s) {
    int iv = s->cfg.checksum_interval;
    if (!iv) return;
    int32_t limit = first_unconfirmed(s);
    if (limit > s->cur - 1) limit = s->cur - 1;
    if (limit > s->rollback_to) limit = s->rollback_to;   /* (a state is right up to the frame a rollback is owed from) */
    for (; s->next_final <= limit; s->next_final++) {
        int32_t f = s->next_final;
        if (f % iv || s->state_frame[f % RB_STATE_RING] != f) continue;
        Final *l = &s->local_final[(f / iv) % RB_CHECK_SLOTS];
        l->frame = f; l->sum = s->state_sum[f % RB_STATE_RING]; l->valid = 1;
        s->last_final = *l;
        for (int p = 0; p < s->cfg.num_players; p++)
            if (p != s->cfg.local_player) compare_final(s, p, l, &s->remote_final[p][(f / iv) % RB_CHECK_SLOTS]);
    }
}

int rb_resolve(RbSession *s) {
    if (s->rollback_to == NONE) return 0;
    int32_t t = s->rollback_to;
    s->rollback_to = NONE;
    int slot = t % RB_STATE_RING;
    if (s->state_frame[slot] != t || t >= s->cur) return 0;   /* (cannot happen within the prediction window) */
    uint32_t depth = (uint32_t)(s->cur - t);
    s->game.load(s->game.user, s->state + (size_t)slot * s->game.state_size);
    s->stats.rollbacks++;
    s->stats.depth_hist[depth < RB_STATE_RING ? depth : RB_STATE_RING - 1]++;
    if (depth > s->stats.max_depth) s->stats.max_depth = depth;
    for (int32_t f = t; f < s->cur; f++) sim_frame(s, f, 1);
    update_finals(s);
    return (int)depth;
}

static uint64_t live_checksum(RbSession *s) {
    if (s->game.checksum) return s->game.checksum(s->game.user);
    s->game.save(s->game.user, s->scratch);
    return rb_hash64(s->scratch, s->game.state_size);
}

/* Sync test: replay the last `depth` frames from their saved state, as a rollback would, and
 * see that the game ends where it was. Anything the simulation reads that save() leaves out,
 * and any non-determinism, shows up here on one machine. */
static void sync_test(RbSession *s) {
    int d = s->cfg.synctest_depth;
    if (!d || s->cur < d) return;
    int32_t t = s->cur - d;
    uint64_t before = live_checksum(s);
    s->game.load(s->game.user, s->state + (size_t)(t % RB_STATE_RING) * s->game.state_size);
    for (int32_t f = t; f < s->cur; f++) sim_frame(s, f, 1);
    if (live_checksum(s) != before) {
        s->stats.synctest_failures++;
        if (!s->desynced) { s->desynced = 1; s->desync_frame = s->cur; s->desync_player = -1; }
    }
}

RbStatus rb_tick(RbSession *s, RbInput local) {
    rb_resolve(s);
    if (s->cur - first_unconfirmed(s) >= s->cfg.max_prediction) {
        s->stats.stalls++;
        update_finals(s);
        return RB_STALL;
    }
    int lp = s->cfg.local_player;
    int32_t lf = s->local_ready + 1;   /* (== cur + input_delay) */
    s->in[lp][lf % RB_INPUT_RING] = local;
    s->in_frame[lp][lf % RB_INPUT_RING] = lf;
    s->local_ready = lf;
    s->conf_upto[lp] = lf;
    sim_frame(s, s->cur, 0);
    s->cur++;
    s->stats.ticks++;
    sync_test(s);
    update_finals(s);
    return RB_OK;
}

RbStatus rb_add_remote_input(RbSession *s, int p, int32_t f, RbInput in) {
    if (p < 0 || p >= s->cfg.num_players || p == s->cfg.local_player || f < 0) return RB_ERR_ARG;
    if (f <= s->conf_upto[p]) {   /* a duplicate: check it still agrees */
        if (!input_known(s, p, f)) return RB_ERR_OLD;
        if (s->in[p][f % RB_INPUT_RING] != in) s->stats.conflicts++;
        return RB_OK;
    }
    if (f >= first_unconfirmed(s) + RB_INPUT_RING) return RB_ERR_FAR;
    int slot = f % RB_INPUT_RING;
    if (input_known(s, p, f)) {
        if (s->in[p][slot] != in) s->stats.conflicts++;
        return RB_OK;
    }
    s->in[p][slot] = in;
    s->in_frame[p][slot] = f;
    while (input_known(s, p, s->conf_upto[p] + 1)) s->conf_upto[p]++;
    if (f < s->cur) {   /* simulated already, with a prediction unless it was known then */
        int ss = f % RB_STATE_RING;
        if (s->state_frame[ss] == f && s->used[ss][p] != in) {
            s->stats.late_inputs++;
            if (f < s->rollback_to) s->rollback_to = f;
        }
    }
    update_finals(s);
    return RB_OK;
}

void rb_report_remote_checksum(RbSession *s, int p, int32_t frame, uint64_t sum) {
    int iv = s->cfg.checksum_interval;
    if (!iv || p < 0 || p >= s->cfg.num_players || p == s->cfg.local_player || frame < 0 || frame % iv) return;
    Final *r = &s->remote_final[p][(frame / iv) % RB_CHECK_SLOTS];
    if (r->valid && r->frame == frame) return;   /* (reports repeat in every packet) */
    r->frame = frame; r->sum = sum; r->valid = 1;
    compare_final(s, p, &s->local_final[(frame / iv) % RB_CHECK_SLOTS], r);
}

int rb_final_checksum(const RbSession *s, int32_t *frame, uint64_t *sum) {
    if (!s->last_final.valid) return 0;
    *frame = s->last_final.frame; *sum = s->last_final.sum;
    return 1;
}

int rb_desync(const RbSession *s, int32_t *frame, int *player) {
    if (!s->desynced) return 0;
    if (frame) *frame = s->desync_frame;
    if (player) *player = s->desync_player;
    return 1;
}

int rb_local_player(const RbSession *s) { return s->cfg.local_player; }
int32_t rb_frame(const RbSession *s) { return s->cur; }
int32_t rb_confirmed_upto(const RbSession *s, int p) { return s->conf_upto[p]; }
int32_t rb_first_unconfirmed(const RbSession *s) { return first_unconfirmed(s); }
int32_t rb_local_ready(const RbSession *s) { return s->local_ready; }
const RbStats *rb_stats(const RbSession *s) { return &s->stats; }

int rb_local_input(const RbSession *s, int32_t f, RbInput *out) {
    int lp = s->cfg.local_player;
    if (!input_known(s, lp, f)) return 0;
    *out = s->in[lp][f % RB_INPUT_RING];
    return 1;
}
