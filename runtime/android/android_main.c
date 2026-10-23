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

__attribute__((visibility("default"))) int SDL_main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *files = SDL_AndroidGetExternalStoragePath();
    if (!files) files = SDL_AndroidGetInternalStoragePath();
    static char modules[4096], root[4096];
    snprintf(root, sizeof root, "%s", files ? files : ".");
    snprintf(modules, sizeof modules, "%s/dump/modules", root);
    setenv("SSBU_DATA_ROOT", root, 1);
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
    __android_log_write(ANDROID_LOG_INFO, TAG, "SDL_main: starting the game");
    char *args[] = { "ssbu", modules, "--status", "30", "--online", NULL };
    return main(5, args);
}
