# Android (experimental, not playable)

NativeSmash does not run on Android yet. This page says what exists, what was learned, and what is missing.

## What exists

- **The Android build tools**, installed by hand in `deps/android` (not in the repository): Google's NDK r27c, Temurin JDK 17, and the SDK's build-tools 35 and platform 35 (their license terms were accepted on this PC). `tools/android/build_vulkancheck.py` uses them.
- **`NativeSmash-VulkanCheck.apk`**: a small app (`tools/android/vulkancheck`) that shows what a phone's Vulkan driver offers against what DXVK, the layer NativeSmash's graphics run on in the Linux build, asks for. Build it with `python tools/android/build_vulkancheck.py`; it signs with a debug key made in `deps/android/debug.keystore`. Install it by copying it to the phone (allow installing from unknown sources), open it, and read the verdict at the bottom. This is the first thing to run: if the GPU lacks what DXVK needs, the graphics need another route.

- **`NativeSmash-Launcher.apk`** (`tools/android/launcher`, built by `tools/android/build_launcher.py`): the phone launcher. It has the screen resolution (Auto, 540p to 1080p, Native) with FSR, the controls (on-screen touch, or a pad whose buttons are remapped by pressing them), the online settings (name, public lobby, lobby code, offline, direct connections), a compatibility check (the Vulkan one) and a Play button that opens into the game with a circular reveal. What is chosen is saved as it is chosen (and when the app is closed) and written, in the same formats as on the PC (`display.ini`, `controllers.ini`, `online_*.txt`), to the app's folder for the game to read. Play leads to a screen that says the game's engine is not in the app yet. Tested in an Android 15 emulator on this PC (English and Spanish, settings kept after closing the app); not yet on a real phone.

## What the research found

- NativeSmash's Linux renderer is Direct3D 11 translated to Vulkan by **DXVK** (x86-64 builds only). DXVK has no first-party Android support, and (its Driver support page) needs Vulkan 1.3 and `VK_EXT_robustness2` with `nullDescriptor` and `robustBufferAccess2`; Direct3D 11's stream output needs `VK_EXT_transform_feedback`. Whether a Mali GPU such as the Pixel 9 Pro XL's Mali-G715 offers those could not be found online: the check app answers it.
- The runtime's Linux layer uses `ucontext`-style fibers and SDL2; Android's C library has no `ucontext`, so the arm64 fibers of the macOS port (`runtime/macos/fiber_arm64.S`) would be needed, and SDL needs its Android activity.
- The recompiled game's C already builds for arm64 (the macOS build does it), so that part is not the problem. The graphics are.

## What is missing for a playable build

1. A graphics route on Android: DXVK built for arm64 Android if the driver allows it, or a Vulkan renderer of NativeSmash's own (like the Metal one on macOS).
2. The runtime's platform layer for Android (files, threads, fibers, window, sound, controllers).
3. The game's data (about 17.6 GB) read from the phone's storage.
4. An installer that builds the APK from each person's own copy (as the Windows one builds the game), and a page for it.
