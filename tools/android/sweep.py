#!/usr/bin/env python3
"""Compiles (syntax only) the engine's runtime sources for arm64 Android, the way tools/linux/build_linux.py lists them, and says
which fail and why. A quick way to see what the Android port still needs (docs/ANDROID.md).

Usage: sweep.py [file.c ...]   (default: every runtime source)"""
import glob
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
A = os.path.join(REPO, "deps", "android")
RUNTIME = os.path.join(REPO, "runtime")
NDK_BIN = glob.glob(os.path.join(A, "ndk", "android-ndk-*", "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin"))[0]
CLANG = os.path.join(NDK_BIN, "clang.exe")
GEN = os.path.join(A, "build", "game", "gen")
DXVK = os.path.join(A, "src", "dxvk", "include", "native")
VKD3D = glob.glob(os.path.join(A, "src", "vkd3d-2.1", "vkd3d-2.1"))[0]
SDL = os.path.join(A, "install", "arm64", "include", "SDL2")

INCLUDES = ["-I", os.path.join(RUNTIME, "android", "include"), "-I", os.path.join(RUNTIME, "linux", "include"), "-I", os.path.join(DXVK, "windows"), "-I", os.path.join(DXVK, "directx"),
            "-I", SDL, "-I", os.path.join(VKD3D, "include"), "-I", GEN, "-I", RUNTIME, "-I", os.path.join(REPO, "third_party", "libvpx"), "-I", os.path.join(REPO, "third_party", "opus", "include")]
DEFINES = ["-D_GNU_SOURCE", "-D_REENTRANT", "-DSSBU_HAVE_OPUS", "-DSSBU_HAVE_VPX", "-DSSBU_HAVE_SDL2", "-DSSBU_ANDROID"]
FLAGS = ["--target=aarch64-linux-android29", "-march=armv8-a", "-fPIC", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-stack-protector",
         "-fvisibility=hidden", "-funwind-tables", "-w", "-fsyntax-only", "-ferror-limit=0"]


def sources():
    found = [os.path.join(RUNTIME, n) for n in sorted(os.listdir(RUNTIME)) if re.fullmatch(r"(host|kernel|gpu\w*|hle\w*|ipc\w*|net\w*|debug_watchdog)\.c", n)]
    found += [os.path.join(RUNTIME, "linux", n) for n in sorted(os.listdir(os.path.join(RUNTIME, "linux"))) if n.endswith((".c", ".cpp"))]
    found += [os.path.join(RUNTIME, "android", n) for n in sorted(os.listdir(os.path.join(RUNTIME, "android"))) if n.endswith(".c")]
    return found


def main():
    os.makedirs(GEN, exist_ok=True)
    font = os.path.join(GEN, "overlay_font.h")
    if not os.path.exists(font):
        with open(font, "w") as f:
            f.write("static const unsigned char overlay_font[%d] = {0};\n" % (24 * 16 * 48 * 6))
    files = [os.path.abspath(a) for a in sys.argv[1:]] or sources()

    def one(src):
        lang = ["-x", "c++", "-std=gnu++17"] if src.endswith(".cpp") else ["-std=gnu11"]
        r = subprocess.run([CLANG, *FLAGS, *lang, *INCLUDES, *DEFINES, src], capture_output=True, text=True)
        errors = [l for l in (r.stdout + r.stderr).splitlines() if "error:" in l]
        return src, errors

    with ThreadPoolExecutor(8) as pool:
        results = list(pool.map(one, files))
    total = 0
    for src, errors in results:
        name = os.path.relpath(src, REPO)
        if errors:
            total += len(errors)
            print(f"{name}: {len(errors)} errors")
            for e in errors[:4]:
                print("    " + re.sub(r"^.*?(runtime[\\/])", r"\1", e)[:200])
        else:
            print(f"{name}: ok")
    print(f"\n{sum(1 for _s, e in results if not e)} of {len(results)} files compile; {total} errors")


if __name__ == "__main__":
    main()
