/* The entry SDL's Android activity (SDLActivity) calls: SDL_main, in libmain.so. It points the game at the app's own folder (the
 * one the launcher writes display.ini, controllers.ini and online_*.txt into: Android/data/<package>/files), where the player puts
 * the game's data, and runs the same main() as every other platform, with the arguments Play SSBU.bat gives it on Windows. */
#include <SDL.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv);

__attribute__((visibility("default"))) int SDL_main(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *files = SDL_AndroidGetExternalStoragePath();
    if (!files) files = SDL_AndroidGetInternalStoragePath();
    static char modules[4096], root[4096];
    snprintf(root, sizeof root, "%s", files ? files : ".");
    snprintf(modules, sizeof modules, "%s/dump/modules", root);
    setenv("SSBU_DATA_ROOT", root, 1);
    if (chdir(root)) perror("chdir");
    /* the game's log, where the player can read it (and send it): <files>/play.log */
    static char log[4096];
    snprintf(log, sizeof log, "%s/play.log", root);
    int fd = open(log, O_WRONLY | O_CREAT | O_TRUNC, 0644);   /* (dup2 onto 2, not freopen: closing fd 2 trips Android's fdsan, which the system's own fences share) */
    if (fd >= 0) { dup2(fd, 2); close(fd); } else perror("play.log");
    setvbuf(stderr, NULL, _IOLBF, 0);
    char *args[] = { "ssbu", modules, "--status", "30", "--online", NULL };
    return main(5, args);
}
