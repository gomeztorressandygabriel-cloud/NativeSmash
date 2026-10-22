#ifndef CPU_SCHED_H
#define CPU_SCHED_H
/* Where the game's threads run, and that Windows doesn't slow them down to save power.
 *
 * A laptop processor like an i5-1335U has two kinds of cores: 2 fast ones (4 threads) and 8 slow ones. Left alone,
 * Windows may run the game's main or graphics thread on a slow core, or treat the process as one to run in its
 * "efficiency mode", and both show as a frame rate that jumps about and gets worse as the laptop warms up. So:
 *   - the process and its threads opt out of power throttling (EcoQoS), and keep the timer resolution they ask for;
 *   - the two threads that set the pace (the guest's main thread and the graphics one) each prefer a core of their own
 *     among the fast ones (a hint: Windows may still move them when it has to);
 *   - the shader compilers and the other background threads are held to the slow cores, so they don't take the
 *     fast ones from the game.
 * The process also runs at high priority (not real-time: that could starve Windows itself), so that a browser, a virus scan
 * or anything else in the background gives way to it. SSBU_PRIORITY=normal leaves it at normal.
 * Nothing happens to the cores on a processor whose cores are all alike. SSBU_CPU_QOS=0 leaves the power throttling alone and
 * SSBU_CPU_PLACE=0 leaves the cores alone (to compare). Static inline: no source file of its own to build. */
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

enum { CPU_ROLE_GAME, CPU_ROLE_MAIN, CPU_ROLE_GPU, CPU_ROLE_BACKGROUND };

typedef struct CpuSchedState {
    int ready, hybrid, qos, place;
    int nfast, nslow;
    ULONG fast_id[64], slow_id[64];
    int fast_cpu[64];          /* the logical processors of the fast cores, one per physical core first */
    int nfast_cores;           /* (distinct physical cores among the fast ones) */
    LONG next_hot;             /* the next fast core a pace-setting thread takes */
    volatile LONG samples[2];  /* (cpu_sched_sample) how often a pace-setting thread was seen on a fast / slow core */
    UCHAR is_fast[256];
} CpuSchedState;

static CpuSchedState *cpu_sched_state(void) {
    static CpuSchedState st;
    return &st;
}

/* (once) the processor's cores, from the CPU sets Windows reports: the ones with the highest efficiency class are the fast ones */
static void cpu_sched_init(void) {
    CpuSchedState *s = cpu_sched_state();
    if (s->ready) return;
    s->ready = 1;
    const char *q = getenv("SSBU_CPU_QOS"), *p = getenv("SSBU_CPU_PLACE");
    s->qos = q ? atoi(q) != 0 : 1;
    s->place = p ? atoi(p) != 0 : 1;
    typedef BOOL (WINAPI *GetCpuSets)(void *, ULONG, PULONG, HANDLE, ULONG);
    GetCpuSets get = (GetCpuSets)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemCpuSetInformation");
    if (!get) return;
    ULONG n = 0;
    get(NULL, 0, &n, NULL, 0);
    if (!n) return;
    unsigned char *buf = (unsigned char *)malloc(n);
    if (!buf || !get(buf, n, &n, NULL, 0)) { free(buf); return; }
    struct { ULONG id; int cpu, core, eff; } sets[64];
    int count = 0, best = 0, worst = 255;
    for (ULONG off = 0; off + 20 <= n && count < 64;) {
        ULONG size = *(ULONG *)(buf + off);
        if (size < 20) break;
        if (*(int *)(buf + off + 4) == 0) {   /* CpuSetInformation */
            sets[count].id = *(ULONG *)(buf + off + 8);
            sets[count].cpu = buf[off + 14];
            sets[count].core = buf[off + 15];
            sets[count].eff = buf[off + 18];
            if (sets[count].eff > best) best = sets[count].eff;
            if (sets[count].eff < worst) worst = sets[count].eff;
            count++;
        }
        off += size;
    }
    free(buf);
    if (best == worst) return;   /* all cores alike */
    s->hybrid = 1;
    int seen_core[64] = {0};
    for (int i = 0; i < count; i++) {
        if (sets[i].cpu < 256) s->is_fast[sets[i].cpu] = sets[i].eff == best;
        if (sets[i].eff == best) {
            s->fast_id[s->nfast++] = sets[i].id;
            if (sets[i].core < 64 && !seen_core[sets[i].core]) { seen_core[sets[i].core] = 1; s->fast_cpu[s->nfast_cores++] = sets[i].cpu; }
        } else
            s->slow_id[s->nslow++] = sets[i].id;
    }
    fprintf(stderr, "[cpu] hybrid processor: %d fast threads (%d cores), %d slow ones; power throttling %s, core placement %s\n",
            s->nfast, s->nfast_cores, s->nslow, s->qos ? "off" : "left alone", s->place ? "on" : "left alone");
}

