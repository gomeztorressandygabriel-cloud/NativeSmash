/* Host runtime interface shared by the loader, the native kernel and native SDK code. */
#pragma once
#include <windows.h>

#include "module.h"

/* ---- guest address space ---------------------------------------------------------
 * Everything the guest can see lives below 2^39, as on the console. */
#define NATIVE_BASE  0x7300000000ull   /* guest addresses standing for native functions */
#define HEAP_BASE    0x7400000000ull   /* svcSetHeapSize */
#define HEAP_LIMIT   0x200000000ull
#define ALIAS_BASE   0x6000000000ull   /* 64 GiB, as on the console: the game reserves 62 GiB of it */
#define ALIAS_SIZE   0x1000000000ull
#define STACK_BASE   0x7700000000ull   /* thread stacks mapped by svcMapMemory, and the main stack */
#define STACK_SIZE   0x80000000ull
#define KERNEL_BASE  0x7780000000ull   /* thread-local regions */
#define SHARED_BASE  0x7800000000ull   /* memory system services share with the game (fonts) */
#ifdef __ANDROID__   /* (the part of the console's address space the guest's memory sits in: recomp.h's RT_MEMBASE) */
#define ASPACE_BASE  0x6000000000ull
#define ASPACE_SIZE  0x2000000000ull
#else
#define ASPACE_BASE  0x8000000ull
#define ASPACE_SIZE  0x7FF8000000ull
#endif

/* ---- native implementations of guest functions -------------------------------------
 * `HLE(name) { ... }` in any runtime source replaces the guest function exported under
 * that (mangled) name: the recompiler forwards the module's own function to it, and the
 * loader binds otherwise-undefined imports to it. Arguments arrive in the guest calling
 * convention (c->x[0..7], c->v[0..7]); results go back in c->x[0] / c->v[0]. */
#define HLE(sym) void hle_##sym(Cpu *c)
/* `HLE_WRAP(address) { ... }` wraps a guest function no module exports, at that (hex) address:
 * the recompiler keeps it as f_<address>_orig and sends every caller here instead. */
#define HLE_WRAP(addr) GuestFn f_##addr##_orig; void hle_wrap_##addr(Cpu *c)
/* Guest memory is mapped RT_MEMBASE above the guest's addresses (recomp.h: 0 but on macOS), so
 * a guest address handed to the host - as a pointer, to the C library, to the OS - goes through
 * these, as recomp.h's ld and st helpers do. */
#define P(T, addr) ((T *)RT_HOST(addr))                     /* guest pointer -> host pointer */
#define G2H(addr) RT_HOST(addr)                              /* guest address -> host pointer (void *) */
#define H2G(ptr) ((u64)(uintptr_t)(ptr) - RT_MEMBASE)        /* host pointer -> guest address */
#define RESULT(module, desc) ((u32)(module) | ((u32)(desc) << 9))
typedef struct HleEntry { const char *name; GuestFn *fn; } HleEntry;
extern const HleEntry hle_table[];
extern const u32 hle_count;

/* ---- loader ---------------------------------------------------------------------- */
void rt_call(Cpu *c, u64 guest_addr);                  /* call a guest function */
/* Call an exported guest function by name with up to six integer arguments; returns x0.
 * From native code running on a guest thread, using that thread's context. */
u64 rt_callsym(Cpu *c, const char *symbol, int n, ...);
u64 rt_now_ns(void);                                   /* monotonic time since the process started */
u64 rt_clock_ns(void);                                 /* monotonic time on the system timer's time line, cheap to ask */
u64 rt_guest_copy(const void *data, size_t size);      /* permanent guest-visible copy (e.g. a string) */
u64 rt_native(GuestFn *fn, const char *name);          /* guest address for a native function */
u64 rt_symbol(const char *name);                       /* guest address of an exported symbol, or 0 */
const char *rt_describe(u64 guest_addr, char *buf, size_t n);   /* "module+0x1234" */
void rt_backtrace(void);
void rt_backtrace_thread(HANDLE thread, int max_frames);   /* another thread's guest frames */
void kernel_dump_threads(void);
_Noreturn void rt_fatal(const char *fmt, ...);         /* message + guest backtrace, then exit */
extern int rt_trace;
extern const char *rt_data_dir;                        /* where the module images (and romfs.cfg) live */
u64 rt_load_nro(Cpu *c, u64 image);                     /* load the recompiled NRO; 0 if none matches */
void account_save_id(u64 uid[2]);                       /* (hle_env.c) the id naming a user's save folder */

/* ---- kernel ---------------------------------------------------------------------- */
void kernel_init(void);
void kernel_add_region(u64 base, u64 size, u32 state, u32 perm);   /* for svcQueryMemory */
Cpu *kernel_main_thread(u32 *handle);                  /* context + handle for the initial thread */
u32 kernel_new_session(void *session);                 /* a handle for a native IPC session */
void *kernel_session_of(u32 handle);
u32 kernel_create_event(void);                         /* a kernel event handle, for native code */
void fiber_counts(u32 *slots, u32 *finalized, u64 *made);   /* (hle_fiber.c) host fibers in use, spare; guest fibers started */
int fibers_save(int which, u32 *running);              /* (hle_fiber.c, the rollback rewind test) every fiber saved, in save 0 or 1; -1: not possible here */
int fibers_restore(int which, u32 *skipped, u32 *dropped);   /* ... and put back (as often as asked): how many; -1: not possible here */
enum { KT_RUNNING, KT_WAITING, KT_SLEEPING, KT_REQUESTING };
int kernel_thread_each(void (*each)(void *user, u64 id, int priority, int core, int state), void *user);   /* every guest thread alive and what it does (0: the kernel's lock is busy) */
int kernel_freeze(u64 *keys, u32 *n, u32 max, int match, u64 *busy_id);   /* (the rewind test) every other guest thread held in a wait (1: the kernel's lock kept until kernel_thaw) */
void kernel_thaw(void);
void kernel_set_fiber(void *thread, u64 fiber);   /* (hle_fiber.c) the guest fiber a thread switched to, for the rewind test */
u64 kernel_tlr_end(void);
u32 kernel_data_regions(u64 *base, u64 *size, u32 max);   /* the modules' writable data (their global variables) */   /* the thread-local regions in use end here (they start at KERNEL_BASE) */
u32 kernel_hold(const u64 *allow, u32 nallow);   /* (the rewind test, frozen) the other guest threads kept in their waits, but for allow */
void kernel_release(void);
u32 kernel_held_woken(u64 *last_id);   /* (the rewind test) held threads woken since the last call, and the last one's guest id */
void kernel_events(u8 *state, int out);   /* (the rewind test, frozen) the events' states (4096 bytes): out, saved into state; else put back */
u32 kernel_memory_regions(u64 *base, u64 *size, u32 max);   /* the guest's heap and stack regions (0: the kernel's lock is busy) */
HANDLE kernel_thread_host(u64 id);                     /* the host thread running a guest thread (NULL: none, or the kernel's lock busy) */

/* ---- native system services (ipc.c) ----------------------------------------------- */
void *ipc_connect(const char *port);                   /* a session on a named port, or NULL */
u32 ipc_request(Cpu *c, void *session, u64 message, u64 size);   /* handle one request in place */
void ipc_close(void *session);
void kernel_signal_event(u32 handle);
/* Point a guest nn::os::SystemEventType at a new kernel event; returns the event's handle. */
u32 rt_attach_system_event(Cpu *c, u64 system_event, int auto_clear);
