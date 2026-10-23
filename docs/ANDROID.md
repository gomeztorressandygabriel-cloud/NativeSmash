# Android (experimental, not playable)

NativeSmash does not run on Android yet. This page says what exists, what was learned, and what is missing.

## What exists

- **The Android build tools**, installed by hand in `deps/android` (not in the repository): Google's NDK r27c, Temurin JDK 17, and the SDK's build-tools 35 and platform 35 (their license terms were accepted on this PC). `tools/android/build_vulkancheck.py` uses them.
- **`NativeSmash-VulkanCheck.apk`**: a small app (`tools/android/vulkancheck`) that shows what a phone's Vulkan driver offers against what DXVK, the layer NativeSmash's graphics run on in the Linux build, asks for. Build it with `python tools/android/build_vulkancheck.py`; it signs with a debug key made in `deps/android/debug.keystore`. Install it by copying it to the phone (allow installing from unknown sources), open it, and read the verdict at the bottom. This is the first thing to run: if the GPU lacks what DXVK needs, the graphics need another route.

- **`NativeSmash-Launcher.apk`** (`tools/android/launcher`, built by `tools/android/build_launcher.py`): the phone launcher. It has the screen resolution (Auto, 540p to 1080p, Native) with FSR, the controls (on-screen touch, or a pad whose buttons are remapped by pressing them), the online settings (name, public lobby, lobby code, offline, direct connections), a compatibility check (the Vulkan one) and a Play button that opens into the game with a circular reveal. What is chosen is saved as it is chosen (and when the app is closed) and written, in the same formats as on the PC (`display.ini`, `controllers.ini`, `online_*.txt`), to the app's folder for the game to read. Play leads to a screen that says the game's engine is not in the app yet. Tested in an Android 15 emulator on this PC (English and Spanish, settings kept after closing the app); not yet on a real phone.

## The game runs on a Pixel 9 Pro XL (2026-10-23)

`NativeSmash-Android.apk` (`python tools/android/build_launcher.py --engine`, after `tools/android/build_android.py`) installs on a Pixel 9 Pro XL (Android 17, Mali-G715) and plays: the title, the
tutorial fight, the sound and the movies. Found and fixed on the way:

