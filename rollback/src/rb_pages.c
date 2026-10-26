/* Page-delta snapshots: see include/rb_pages.h. */
#include "rb_pages.h"

#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t page; uint8_t *old, *neu; } Entry;   /* the page's copy before and after the interval */
typedef struct { int32_t frame; size_t n; Entry *e; } Delta;

struct RbPages {
    uint8_t *mem;
    size_t npages, psize;
    uint8_t *base;            /* the baseline copy: head[] points into it until a page is saved */
    uint8_t **head;           /* the newest saved copy of each page */
    Delta *ring;              /* oldest first, in a circular buffer */
    int cap, first, count;
    RbPagesStats stats;
};

static int owned(const RbPages *p, const uint8_t *page) {
    return page < p->base || page >= p->base + p->npages * p->psize;
}

static void release(RbPages *p, uint8_t *page) {
    if (owned(p, page)) { free(page); p->stats.pages_held--; }
}

RbPages *rbp_create(uint8_t *mem, size_t npages, size_t psize, int ring) {
    if (!mem || !npages || !psize || ring < 1) return NULL;
    RbPages *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->mem = mem; p->npages = npages; p->psize = psize; p->cap = ring + 1;
    p->base = malloc(npages * psize);
    p->head = malloc(npages * sizeof *p->head);
    p->ring = calloc((size_t)p->cap, sizeof *p->ring);
    if (!p->base || !p->head || !p->ring) { rbp_destroy(p); return NULL; }
    memcpy(p->base, mem, npages * psize);
    for (size_t i = 0; i < npages; i++) p->head[i] = p->base + i * psize;
    return p;
}

static Delta *at(RbPages *p, int i) { return &p->ring[(p->first + i) % p->cap]; }

static void expire_oldest(RbPages *p) {
    Delta *d = at(p, 0);
    for (size_t i = 0; i < d->n; i++) release(p, d->e[i].old);   /* (nothing can be restored to before it) */
    free(d->e);
    d->e = NULL; d->n = 0;
    p->first = (p->first + 1) % p->cap;
    p->count--;
}

void rbp_destroy(RbPages *p) {
    if (!p) return;
    while (p->count) {
        Delta *d = at(p, 0);
        for (size_t i = 0; i < d->n; i++) release(p, d->e[i].old);
        free(d->e);
        p->first = (p->first + 1) % p->cap;
        p->count--;
    }
    if (p->head) for (size_t i = 0; i < p->npages; i++) release(p, p->head[i]);
    free(p->ring); free(p->head); free(p->base); free(p);
}

int rbp_save(RbPages *p, int32_t frame, const uint32_t *dirty, size_t nd) {
    if (p->count && frame <= at(p, p->count - 1)->frame) {
        return frame == at(p, p->count - 1)->frame && !nd ? 0 : -1;
    }
    if (p->count == p->cap) expire_oldest(p);
    Delta *d = at(p, p->count);
    d->frame = frame;
    d->e = nd ? malloc(nd * sizeof *d->e) : NULL;
    if (nd && !d->e) return -1;
    d->n = 0;
    for (size_t i = 0; i < nd; i++) {
        uint32_t pg = dirty[i];
        if (pg >= p->npages) continue;
        uint8_t *copy = malloc(p->psize);
        if (!copy) continue;
        memcpy(copy, p->mem + (size_t)pg * p->psize, p->psize);
        d->e[d->n++] = (Entry){pg, p->head[pg], copy};
        p->head[pg] = copy;
        p->stats.pages_held++;
    }
    p->count++;
    p->stats.saves++;
    p->stats.pages_copied += d->n;
    return 0;
}

int rbp_load(RbPages *p, int32_t frame, const uint32_t *dirty, size_t nd) {
    int k = -1;
    for (int i = 0; i < p->count; i++) if (at(p, i)->frame == frame) k = i;
    if (k < 0) return -1;
    for (size_t i = 0; i < nd; i++) {   /* written since the last save: back to what that save held */
        if (dirty[i] >= p->npages) continue;
        memcpy(p->mem + (size_t)dirty[i] * p->psize, p->head[dirty[i]], p->psize);
        p->stats.pages_restored++;
    }
    while (p->count - 1 > k) {   /* undo the newer saves, newest first */
        Delta *d = at(p, p->count - 1);
        for (size_t i = d->n; i-- > 0;) {
            Entry *e = &d->e[i];
            memcpy(p->mem + (size_t)e->page * p->psize, e->old, p->psize);
            p->stats.pages_restored++;
            p->head[e->page] = e->old;
            release(p, e->neu);
        }
        free(d->e);
        d->e = NULL; d->n = 0;
        p->count--;
    }
    p->stats.loads++;
    return 0;
}

const RbPagesStats *rbp_stats(const RbPages *p) { return &p->stats; }
