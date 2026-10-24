#include "toygame.h"

#include <string.h>

#define STAGE_W (400 << 8)
#define GROUND  0

static uint32_t rng_next(uint64_t *r) {
    *r ^= *r << 13; *r ^= *r >> 7; *r ^= *r << 17;
    return (uint32_t)(*r >> 32);
}

void toy_init(Toy *t, int n) {
    memset(t, 0, sizeof *t);
    t->s.rng = 0x1234567890ABCDEFull;
    t->s.nplayers = n;
    for (int i = 0; i < n; i++) {
        t->s.f[i].x = (100 + 60 * i) << 8;
        t->s.f[i].facing = i & 1 ? -1 : 1;
        t->s.f[i].grounded = 1;
    }
    for (int i = 0; i < TOY_BLOB_WORDS; i++) t->s.blob[i] = (uint32_t)i * 2654435761u;
}

static void toy_save(void *u, void *buf) { memcpy(buf, &((Toy *)u)->s, sizeof(ToyState)); }
static void toy_load(void *u, const void *buf) { memcpy(&((Toy *)u)->s, buf, sizeof(ToyState)); }

static void toy_advance(void *u, const RbInput *in, int resim) {
    Toy *t = u;
    ToyState *s = &t->s;
    t->resim_calls += (uint64_t)resim;
    if (t->bug == TOY_BUG_FAULT && s->frame == t->fault_frame) s->f[0].x ^= 0x100;
    for (int i = 0; i < s->nplayers; i++) {
        ToyFighter *f = &s->f[i];
        uint32_t b = (uint32_t)in[i];
        if (b & TOY_LEFT) { f->vx -= 40; f->facing = -1; }
        if (b & TOY_RIGHT) { f->vx += 40; f->facing = 1; }
        if ((b & TOY_JUMP) && f->grounded) { f->vy = 1800; f->grounded = 0; }
        if (f->cooldown > 0) f->cooldown--;
        if ((b & TOY_ATTACK) && !f->cooldown) {
            f->cooldown = 20;
            for (int j = 0; j < s->nplayers; j++) {
                if (j == i) continue;
                int32_t dx = (s->f[j].x - f->x) * f->facing;
                int32_t dy = s->f[j].y - f->y;
                if (dx > 0 && dx < (40 << 8) && dy > -(30 << 8) && dy < (30 << 8)) {
                    s->f[j].damage += 4 + (int32_t)(rng_next(&s->rng) % 3);
                    s->f[j].vx += f->facing * (300 + s->f[j].damage * 8);
                    if (!resim) t->sound_events++; else t->sound_suppressed++;   /* (a side effect, which must not repeat) */
                }
            }
        }
    }
    for (int i = 0; i < s->nplayers; i++) {
        ToyFighter *f = &s->f[i];
        f->vx = f->vx * 15 / 16;
        if (t->bug == TOY_BUG_UNSAVED && s->frame % 10 == 0) f->vx += (int32_t)(t->unsaved & 3);
        f->x += f->vx;
        if (f->x < 0) { f->x = 0; f->vx = 0; }
        if (f->x > STAGE_W) { f->x = STAGE_W; f->vx = 0; }
        if (!f->grounded) {
            f->vy -= 90;
            f->y += f->vy;
            if (f->y <= GROUND) { f->y = GROUND; f->vy = 0; f->grounded = 1; }
        }
    }
    s->blob[(uint32_t)(s->frame * 7) % TOY_BLOB_WORDS] ^= (uint32_t)s->f[0].x + (uint32_t)s->frame;
    s->frame++;
    t->unsaved++;
}

static uint64_t toy_checksum(void *u) { return rb_hash64(&((Toy *)u)->s, sizeof(ToyState)); }

RbGame toy_game(Toy *t) {
    RbGame g = {t, sizeof(ToyState), toy_save, toy_load, toy_advance, toy_checksum};
    return g;
}

uint64_t toy_hash(const Toy *t) { return rb_hash64(&t->s, sizeof(ToyState)); }

/* Buttons are held for a while and then change, as a person's are: every 8 frames a new
 * decision, taken from a hash of (seed, player, which 8-frame slot). */
RbInput toy_script(int p, int32_t f, uint64_t seed) {
    uint64_t h = rb_hash64(&(uint64_t[]){seed, (uint64_t)p, (uint64_t)(f / 8)}, 24);
    uint32_t b = 0;
    switch (h & 7) { case 0: case 1: b |= TOY_LEFT; break; case 2: case 3: b |= TOY_RIGHT; break; default: break; }
    if ((h >> 8 & 15) == 0) b |= TOY_JUMP;
    if ((h >> 16 & 3) == 0) b |= TOY_ATTACK;
    return b;
}