- SDL.h renames `main` to `SDL_main`: the entry called itself (`runtime/android/android_main.c`).
- DXVK's libraries must have plain sonames (the APK holds `lib*.so`), and it must look for SDL as `libSDL2.so`.
- The Mali-G715 lacks features DXVK asks for: `fillModeNonSolid`, `multiViewport`, `shaderClipDistance`, `shaderCullDistance`, `robustBufferAccess2` and block-compressed textures
  (`textureCompressionBC`). `tools/android/patches/dxvk-android.patch` (apply it to DXVK v3.1.1's tree) makes them optional; the textures are decoded by the processor when loaded
  (`runtime/gpu_d3d11.c`, with `third_party/bcdec`).
- Files made by `adb push` belong to the shell and the app cannot enter their folders: the launcher makes `dump/` and the others itself, the data is pushed into them.
- The data on the phone: `python tools/android/make_data.py`, then `adb push` of the three folders into `Android/data/com.nativesmash.app/files/dump/` and of `romfs.bin` beside it.

Not done yet: touch controls (the game has none on screen), the pad names the launcher writes (`controllers.ini` wants SDL's names), speed and memory tuning, shader compilation stutter, and the installer for others.

First match on the Pixel (Link against Mario, 2026-10-23, 540p): menus and the title run at 57 to 59 frames a second; the opening movie at about 50. In the match the first-time shader
compilation (two compiler threads at 100 %) stalls the game for seconds at a time and draws that wait for their shaders are skipped: the stage and a fighter did not show, and the match ran at
about 11 frames a second for the first minute. It also stopped after some 16 minutes with a SIGILL inside libunwind on DXVK's command-stream thread (`reports/crash_*.txt`). The `keyboard_player` of
the launcher's controllers.ini is 1 so that a keyboard (or `adb shell input keyevent`) plays until the touch controls exist.
Next: a shader cache that survives (DXVK's state cache and the game's own prewarm), the crash, then the touch controls.

## What the research found

- NativeSmash's Linux renderer is Direct3D 11 translated to Vulkan by **DXVK** (x86-64 builds only). DXVK has no first-party Android support, and (its Driver support page) needs Vulkan 1.3 and `VK_EXT_robustness2` with `nullDescriptor` and `robustBufferAccess2`; Direct3D 11's stream output needs `VK_EXT_transform_feedback`. Whether a Mali GPU such as the Pixel 9 Pro XL's Mali-G715 offers those could not be found online: the check app answers it.
- The runtime's Linux layer uses `ucontext`-style fibers and SDL2; Android's C library has no `ucontext`, so the arm64 fibers of the macOS port (`runtime/macos/fiber_arm64.S`) would be needed, and SDL needs its Android activity.
- The recompiled game's C already builds for arm64 (the macOS build does it), so that part is not the problem. The graphics are.

## The engine's pieces built for arm64 Android so far (2026-10-23)

With the tools above, these were built for `aarch64-linux-android29` (all into `deps/android/build`, not in the repository):

| Piece | How | Result |
| :--- | :--- | :--- |
| SDL2 2.32.8 | CMake with the NDK toolchain file, `-DANDROID_ABI=arm64-v8a` | `install/arm64/lib/libSDL2.so` |
| DXVK 3.1.1 (Direct3D 11 and DXGI on Vulkan) | `meson setup` with a cross file (clang from the NDK, `system = 'linux'`, `pkg_config_libdir` at the SDL2 install), `-Dnative_sdl2=enabled`, D3D8/9/10 off; glslang from its Windows release in the PATH; then `ninja` | `libdxvk_d3d11.so`, `libdxvk_dxgi.so` (256 of 256 steps, no errors) |
| vkd3d-shader 2.1 | `python tools/android/build_vkd3d_android.py` (parsers from winflexbison) | `build/vkd3d/libvkd3d-shader.a` |
| libopus and libvpx | `tools/codec_libs.py`'s `build_static` (the macOS arm64 configuration, plain C) with the NDK clang | `build/game/codecs/libopus.a`, `libvpx.a` |

So the graphics layer (DXVK) and the shader compiler compile for Android arm64. Whether DXVK then runs on a phone's GPU is not known: it needs the driver's `VK_EXT_robustness2` and the other features the check app lists.

## What stops a playable build, found by reading the code

The engine itself (`runtime/`) has two platform layers: Linux (x86-64) and macOS (arm64). Android is Linux on arm64, so it needs parts of both, and a third piece of its own:

- `runtime/host.c` and others use `__APPLE__` to mean "arm64" in about 30 places (reading the faulting register state in a signal handler, for instance): they must also hold for Android.
- `runtime/linux/win32.c` (the Windows API shim) has the x86-64 fiber switch and the memory layout of x86-64 Linux; Android needs the arm64 fibers (`runtime/macos/fiber_arm64.S` as a model) and its own memory layout.
- The compile flags of the Linux build are x86-64 (AVX2) and non-PIE; an Android app needs a shared library (`-fPIC`).
- The generated game C (about 100 modules) must be compiled again for arm64 (hours on a laptop), and the result linked into `libmain.so` with SDL's activity (`SDLActivity`) in the APK, the launcher (`tools/android/launcher`) opening it.
- None of this can be tested without the phone: the emulator here is x86-64.

It is days of work with the phone at hand, not a night.

## What is missing for a playable build

1. A graphics route on Android: DXVK built for arm64 Android if the driver allows it, or a Vulkan renderer of NativeSmash's own (like the Metal one on macOS).
2. The runtime's platform layer for Android (files, threads, fibers, window, sound, controllers).
3. The game's data (about 17.6 GB) read from the phone's storage.
4. An installer that builds the APK from each person's own copy (as the Windows one builds the game), and a page for it.
