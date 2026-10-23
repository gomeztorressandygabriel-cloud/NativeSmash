#!/usr/bin/env python3
"""Builds vkd3d-shader (HLSL -> DXBC, what NativeSmash's Direct3D 11 renderer compiles its shaders with) as a static library for
arm64 Android: deps/android/build/vkd3d/libvkd3d-shader.a. The same sources, config and parsers as tools/linux/build_vkd3d.py,
compiled with the NDK. Needs the sources in deps/android/src (vkd3d-2.1, SPIRV-Headers, winflexbison: docs/ANDROID.md).

Usage: build_vkd3d_android.py"""
import glob
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
A = os.path.join(REPO, "deps", "android")
SRC = glob.glob(os.path.join(A, "src", "vkd3d-2.1", "vkd3d-2.1"))[0]
SPIRV = glob.glob(os.path.join(A, "src", "spirv-headers", "SPIRV-Headers-*", "include"))[0]
FLEXBISON = os.path.join(A, "src", "winflexbison")
OUT = os.path.join(A, "build", "vkd3d")
GEN = os.path.join(OUT, "gen")
NDK_BIN = glob.glob(os.path.join(A, "ndk", "android-ndk-*", "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin"))[0]
CLANG = os.path.join(NDK_BIN, "clang.exe")
AR = os.path.join(NDK_BIN, "llvm-ar.exe")

CONFIG = """#define HAVE_ATOMIC_EXCHANGE_N 1
#define HAVE_BUILTIN_ADD_OVERFLOW 1
#define HAVE_BUILTIN_CLZ 1
#define HAVE_BUILTIN_CTZ 1
#define HAVE_BUILTIN_POPCOUNT 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYNC_ADD_AND_FETCH 1
#define HAVE_SYNC_BOOL_COMPARE_AND_SWAP 1
#define HAVE_SPIRV_UNIFIED1_SPIRV_H 1
#define HAVE_SPIRV_UNIFIED1_GLSL_STD_450_H 1
#define PACKAGE_NAME "vkd3d"
#define PACKAGE_VERSION "2.1"
#define PACKAGE_STRING "vkd3d 2.1"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_TARNAME "vkd3d"
#define PACKAGE_URL ""
#define VERSION "2.1"
#define HAVE_DLFCN_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_STRTOF_L 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
"""


def generate():
    os.makedirs(GEN, exist_ok=True)
    src = os.path.join(SRC, "libs", "vkd3d-shader")
    for name in ("hlsl", "preproc"):
        if not os.path.exists(os.path.join(GEN, name + ".tab.c")):
            subprocess.run([os.path.join(FLEXBISON, "win_bison.exe"), "-d", "-o", name + ".tab.c", os.path.join(src, name + ".y")], cwd=GEN, check=True)
        if not os.path.exists(os.path.join(GEN, name + ".yy.c")):
            subprocess.run([os.path.join(FLEXBISON, "win_flex.exe"), "-o", name + ".yy.c", os.path.join(src, name + ".l")], cwd=GEN, check=True)
    with open(os.path.join(GEN, "vkd3d_version.h"), "w") as f:
        f.write('#define VKD3D_VCS_ID ""\n')


def patch_sources():
    """tools/android/patches/vkd3d-conditionals-memo.patch: evaluate_conditionals_recurse walked shared operands again and again (it was most of the
    time compiling a shader on a phone). Applied once, to the sources in deps."""
    target = os.path.join(SRC, "libs", "vkd3d-shader", "hlsl_codegen.c")
    if "eval_cond_memo" in open(target, encoding="utf-8").read():
        return
    patch = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patches", "vkd3d-conditionals-memo.patch")
    subprocess.run(["git", "apply", "--unsafe-paths", "-p1", "--directory", os.path.relpath(SRC, REPO).replace(os.sep, "/"), patch], cwd=REPO, check=True)


def main():
    patch_sources()
    generate()
    inc = os.path.join(OUT, "include")
    os.makedirs(inc, exist_ok=True)
    with open(os.path.join(inc, "config.h"), "w") as f:
        f.write(CONFIG)
    shader = sorted(glob.glob(os.path.join(SRC, "libs", "vkd3d-shader", "*.c")))
    common = [os.path.join(SRC, "libs", "vkd3d-common", n) for n in ("debug.c", "error.c", "memory.c", "utf8.c")]
    gen = [os.path.join(GEN, n) for n in ("hlsl.tab.c", "hlsl.yy.c", "preproc.tab.c", "preproc.yy.c")]
    flags = ["--target=aarch64-linux-android29", "-O2", "-fPIC", "-w", "-std=gnu11", "-I", inc, "-I", GEN, "-I", os.path.join(SRC, "include"),
             "-I", os.path.join(SRC, "include", "private"), "-I", os.path.join(SRC, "libs", "vkd3d-shader"), "-I", SPIRV,
             "-DLIBVKD3D_SHADER_SOURCE", "-DVKD3D_NO_VULKAN_H", "-DVKD3D_DEBUG_ENV=VKD3D_SHADER_DEBUG", "-DNDEBUG"]
    srcs = shader + common + gen
    objs = [os.path.join(OUT, "obj", os.path.basename(s)[:-2] + ".o") for s in srcs]
    os.makedirs(os.path.join(OUT, "obj"), exist_ok=True)

    def one(job):
        s, o = job
        r = subprocess.run([CLANG, *flags, "-c", s, "-o", o], capture_output=True, text=True)
        return (s, r.stdout + r.stderr) if r.returncode else None

    with ThreadPoolExecutor(8) as pool:
        failures = [f for f in pool.map(one, zip(srcs, objs)) if f]
    for s, err in failures[:6]:
        print(f"FAILED {os.path.basename(s)}:\n{err[:2500]}")
    if failures:
        sys.exit(1)
    lib = os.path.join(OUT, "libvkd3d-shader.a")
    if os.path.exists(lib):
        os.remove(lib)
    subprocess.run([AR, "rcs", lib, *objs], check=True)
    print(f"{lib}: {os.path.getsize(lib) / 1048576:.1f} MB")


if __name__ == "__main__":
    main()
