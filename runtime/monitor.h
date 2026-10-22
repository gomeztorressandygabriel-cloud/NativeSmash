#ifndef MONITOR_H
#define MONITOR_H
/* The monitor: everything that can slow the game down, measured once a second while it runs.
 *
 * SSBU_MONITOR=1 (or `monitor = on` in display.ini) starts a thread that, each second, reads the frame counters (the frames the
 * game makes, shows and skips, how far apart it queues them), how busy the graphics thread is, how much processor each thread of
 * the game uses, the processor itself (use and clock of its fast and slow cores, its temperature), the memory (the game's, the
 * PC's, the graphics card's, what the PC is reading from the disk because it ran out), and writes
 *   build\monitor.csv       one row a second (kept: the whole run);
 *   build\monitor_now.txt   the last second, as text, rewritten each second (to read while the game runs);
 * and gives the same text to the details of the overlay (F2), which shows it over the picture.
 * Static inline: no source file of its own to build; Windows only (the others get nothing). */
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "cpu_sched.h"

/* what the other files give it (each counts what it knows) */
unsigned long long present_frames(void);
void present_skip_stats(unsigned long long *skipped, unsigned long long *queued);
void present_interval_stats(double *avg_ms, double *max_ms);   /* since the last call */
unsigned long long gpu_busy_ns_total(void);
extern unsigned long long gpu_stat_draws, gpu_stat_skipped_draws;
void gfx_monitor_stats(unsigned long long *gpu_used, unsigned long long *gpu_budget, unsigned *textures, unsigned long long *texture_bytes,
                       unsigned long long *surface_bytes, unsigned long long *shaders);

static char monitor_text_buf[2048];
static SRWLOCK monitor_text_lock = SRWLOCK_INIT;

/* the last second as text, for the overlay's details; "" while the monitor is off */
static void monitor_text(char *out, size_t size) {
    AcquireSRWLockShared(&monitor_text_lock);
    snprintf(out, size, "%s", monitor_text_buf);
    ReleaseSRWLockShared(&monitor_text_lock);
}

typedef LONG (WINAPI *Mon_PdhOpenQuery)(LPCWSTR, DWORD_PTR, HANDLE *);
typedef LONG (WINAPI *Mon_PdhAddCounter)(HANDLE, LPCWSTR, DWORD_PTR, HANDLE *);
typedef LONG (WINAPI *Mon_PdhCollect)(HANDLE);
typedef LONG (WINAPI *Mon_PdhArray)(HANDLE, DWORD, LPDWORD, LPDWORD, PDH_FMT_COUNTERVALUE_ITEM_W *);
typedef LONG (WINAPI *Mon_PdhValue)(HANDLE, DWORD, LPDWORD, PDH_FMT_COUNTERVALUE *);
typedef HRESULT (WINAPI *Mon_GetThreadDescription)(HANDLE, PWSTR *);
typedef BOOL (WINAPI *Mon_MemoryInfo)(HANDLE, void *, DWORD);

typedef struct MonThread { DWORD id; unsigned long long cpu_100ns; } MonThread;

static struct {
    Mon_PdhArray array_fn;
    Mon_PdhValue value_fn;
    HANDLE perf, util, temp, pages;
} mon_pdh;

/* average of an expanded counter over the logical processors that are fast (fast != 0) or slow (fast == 0); -1 when there are none */
static double mon_average_cores(HANDLE counter, int fast) {
    CpuSchedState *cs = cpu_sched_state();
    DWORD bytes = 0, count = 0;
    if (!counter || mon_pdh.array_fn(counter, PDH_FMT_DOUBLE, &bytes, &count, NULL) != ((LONG)0x800007D2L) || !bytes) return -1;
    PDH_FMT_COUNTERVALUE_ITEM_W *items = (PDH_FMT_COUNTERVALUE_ITEM_W *)malloc(bytes);
    double sum = 0;
    int n = 0;
    if (items && mon_pdh.array_fn(counter, PDH_FMT_DOUBLE, &bytes, &count, items) == ERROR_SUCCESS) {
        for (DWORD i = 0; i < count; i++) {
            int group = 0, cpu = -1;
            if (swscanf(items[i].szName, L"%d,%d", &group, &cpu) != 2 || cpu < 0 || cpu > 255) continue;   /* ("_Total" and the like) */
            if (cs->hybrid && (cs->is_fast[cpu] != 0) != (fast != 0)) continue;
            if (!cs->hybrid && !fast) continue;
            sum += items[i].FmtValue.doubleValue;
            n++;
        }
    }
    free(items);
    return n ? sum / n : -1;
}