typedef struct CpuPowerState { ULONG version, control, state; } CpuPowerState;

/* (once, at the start of the program) the process: no efficiency mode, and the timer resolution it asked for kept */
static void cpu_sched_process(void) {
    cpu_sched_init();
    CpuSchedState *s = cpu_sched_state();
    const char *pr = getenv("SSBU_PRIORITY");
    if (!(pr && !_stricmp(pr, "normal"))) {
        if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) fprintf(stderr, "[cpu] the game runs at high priority\n");
        SetProcessPriorityBoost(GetCurrentProcess(), FALSE);   /* (FALSE: the boosts Windows gives a thread that was waiting stay on) */
    }
    if (!s->qos) return;
    typedef BOOL (WINAPI *SetInfo)(HANDLE, int, void *, DWORD);
    SetInfo set = (SetInfo)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetProcessInformation");
    if (!set) return;
    CpuPowerState st = {1, 0x1 | 0x4, 0};   /* EXECUTION_SPEED and IGNORE_TIMER_RESOLUTION: controlled by us, and off */
    if (!set(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &st, sizeof st)) {
        st.control = 0x1;
        set(GetCurrentProcess(), 4, &st, sizeof st);   /* (older Windows 10 has no timer resolution flag) */
    }
}

/* (at the start of a thread) its role: see the top */
static void cpu_sched_thread(int role) {
    cpu_sched_init();
    CpuSchedState *s = cpu_sched_state();
    if (s->qos && role != CPU_ROLE_BACKGROUND) {
        typedef BOOL (WINAPI *SetInfo)(HANDLE, int, void *, DWORD);
        SetInfo set = (SetInfo)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetThreadInformation");
        CpuPowerState st = {1, 0x1, 0};
        if (set) set(GetCurrentThread(), 3 /* ThreadPowerThrottling */, &st, sizeof st);
    }
    if (!s->hybrid || !s->place) return;
    if (role == CPU_ROLE_BACKGROUND) {
        typedef BOOL (WINAPI *SetSets)(HANDLE, const ULONG *, ULONG);
        SetSets sets = (SetSets)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetThreadSelectedCpuSets");
        if (sets && s->nslow) sets(GetCurrentThread(), s->slow_id, (ULONG)s->nslow);
    } else if ((role == CPU_ROLE_MAIN || role == CPU_ROLE_GPU) && s->nfast_cores) {
        int cpu = s->fast_cpu[(InterlockedIncrement(&s->next_hot) - 1) % s->nfast_cores];
        PROCESSOR_NUMBER pn = {0, (BYTE)cpu, 0};
        SetThreadIdealProcessorEx(GetCurrentThread(), &pn, NULL);
    }
}

/* (from a pace-setting thread, now and then) which kind of core it is on, for the status lines */
static void cpu_sched_sample(void) {
    CpuSchedState *s = cpu_sched_state();
    if (!s->hybrid) return;
    PROCESSOR_NUMBER pn;
    GetCurrentProcessorNumberEx(&pn);
    InterlockedIncrement(&s->samples[s->is_fast[pn.Number] ? 0 : 1]);
}

static void cpu_sched_status(void) {
    CpuSchedState *s = cpu_sched_state();
    if (!s->hybrid) return;
    LONG f = s->samples[0], w = s->samples[1];
    if (f + w) fprintf(stderr, "[status] cores: the pace-setting threads were on a fast core %.0f%% of the time (%ld samples)\n", 100.0 * f / (f + w), (long)(f + w));
    s->samples[0] = s->samples[1] = 0;
}
#else
enum { CPU_ROLE_GAME, CPU_ROLE_MAIN, CPU_ROLE_GPU, CPU_ROLE_BACKGROUND };
static inline void cpu_sched_process(void) {}
static inline void cpu_sched_thread(int role) { (void)role; }
static inline void cpu_sched_sample(void) {}
static inline void cpu_sched_status(void) {}
#endif
#endif
