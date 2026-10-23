package com.nativesmash.app;

import org.libsdl.app.SDLActivity;

/**
 * The game itself: SDL's activity loads libSDL2.so and libmain.so (the recompiled game and its runtime, tools/android/build_android.py)
 * and runs SDL_main (runtime/android/android_main.c) on a thread of its own. Only in the APK built with --engine; without it the
 * launcher's Play button goes to GameActivity, which says the engine is not in the app.
 */
public class EngineActivity extends SDLActivity {
}
