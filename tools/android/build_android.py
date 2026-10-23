#!/usr/bin/env python3
"""Builds the game for arm64 Android as a shared library, libmain.so, the way tools/linux/build_linux.py builds it for Linux: the
recompiled game's C (the Windows build's: <port>/build/{main,subsdk0,sdk,nro/*}) and the runtime (runtime/*.c, runtime/linux, and
runtime/android's arm64 fibers and backtrace), compiled with the NDK's clang, linked with DXVK, SDL2, vkd3d-shader, libopus and libvpx
built for Android (docs/ANDROID.md). Incremental: a file is compiled again when its source is newer than its object.

Usage: build_android.py [--port <dir>] [--jobs N] [--runtime-only] [--link-only]"""
import glob
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
A = os.path.join(REPO, "deps", "android")
RUNTIME = os.path.join(REPO, "runtime")
NDK_BIN = glob.glob(os.path.join(A, "ndk", "android-ndk-*", "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin"))[0]
CLANG, CLANGXX, AR, NM = (os.path.join(NDK_BIN, n) for n in ("clang.exe", "clang++.exe", "llvm-ar.exe", "llvm-nm.exe"))
OUT = os.path.join(A, "build", "game")
GEN = os.path.join(OUT, "gen")
DXVK_INC = os.path.join(A, "src", "dxvk", "include", "native")
VKD3D = glob.glob(os.path.join(A, "src", "vkd3d-2.1", "vkd3d-2.1"))[0]
SDL_INC = os.path.join(A, "install", "arm64", "include", "SDL2")


def arg(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


PORT = os.path.abspath(arg("--port") or REPO)
JOBS = int(arg("--jobs", "10"))
TARGET = ["--target=aarch64-linux-android29"]
# Guest FP semantics need IEEE-exact arithmetic (no contraction, no fast-math), as on every platform. Shared-library code: -fPIC.
# Unwind tables stay: C++ exceptions pass through the generated C frames (runtime/recomp.h).
CFLAGS = TARGET + ["-march=armv8-a", "-fPIC", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-stack-protector", "-fvisibility=hidden",
                   "-funwind-tables", "-w", "-I", RUNTIME, "-DSSBU_ANDROID"]
RUNTIME_INCLUDES = ["-I", os.path.join(RUNTIME, "android", "include"), "-I", os.path.join(RUNTIME, "linux", "include"),
                    "-I", os.path.join(DXVK_INC, "windows"), "-I", os.path.join(DXVK_INC, "directx"), "-I", SDL_INC,
                    "-I", os.path.join(VKD3D, "include"), "-I", GEN, "-I", os.path.join(REPO, "third_party", "libvpx"),
                    "-I", os.path.join(REPO, "third_party", "opus", "include")]
RUNTIME_DEFINES = ["-D_GNU_SOURCE", "-D_REENTRANT", "-DSSBU_HAVE_OPUS", "-DSSBU_HAVE_VPX", "-DSSBU_HAVE_SDL2"]
LOW_PRIORITY = getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0)


def needs(obj, *srcs):
    return not os.path.exists(obj) or os.path.getmtime(obj) < max(os.path.getmtime(s) for s in srcs)


def has_landing_sites(path):
    with open(path, "rb") as f:
        return b"__try" in f.read()


def write_if_changed(path, text):
    if os.path.exists(path) and open(path, encoding="utf-8").read() == text:
        return False
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return True


def modules():
    build = os.path.join(PORT, "build")
    found = [(n, os.path.join(build, n), os.path.join(OUT, n)) for n in ("main", "subsdk0", "sdk")]
    nro = os.path.join(build, "nro")
    if os.path.isdir(nro):
        found += [(n, os.path.join(nro, n), os.path.join(OUT, "nro", n)) for n in sorted(os.listdir(nro))
                  if os.path.exists(os.path.join(nro, n, "manifest.tsv"))]
    return [m for m in found if os.path.exists(os.path.join(m[1], "manifest.tsv"))]


def game_jobs():
    """(source, object, optimisation, extra flags, size) of every file of every module."""
    recomp_h = os.path.join(RUNTIME, "recomp.h")
    jobs = []
    for name, src_dir, out_dir in modules():
        with open(os.path.join(src_dir, "manifest.tsv")) as f:
            rows = [line.split("\t") for line in f.read().splitlines() if line]
        for fname, insns, biggest in rows + [["g_module.c", "0", "0"]]:
            src = os.path.join(src_dir, fname)
            obj = os.path.join(out_dir, fname[:-2] + ".o")
            opt = "-O1" if int(biggest) > 20000 or fname == "g_module.c" else "-O2"   # (superlinear optimiser time on very large functions)
            if not needs(obj, src, recomp_h):
                continue
            if fname != "g_module.c" and has_landing_sites(src):
                if int(biggest) > 50000:
                    opt = "-O0"
                wrapper = os.path.join(out_dir, fname[:-2] + "_cxx.cpp")
                os.makedirs(out_dir, exist_ok=True)
                text = '#include "recomp.h"\nextern "C" {\n#include "%s"\n}\n' % src.replace("\\", "/")
                if not os.path.exists(wrapper) or open(wrapper).read() != text:
                    with open(wrapper, "w") as f:
                        f.write(text)
                jobs.append((wrapper, obj, opt, ["-x", "c++", "-std=gnu++17", "-fexceptions"], os.path.getsize(src)))
            else:
                jobs.append((src, obj, opt, ["-std=gnu11"], os.path.getsize(src)))
    return jobs


