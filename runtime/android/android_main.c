/* The entry SDL's Android activity (SDLActivity) calls: SDL_main, in libmain.so. It points the game at the app's own folder (the
 * one the launcher writes display.ini, controllers.ini and online_*.txt into: Android/data/<package>/files), where the player puts
 * the game's data, and runs the same main() as every other platform, with the arguments Play SSBU.bat gives it on Windows.
 *
 * What the game writes to stderr goes to the system log (adb logcat -s ssbu) as well as to <files>/play.log, and while the game is
 * starting, a sampler thread logs where the main thread is every few seconds (module + offset of each frame: for a stuck start-up). */
#include <SDL.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "execinfo.h"

#undef main   /* (SDL.h renames main to SDL_main for apps like this one: the game's own main, host.c's, is the real one) */
int main(int argc, char **argv);

#define TAG "ssbu"

static int log_file = -1;

/* stderr, line by line, into the system log and the file */
static void *pump(void *arg) {
    int in = (int)(intptr_t)arg;
    char line[2048];
    size_t n = 0;
    for (;;) {
        char c;
        ssize_t r = read(in, &c, 1);
        if (r <= 0) break;
        if (c != '\n' && n < sizeof line - 1) { line[n++] = c; continue; }
        line[n] = 0;
        __android_log_write(ANDROID_LOG_INFO, TAG, line);
        if (log_file >= 0) { (void)!write(log_file, line, n); (void)!write(log_file, "\n", 1); }
        n = 0;
    }
    return NULL;
}

static pthread_t main_thread;
static void *volatile frames[40];
static volatile int frame_count = -1;

static void on_sample(int sig, siginfo_t *si, void *uc) {
    (void)sig; (void)si; (void)uc;
    frame_count = backtrace((void **)frames, 40);
}

static void *sampler(void *arg) {
    (void)arg;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_sample;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGUSR2, &sa, NULL);
    for (int round = 0; round < 20; round++) {   /* (a minute: a start-up that is slow shows where, one that is stuck shows it again and again) */
        sleep(3);
        frame_count = -1;
        if (pthread_kill(main_thread, SIGUSR2)) break;
        for (int i = 0; i < 40 && frame_count < 0; i++) usleep(50000);
        char text[1024];
        int len = snprintf(text, sizeof text, "[sample %d]", round);
        for (int i = 0; i < frame_count && len < (int)sizeof text - 64; i++) {
            Dl_info di;
            if (dladdr(frames[i], &di) && di.dli_fname) {
                const char *base = strrchr(di.dli_fname, '/');
                len += snprintf(text + len, sizeof text - (size_t)len, " %s+%#lx", base ? base + 1 : di.dli_fname, (unsigned long)((uintptr_t)frames[i] - (uintptr_t)di.dli_fbase));
            } else len += snprintf(text + len, sizeof text - (size_t)len, " %p", frames[i]);
        }
        __android_log_write(ANDROID_LOG_INFO, TAG, text);
    }
    return NULL;
}

/* A sampling profiler for the threads that compile shaders (SSBU_PROFILE=<thread name part>): every 20 ms one such thread is signalled and its stack
 * noted; every 20 s the places most often on a stack (inclusive) and most often at its top are logged as module+offset, for llvm-symbolizer. */
#include <sys/syscall.h>
#include <dirent.h>

#define PROF_SLOTS 16384
static struct { uintptr_t base; uint32_t off; uint32_t incl, leaf; } prof[PROF_SLOTS];
static void *volatile pframes[48];
static volatile int pcount = -1;

static void on_prof(int sig, siginfo_t *si, void *uc) { (void)sig; (void)si; (void)uc; pcount = backtrace((void **)pframes, 48); }

static void prof_note(void *pc, int leaf) {
    Dl_info di;
    if (!dladdr(pc, &di) || !di.dli_fbase) return;
    uint32_t off = (uint32_t)((uintptr_t)pc - (uintptr_t)di.dli_fbase);
    uint32_t h = (uint32_t)(((uintptr_t)di.dli_fbase >> 12) * 2654435761u ^ (off >> 2) * 40503u) % PROF_SLOTS;
    for (int tries = 0; tries < 64; tries++, h = (h + 1) % PROF_SLOTS) {
        if (!prof[h].base) { prof[h].base = (uintptr_t)di.dli_fbase; prof[h].off = off; }
        if (prof[h].base == (uintptr_t)di.dli_fbase && prof[h].off == off) { if (leaf) prof[h].leaf++; else prof[h].incl++; return; }
    }
}

static int prof_threads, prof_errors, prof_timeouts;
static void prof_report(int samples, const char *what) {
    char text[2000];
    for (int pass = 0; pass < 2; pass++) {
        int len = snprintf(text, sizeof text, "[profile %s] %d samples (threads %d, signal errors %d, no answer %d), %s:", what, samples, prof_threads, prof_errors, prof_timeouts, pass ? "at the top" : "on the stack");
        for (int k = 0; k < 10; k++) {
            int best = -1; uint32_t bv = 0;
            for (int i = 0; i < PROF_SLOTS; i++) { uint32_t v = pass ? prof[i].leaf : prof[i].incl; if (v > bv) { bv = v; best = i; } }
            if (best < 0) break;
            Dl_info di; const char *name = "?";
            if (dladdr((void *)(prof[best].base + prof[best].off), &di) && di.dli_fname) { name = strrchr(di.dli_fname, '/'); name = name ? name + 1 : di.dli_fname; }
            len += snprintf(text + len, sizeof text - (size_t)len, " %s+%#x=%u", name, prof[best].off, bv);
            if (pass) prof[best].leaf = 0; else prof[best].incl = 0;
        }
        __android_log_write(ANDROID_LOG_INFO, TAG, text);
    }
    memset(prof, 0, sizeof prof);
}

