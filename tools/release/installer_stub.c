/* "Install NativeSmash.exe": the one file a player sees next to the NativeSmash folder in the download.
 * It hides that folder (everything the project is made of), then starts the installer inside it.
 * Built by tools\make_release.py; the icon is docs\assets\nativesmash.ico. */
#include <windows.h>
#include <wchar.h>

#define DATA_DIR L"NativeSmash"
#define BAT_NAME L"Install NativeSmash.bat"
#define BACKSLASH L'\\'

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    wchar_t exe[MAX_PATH * 2], dir[MAX_PATH * 2], data[MAX_PATH * 2], bat[MAX_PATH * 2], line[MAX_PATH * 4 + 64];
    (void)inst; (void)prev; (void)cmd; (void)show;
    DWORD n = GetModuleFileNameW(NULL, exe, (DWORD)(sizeof exe / sizeof exe[0]));
    if (n == 0 || n >= sizeof exe / sizeof exe[0]) return 1;
    wcscpy(dir, exe);
    wchar_t *slash = wcsrchr(dir, BACKSLASH);
    if (!slash) return 1;
    *slash = 0;
    swprintf(data, sizeof data / sizeof data[0], L"%ls\\%ls", dir, DATA_DIR);
    swprintf(bat, sizeof bat / sizeof bat[0], L"%ls\\%ls", data, BAT_NAME);
    if (GetFileAttributesW(bat) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL,
            L"No encuentro la carpeta \"NativeSmash\" junto a este programa.\n"
            L"Extrae todo el zip antes de abrirlo (clic derecho > Extraer todo).\n\n"
            L"I can't find the \"NativeSmash\" folder next to this program.\n"
            L"Extract the whole zip first (right-click > Extract all).",
            L"NativeSmash", MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    DWORD attr = GetFileAttributesW(data);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_HIDDEN))
        SetFileAttributesW(data, attr | FILE_ATTRIBUTE_HIDDEN);
    /* cmd /c ""<bat>"": the batch file takes the internet mark off the files and runs the installer */
    swprintf(line, sizeof line / sizeof line[0], L"cmd.exe /c \"\"%ls\"\"", bat);
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, line, NULL, NULL, FALSE, 0, NULL, data, &si, &pi)) {
        MessageBoxW(NULL, L"No se pudo abrir el instalador.\nCould not start the installer.", L"NativeSmash", MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}