def hle_names():
    names = set()
    for n in os.listdir(RUNTIME):
        if n.endswith(".c"):
            with open(os.path.join(RUNTIME, n), encoding="utf-8") as f:
                names |= set(re.findall(r"^HLE\((\w+)\)", f.read(), re.M))
    return sorted(names)


def hle_wraps():
    found = set()
    for n in os.listdir(RUNTIME):
        if n.endswith(".c"):
            with open(os.path.join(RUNTIME, n), encoding="utf-8") as f:
                found |= {int(a, 16) for a in re.findall(r"^HLE_WRAP\(([0-9a-fA-F]+)\)", f.read(), re.M)}
    return sorted(found)


def runtime_jobs():
    os.makedirs(GEN, exist_ok=True)
    font = os.path.join(GEN, "overlay_font.h")
    if not os.path.exists(font):   # (the debug overlay's glyphs: made with GDI on Windows; empty here, the overlay shows no text)
        with open(font, "w") as f:
            f.write("static const unsigned char overlay_font[%d] = {0};\n" % (24 * 16 * 48 * 6))
    rt_out = os.path.join(OUT, "rt")
    headers = [os.path.join(RUNTIME, n) for n in os.listdir(RUNTIME) if n.endswith(".h")] + [os.path.join(RUNTIME, "linux", "include", "windows.h")]
    sources = [os.path.join(RUNTIME, n) for n in sorted(os.listdir(RUNTIME))
               if re.fullmatch(r"(host|kernel|gpu\w*|hle\w*|ipc\w*|net\w*|debug_watchdog)\.c", n)]
    sources += [os.path.join(RUNTIME, "linux", n) for n in sorted(os.listdir(os.path.join(RUNTIME, "linux"))) if n.endswith((".c", ".cpp"))]
    sources += [os.path.join(RUNTIME, "android", n) for n in sorted(os.listdir(os.path.join(RUNTIME, "android"))) if n.endswith((".c", ".S"))]
    flags_c = ["-std=gnu11", *RUNTIME_INCLUDES, *RUNTIME_DEFINES]
    jobs = []
    for src in sources:
        obj = os.path.join(rt_out, os.path.basename(src).rsplit(".", 1)[0] + ".o")
        if not needs(obj, src, *headers):
            continue
        if src.endswith(".cpp"):
            jobs.append((src, obj, "-O2", ["-x", "c++", "-std=gnu++17", "-fexceptions", *RUNTIME_INCLUDES, *RUNTIME_DEFINES], os.path.getsize(src)))
        elif src.endswith(".S"):
            jobs.append((src, obj, "-O2", [], os.path.getsize(src)))
        else:
            jobs.append((src, obj, "-O2", flags_c, os.path.getsize(src)))
    names = hle_names()
    table = os.path.join(GEN, "hle_table.c")
    changed = write_if_changed(table, '#include "host.h"\n\n' + "".join(f"GuestFn hle_{n};\n" for n in names)
                               + "const HleEntry hle_table[] = {\n" + "".join(f'    {{"{n}", hle_{n}}},\n' for n in names)
                               + f"    {{0, 0}}\n}};\nconst u32 hle_count = {len(names)};\n")
    if changed or needs(os.path.join(rt_out, "hle_table.o"), table, *headers):
        jobs.append((table, os.path.join(rt_out, "hle_table.o"), "-O1", flags_c, os.path.getsize(table)))
    missing = os.path.join(GEN, "wrap_missing.c")
    changed = write_if_changed(missing, '#include "host.h"\n\nvoid rt_wrap_missing(Cpu *c);\n'
                               + "".join(f"__attribute__((weak)) void f_{a:x}_orig(Cpu *c) {{ rt_wrap_missing(c); }}\n" for a in hle_wraps()))
    if changed or needs(os.path.join(rt_out, "wrap_missing.o"), missing, *headers):
        jobs.append((missing, os.path.join(rt_out, "wrap_missing.o"), "-O1", flags_c, os.path.getsize(missing)))
    nros = [n for n, src, _o in modules() if os.path.basename(os.path.dirname(src)) == "nro"]
    nro_table = os.path.join(GEN, "nro_table.c")
    changed = write_if_changed(nro_table, '#include "module.h"\n\n' + "".join(f"extern const RtModule rt_module_{n};\n" for n in nros)
                               + "const RtModule *const rt_nro_modules[] = {" + "".join(f"&rt_module_{n}, " for n in nros) + "0};\n"
                               + f"const u32 rt_nro_count = {len(nros)};\n")
    if changed or needs(os.path.join(rt_out, "nro_table.o"), nro_table):
        jobs.append((nro_table, os.path.join(rt_out, "nro_table.o"), "-O1", ["-std=gnu11", "-I", RUNTIME], os.path.getsize(nro_table)))
    return jobs


