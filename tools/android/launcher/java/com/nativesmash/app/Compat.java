package com.nativesmash.app;

/** What this phone's Vulkan driver offers, against what NativeSmash's graphics need (vkcheck.c). */
final class Compat {
    static { System.loadLibrary("vkcheck"); }
    static native String report();
}
