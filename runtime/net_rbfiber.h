/* Fibers of the runtime's own, for Windows, whose state a rollback can save and restore
 * (net_rbfiber.c). Used by hle_fiber.c only while the rollback rewind test is on (net_rbtest.h);
 * otherwise the game's fibers stay Windows' own. Elsewhere (Linux, Android, macOS) the fibers are
 * already the runtime's own (linux/win32.c, macos/fiber.c), and these are not built. */
#pragma once
#include "host.h"

#ifdef _WIN32
void *rbf_convert(void *param);                                    /* ConvertThreadToFiberEx */
void *rbf_create(SIZE_T reserve, LPFIBER_START_ROUTINE fn, void *param);   /* CreateFiberEx */
void rbf_switch(void *to);                                         /* SwitchToFiber */
void rbf_delete(void *fiber);                                      /* DeleteFiber: kept, not freed (a rewind may need it back) */
void *rbf_data(void);                                              /* GetFiberData */
int rbf_is_fiber(void);                                            /* IsThreadAFiber */

/* For a rewind: a fiber's state - the part of its stack in use and where it stopped - saved and put
 * back. Only for a fiber that is not running (0 otherwise). */
typedef struct RbfSaved { void *fiber, *sp, *tib_base, *tib_limit, *tib_dealloc; u8 *stack; size_t bytes; } RbfSaved;
int rbf_save(void *fiber, RbfSaved *out);
int rbf_restore(const RbfSaved *s);
u64 rbf_hash(void *fiber);   /* (a check) where it stopped and its stack in use, as one number; 0 while it runs */
int rbf_running(void *fiber);
#endif