def compile_job(job):
    src, obj, opt, flags, _size = job
    os.makedirs(os.path.dirname(obj), exist_ok=True)
    compiler = CLANGXX if src.endswith(".cpp") else CLANG
    r = subprocess.run([compiler, "-c", opt, *CFLAGS, *flags, src, "-o", obj], capture_output=True, text=True, creationflags=LOW_PRIORITY)
    return (src, (r.stdout + r.stderr)) if r.returncode else None


def compile_all(jobs, what):
    if not jobs:
        print(f"{what}: up to date", flush=True)
        return True
    jobs = sorted(jobs, key=lambda j: j[4], reverse=True)   # biggest first: a few files take minutes
    t0 = time.time()
    failures = []
    with ThreadPoolExecutor(JOBS) as pool:
        futures = [pool.submit(compile_job, j) for j in jobs]
        for done, fut in enumerate(as_completed(futures), 1):
            r = fut.result()
            if r:
                failures.append(r)
            if done % 50 == 0 or done == len(jobs):
                print(f"{what}: {done}/{len(jobs)} compiled, {time.time() - t0:,.0f}s", flush=True)
    for src, err in failures[:8]:
        print(f"FAILED {src}:\n{err[:2500]}", flush=True)
    return not failures


def patched_dxvk():
    """DXVK's two libraries with plain sonames in OUT/lib: they are built as libdxvk_d3d11.so.0 (and name each other so), which an APK's
    loader cannot find (it has files named lib*.so). The names are cut in place (the file keeps its size: the three bytes of ".0" and the terminator become nulls), in
    each library's own soname and in the other's needed entry."""
    dxvk = os.path.join(A, "build", "dxvk", "src")
    os.makedirs(os.path.join(OUT, "lib"), exist_ok=True)
    names = ("libdxvk_d3d11.so", "libdxvk_dxgi.so")
    for name, folder in zip(names, ("d3d11", "dxgi")):
        with open(os.path.join(dxvk, folder, name), "rb") as f:
            data = f.read()
        for other in names:
            data = data.replace((other + ".0").encode() + b"\x00", other.encode() + b"\x00\x00\x00")
        with open(os.path.join(OUT, "lib", name), "wb") as f:
            f.write(data)


def link():
    patched_dxvk()
    objs = sorted(glob.glob(os.path.join(OUT, "rt", "*.o")))
    for _name, _src, out_dir in modules():
        objs += sorted(glob.glob(os.path.join(out_dir, "*.o")))
    lib = os.path.join(OUT, "libmain.so")
    rsp = lib + ".rsp"
    with open(rsp, "w") as f:
        f.write("\n".join('"%s"' % o.replace("\\", "/") for o in objs))
    t0 = time.time()
    cmd = [CLANGXX, *TARGET, "-shared", "-fuse-ld=lld", "@" + rsp, "-o", lib,
           os.path.join(A, "build", "vkd3d", "libvkd3d-shader.a"), os.path.join(OUT, "codecs", "libopus.a"), os.path.join(OUT, "codecs", "libvpx.a"),
           "-L" + os.path.join(OUT, "lib"), "-L" + os.path.join(A, "install", "arm64", "lib"),
           "-l:libdxvk_d3d11.so", "-l:libdxvk_dxgi.so", "-lSDL2", "-static-libstdc++", "-llog", "-landroid", "-lm", "-ldl",
           "-Wl,-z,max-page-size=16384", "-Wl,--allow-shlib-undefined", "-Wl,--build-id", "-Wl,-Map=" + lib + ".map"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        errors = [l for l in (r.stdout + r.stderr).splitlines() if "error" in l.lower() or "undefined" in l.lower()] or (r.stdout + r.stderr).splitlines()
        print("link failed\n" + "\n".join(errors)[:8000])
        return False
    print(f"linked {len(objs)} objects into {lib} ({os.path.getsize(lib) / 1048576:,.0f} MB) in {time.time() - t0:,.0f}s")
    return True


def main():
    os.makedirs(OUT, exist_ok=True)
    if not modules():
        raise SystemExit(f"no recompiled game in {os.path.join(PORT, 'build')} (tools/recomp.py all)")
    print(f"game tree: {PORT}", flush=True)
    if "--link-only" in sys.argv:
        return link()
    if not compile_all(runtime_jobs(), "runtime"):
        return False
    if "--runtime-only" in sys.argv:
        return True
    if not compile_all(game_jobs(), "game"):
        return False
    return link()


if __name__ == "__main__":
    sys.exit(0 if main() else 1)
