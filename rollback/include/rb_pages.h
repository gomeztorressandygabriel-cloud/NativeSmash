/* Page-delta snapshots of a big memory region: the alternative to copying all of it every frame.
 *
 * The caller says which pages were written since the previous call (on Windows, what
 * GetWriteWatch returns with WRITE_WATCH_FLAG_RESET; here, any list). Each save keeps copies of
 * only those pages, and a load undoes the newer saves' changes. Memory held is the baseline
 * (one copy of the region, made at creation) plus the pages dirtied in the last `ring` saves.
 *
 * Contract: saves use increasing frame numbers; a save of the newest frame again with no dirty
 * pages is a no-op (a rollback re-saves the frame it loaded). A dirty list holds each page
 * once. A load of frame f leaves the region as it was at the save of f and forgets newer saves. */
#ifndef RB_PAGES_H
#define RB_PAGES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RbPages RbPages;

typedef struct RbPagesStats {
    uint64_t saves, loads;
    uint64_t pages_copied;      /* saved */
    uint64_t pages_restored;    /* written back by loads */
    size_t pages_held;          /* copies alive now, baseline excluded */
} RbPagesStats;

RbPages *rbp_create(uint8_t *mem, size_t npages, size_t page_size, int ring);
void rbp_destroy(RbPages *p);
int rbp_save(RbPages *p, int32_t frame, const uint32_t *dirty, size_t ndirty);
int rbp_load(RbPages *p, int32_t frame, const uint32_t *dirty, size_t ndirty);
const RbPagesStats *rbp_stats(const RbPages *p);

#ifdef __cplusplus
}
#endif
#endif