static void *profiler(void *arg) {
    const char *want = arg;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_prof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGRTMIN + 4, &sa, NULL);   /* (not SIGUSR1: the runtime of Android takes it) */
    int samples = 0;
    time_t last = time(NULL);
    for (;;) {
        DIR *d = opendir("/proc/self/task");
        int tids[64], n = 0;
        struct dirent *e;
        while (d && (e = readdir(d)) && n < 64) {
            int tid = atoi(e->d_name);
            if (tid <= 0) continue;
            char path[64], comm[32] = "";
            snprintf(path, sizeof path, "/proc/self/task/%d/comm", tid);
            FILE *f = fopen(path, "r");
            if (f) { if (fgets(comm, sizeof comm, f)) comm[strcspn(comm, "\n")] = 0; fclose(f); }
            if (strstr(comm, want)) tids[n++] = tid;
        }
        if (d) closedir(d);
        prof_threads = n;
        for (int i = 0; i < n; i++) {
            pcount = -1;
            if (syscall(SYS_tgkill, getpid(), tids[i], SIGRTMIN + 4)) { prof_errors++; continue; }
            for (int w = 0; w < 2000 && pcount < 0; w++) usleep(500);
            if (pcount <= 0) { prof_timeouts++; continue; }
            samples++;
            prof_note(pframes[0], 1);
            for (int j = 0; j < pcount; j++) prof_note(pframes[j], 0);
            usleep(20000);
        }
        usleep(20000);
        if (time(NULL) - last >= 20) { prof_report(samples, want); samples = 0; last = time(NULL); }
    }
    return NULL;
}

__attribute__((visibility("default"))) int SDL_main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *files = SDL_AndroidGetExternalStoragePath();
    if (!files) files = SDL_AndroidGetInternalStoragePath();
    static char modules[4096], root[4096];
    snprintf(root, sizeof root, "%s", files ? files : ".");
    snprintf(modules, sizeof modules, "%s/dump/modules", root);
    setenv("SSBU_DATA_ROOT", root, 1);
    setenv("SSBU_PROFILE", "shader compiler", 1);   /* (to find where compiling shaders spends its time) */
    setenv("VKD3D_SHADER_DEBUG", "err", 1);   /* (vkd3d-shader says "fixme" for every constant it folds: thousands of lines a second into the system log) */
    if (chdir(root)) perror("chdir");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");   /* (the game is wide: SDL would otherwise follow the window's shape, and the phone's) */
    __android_log_print(ANDROID_LOG_INFO, TAG, "SDL_main: data in %s", root);
    static char log[4096];
    snprintf(log, sizeof log, "%s/play.log", root);
    log_file = open(log, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_file >= 0) (void)!ftruncate(log_file, 0);
    int p[2];
    if (!pipe(p)) {   /* (dup2 onto 2, not freopen: closing fd 2 trips Android's fdsan, which the system's own fences share) */
        dup2(p[1], 2);
        close(p[1]);
        pthread_t t;
        pthread_create(&t, NULL, pump, (void *)(intptr_t)p[0]);
        pthread_detach(t);
    }
    setvbuf(stderr, NULL, _IOLBF, 0);
    main_thread = pthread_self();
    pthread_t s;
    pthread_create(&s, NULL, sampler, NULL);
    pthread_detach(s);
    {   /* can the game read what the player put in its folder? (a file pushed with adb belongs to the shell, in a group the app must share) */
        static const char *what[] = { "dump/modules/main.reloc.bin", "dump/modules", "dump", "romfs.bin", "dump/fonts", NULL };
        for (int i = 0; what[i]; i++) {
            char path[4200];
            snprintf(path, sizeof path, "%s/%s", root, what[i]);
            errno = 0;
            int ok = access(path, R_OK) == 0;
            int e = errno;
            FILE *f = fopen(path, "rb");
            __android_log_print(ANDROID_LOG_INFO, TAG, "check %s: access %s (%s), fopen %s (%s), uid %d", what[i], ok ? "ok" : "NO", strerror(e), f ? "ok" : "NO", f ? "-" : strerror(errno), (int)getuid());
            if (f) fclose(f);
        }
    }
    {   /* SSBU_PROFILE=<part of a thread's name>: the sampling profiler above, on those threads */
        const char *want = getenv("SSBU_PROFILE");
        if (want && *want) { static char name[64]; snprintf(name, sizeof name, "%s", want); pthread_t pt; pthread_create(&pt, NULL, profiler, name); pthread_detach(pt); }
    }
    __android_log_write(ANDROID_LOG_INFO, TAG, "SDL_main: starting the game");
    char *args[] = { "ssbu", modules, "--status", "30", "--online", NULL };
    return main(5, args);
}