static double mon_single(HANDLE counter, int maximum) {
    DWORD bytes = 0, count = 0;
    if (!counter || mon_pdh.array_fn(counter, PDH_FMT_DOUBLE, &bytes, &count, NULL) != ((LONG)0x800007D2L) || !bytes) {
        PDH_FMT_COUNTERVALUE v;
        return counter && mon_pdh.value_fn(counter, PDH_FMT_DOUBLE, NULL, &v) == ERROR_SUCCESS ? v.doubleValue : -1;
    }
    PDH_FMT_COUNTERVALUE_ITEM_W *items = (PDH_FMT_COUNTERVALUE_ITEM_W *)malloc(bytes);
    double best = -1;
    if (items && mon_pdh.array_fn(counter, PDH_FMT_DOUBLE, &bytes, &count, items) == ERROR_SUCCESS)
        for (DWORD i = 0; i < count; i++) {
            double v = items[i].FmtValue.doubleValue;
            if (maximum ? v > best : best < 0) best = v;
        }
    free(items);
    return best;
}

static DWORD WINAPI monitor_main(LPVOID unused) {
    (void)unused;
    SetThreadDescription(GetCurrentThread(), L"monitor");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    cpu_sched_thread(CPU_ROLE_BACKGROUND);
    CreateDirectoryA("build", NULL);
    FILE *csv = fopen("build\\monitor.csv", "w");
    if (!csv) return 0;
    fprintf(csv, "t,game_fps,shown_fps,skipped,interval_avg_ms,interval_max_ms,gpu_thread_pct,draws_per_s,process_cpu_pct,main_pct,gpu_pct,top_threads,"
                 "private_mb,avail_mb,page_faults_s,disk_pages_in_s,gpu_used_mb,gpu_budget_mb,textures,texture_mb,surface_mb,shaders,"
                 "fast_use,slow_use,fast_clock,slow_clock,temp_c\n");
    HMODULE pdh = LoadLibraryA("pdh.dll");
    Mon_PdhOpenQuery open_q = pdh ? (Mon_PdhOpenQuery)(void *)GetProcAddress(pdh, "PdhOpenQueryW") : NULL;
    Mon_PdhAddCounter add_c = pdh ? (Mon_PdhAddCounter)(void *)GetProcAddress(pdh, "PdhAddEnglishCounterW") : NULL;
    Mon_PdhCollect collect = pdh ? (Mon_PdhCollect)(void *)GetProcAddress(pdh, "PdhCollectQueryData") : NULL;
    mon_pdh.array_fn = pdh ? (Mon_PdhArray)(void *)GetProcAddress(pdh, "PdhGetFormattedCounterArrayW") : NULL;
    mon_pdh.value_fn = pdh ? (Mon_PdhValue)(void *)GetProcAddress(pdh, "PdhGetFormattedCounterValue") : NULL;
    HANDLE query = NULL;
    if (open_q && add_c && collect && mon_pdh.array_fn && mon_pdh.value_fn && open_q(NULL, 0, &query) == ERROR_SUCCESS) {
        add_c(query, L"\\Processor Information(*)\\% Processor Performance", 0, &mon_pdh.perf);
        add_c(query, L"\\Processor Information(*)\\% Processor Utility", 0, &mon_pdh.util);
        add_c(query, L"\\Thermal Zone Information(*)\\Temperature", 0, &mon_pdh.temp);
        add_c(query, L"\\Memory\\Pages Input/sec", 0, &mon_pdh.pages);
        collect(query);
    }
    Mon_GetThreadDescription get_desc = (Mon_GetThreadDescription)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadDescription");
    Mon_MemoryInfo mem_info = (Mon_MemoryInfo)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetProcessMemoryInfo");
    static MonThread prev[1024];
    int nprev = 0;
    LARGE_INTEGER freq, t0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    LARGE_INTEGER last = t0;
    unsigned long long last_shown = present_frames(), last_skipped = 0, last_queued = 0, last_busy = gpu_busy_ns_total(), last_draws = gpu_stat_draws + gpu_stat_skipped_draws;
    DWORD last_faults = 0;
    double interval_avg = 0, interval_max = 0;
    present_interval_stats(&interval_avg, &interval_max);
    present_skip_stats(&last_skipped, &last_queued);
    for (;;) {
        Sleep(1000);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double wall = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        double t = (double)(now.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        last = now;
        if (wall <= 0) continue;
        unsigned long long shown = present_frames(), skipped = 0, queued = 0, busy = gpu_busy_ns_total(), draws = gpu_stat_draws + gpu_stat_skipped_draws;
        present_skip_stats(&skipped, &queued);
        present_interval_stats(&interval_avg, &interval_max);
        double game_fps = (double)(queued - last_queued) / wall, shown_fps = (double)(shown - last_shown) / wall;
        double skipped_s = (double)(skipped - last_skipped) / wall, gpu_pct = (double)(busy - last_busy) / 1e9 / wall * 100.0, draws_s = (double)(draws - last_draws) / wall;
        last_shown = shown; last_skipped = skipped; last_queued = queued; last_busy = busy; last_draws = draws;
        /* the threads: how much of one core each took this second */
        struct { char name[40]; double pct; } top[4];
        memset(top, 0, sizeof top);
        double process_pct = 0, main_pct = 0, gpu_thread_pct = 0;
        MonThread cur[1024];
        int ncur = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te;
            te.dwSize = sizeof te;
            DWORD pid = GetCurrentProcessId();
            for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
                if (te.th32OwnerProcessID != pid || ncur >= 1024) continue;
                HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
                if (!h) continue;
                FILETIME c, e, k, u;
                if (GetThreadTimes(h, &c, &e, &k, &u)) {
                    unsigned long long total = (((unsigned long long)k.dwHighDateTime << 32) | k.dwLowDateTime) + (((unsigned long long)u.dwHighDateTime << 32) | u.dwLowDateTime);
                    cur[ncur].id = te.th32ThreadID; cur[ncur].cpu_100ns = total;
                    unsigned long long before = 0;
                    for (int i = 0; i < nprev; i++) if (prev[i].id == te.th32ThreadID) { before = prev[i].cpu_100ns; break; }
                    if (before) {
                        double pct = (double)(total - before) / 1e7 / wall * 100.0;
                        process_pct += pct;
                        char name[40];
                        snprintf(name, sizeof name, "thread %lu", (unsigned long)te.th32ThreadID);
                        PWSTR w = NULL;
                        if (get_desc && SUCCEEDED(get_desc(h, &w)) && w) {
                            if (*w) snprintf(name, sizeof name, "%.38ls", w);
                            LocalFree(w);
                        }
                        if (!strncmp(name, "guest main", 10)) main_pct = pct;
                        if (!strcmp(name, "gpu")) gpu_thread_pct = pct;
                        for (int i = 0; i < 4; i++)
                            if (pct > top[i].pct) { memmove(&top[i + 1], &top[i], (3 - i) * sizeof top[0]); snprintf(top[i].name, sizeof top[i].name, "%s", name); top[i].pct = pct; break; }
                    }
                    ncur++;
                }
                CloseHandle(h);
            }
            CloseHandle(snap);
        }
        memcpy(prev, cur, ncur * sizeof *cur);
        nprev = ncur;
        /* the memory */
        double private_mb = 0, faults_s = 0;
        if (mem_info) {
            struct { DWORD cb, PageFaultCount; SIZE_T a, b, c, d, e, f, PagefileUsage, h, PrivateUsage; } pmc;   /* (PROCESS_MEMORY_COUNTERS_EX: 80 bytes) */
            memset(&pmc, 0, sizeof pmc);
            pmc.cb = sizeof pmc;
            if (mem_info(GetCurrentProcess(), &pmc, sizeof pmc)) {
                private_mb = (double)pmc.PrivateUsage / 1048576.0;
                faults_s = last_faults ? (double)(pmc.PageFaultCount - last_faults) / wall : 0;
                last_faults = pmc.PageFaultCount;
            }
        }
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof ms;
        double avail_mb = GlobalMemoryStatusEx(&ms) ? (double)ms.ullAvailPhys / 1048576.0 : 0;
        unsigned long long gpu_used = 0, gpu_budget = 0, tex_bytes = 0, surf_bytes = 0, shaders = 0;
        unsigned textures = 0;
        gfx_monitor_stats(&gpu_used, &gpu_budget, &textures, &tex_bytes, &surf_bytes, &shaders);
        /* the processor */
        double fast_use = -1, slow_use = -1, fast_clock = -1, slow_clock = -1, temp = -1, pages_in = -1;
        if (query && collect(query) == ERROR_SUCCESS) {
            fast_use = mon_average_cores(mon_pdh.util, 1); slow_use = mon_average_cores(mon_pdh.util, 0);
            fast_clock = mon_average_cores(mon_pdh.perf, 1); slow_clock = mon_average_cores(mon_pdh.perf, 0);
            temp = mon_single(mon_pdh.temp, 1);
            if (temp > 200) temp -= 273.15;   /* (kelvin) */
            pages_in = mon_single(mon_pdh.pages, 0);
        }
        char tops[200] = "";
        for (int i = 0; i < 4 && top[i].pct > 0.5; i++) {
            char one[64];
            snprintf(one, sizeof one, "%s%s:%.0f", i ? ";" : "", top[i].name, top[i].pct);
            strncat(tops, one, sizeof tops - strlen(tops) - 1);
        }
        for (char *p = tops; *p; p++) if (*p == ',') *p = ' ';
        fprintf(csv, "%.0f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%.0f,%.0f,%s,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%u,%.0f,%.0f,%llu,%.0f,%.0f,%.0f,%.0f,%.0f\n",
                t, game_fps, shown_fps, skipped_s, interval_avg, interval_max, gpu_pct, draws_s, process_pct, main_pct, gpu_thread_pct, tops, private_mb, avail_mb, faults_s,
                pages_in, gpu_used / 1048576.0, gpu_budget / 1048576.0, textures, tex_bytes / 1048576.0, surf_bytes / 1048576.0, (unsigned long long)shaders,
                fast_use, slow_use, fast_clock, slow_clock, temp);
        fflush(csv);
        char text[2048];
        snprintf(text, sizeof text,
                 "MONITOR  %.0f s\n"
                 "game %.1f frames/s (%.0f%% speed)  shown %.1f/s  skipped %.1f/s\n"
                 "frame interval %.1f ms (worst %.0f ms)  draws %.0f/s  gpu thread %.0f%% busy\n"
                 "cpu: main thread %.0f%%  gpu thread %.0f%%  whole game %.0f%% of one core\n"
                 "     busiest: %s\n"
                 "cores: fast use %.0f%% clock %.0f%%   slow use %.0f%% clock %.0f%%   temp %.0f C\n"
                 "memory: game %.0f MB  PC available %.0f MB  disk reads %.0f pages/s  faults %.0f/s\n"
                 "graphics: %.0f of %.0f MB   textures %u (%.0f MB)  images %.0f MB  shaders %llu\n",
                 t, game_fps, game_fps / 0.6, shown_fps, skipped_s, interval_avg, interval_max, draws_s, gpu_pct, main_pct, gpu_thread_pct, process_pct,
                 tops[0] ? tops : "-", fast_use, fast_clock, slow_use, slow_clock, temp, private_mb, avail_mb, pages_in, faults_s,
                 gpu_used / 1048576.0, gpu_budget / 1048576.0, textures, tex_bytes / 1048576.0, surf_bytes / 1048576.0, (unsigned long long)shaders);
        AcquireSRWLockExclusive(&monitor_text_lock);
        snprintf(monitor_text_buf, sizeof monitor_text_buf, "%s", text);
        ReleaseSRWLockExclusive(&monitor_text_lock);
        FILE *now_f = fopen("build\\monitor_now.txt", "w");
        if (now_f) { fputs(text, now_f); fclose(now_f); }
    }
}

static void monitor_start(void) {
    CloseHandle(CreateThread(NULL, 0, monitor_main, NULL, 0, NULL));
    fprintf(stderr, "[monitor] on: build\\monitor.csv (a row a second), build\\monitor_now.txt (the last second), F2 on screen\n");
}
#else
static inline void monitor_text(char *out, size_t size) { if (size) out[0] = 0; }
static inline void monitor_start(void) {}
#endif
#endif
