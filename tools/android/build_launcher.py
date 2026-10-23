#!/usr/bin/env python3
"""Builds NativeSmash-Launcher.apk (tools/android/launcher): the phone launcher (screen, controls, online, Play) with the
Vulkan check inside it. Same tools as build_vulkancheck.py (docs/ANDROID.md); the APK is signed with the debug key there.

Usage: build_launcher.py [--out <folder>] [--engine]

--engine puts the game in the app (NativeSmash-Android.apk): SDL's activity, libmain.so (tools/android/build_android.py) and the libraries it
needs. Without it, the launcher alone (NativeSmash-Launcher.apk), whose Play button says the engine is not in the app."""
import glob
import os
import shutil
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_vulkancheck as b  # noqa: E402

SRC = os.path.join(b.REPO, "tools", "android", "launcher")
WORK = os.path.join(b.REPO, "build", "android-launcher")
JAVA_ENV = {**os.environ, "JAVA_HOME": b.JDK}
ENGINE = "--engine" in sys.argv
GAME = os.path.join(b.ANDROID, "build", "game")
SDL_JAVA = os.path.join(b.ANDROID, "src", "SDL2-2.32.8", "SDL2-2.32.8", "android-project", "app", "src", "main", "java")


def engine_libs():
    """{apk path: file} of the native libraries of the engine build. DXVK's carry a versioned soname (libdxvk_d3d11.so.0), which Android's
    loader would look for by that name: it is cut to the plain name in place (the string's slot is longer, so the rest is null)."""
    dxvk = os.path.join(b.ANDROID, "build", "dxvk", "src")
    libs = {"libmain.so": os.path.join(GAME, "libmain.so"), "libSDL2.so": os.path.join(b.ANDROID, "install", "arm64", "lib", "libSDL2.so")}
    for name, folder in (("libdxvk_d3d11.so", "d3d11"), ("libdxvk_dxgi.so", "dxgi")):
        data = open(os.path.join(dxvk, folder, name), "rb").read()
        data = data.replace((name + ".0").encode() + b"\x00", name.encode() + b"\x00\x00")
        patched = os.path.join(WORK, name)
        with open(patched, "wb") as f:
            f.write(data)
        libs[name] = patched
    missing = [n for n, p in libs.items() if not os.path.exists(p)]
    if missing:
        sys.exit("the engine build needs: " + ", ".join(missing) + " (tools/android/build_android.py, docs/ANDROID.md)")
    return libs


def main():
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else os.path.join(b.REPO, "dist")
    shutil.rmtree(WORK, ignore_errors=True)
    for d in ("lib/arm64-v8a", "classes", "dex", "gen", "res/mipmap-xxxhdpi"):
        os.makedirs(os.path.join(WORK, d))
    # the icon
    from PIL import Image
    Image.open(os.path.join(b.REPO, "docs", "assets", "nativesmash-512.png")).convert("RGBA").resize((192, 192), Image.LANCZOS).save(
        os.path.join(WORK, "res", "mipmap-xxxhdpi", "ic_launcher.png"))
    # the Vulkan check, for this app's Compat class
    c = open(os.path.join(b.SRC, "vkcheck.c"), encoding="utf-8").read().replace(
        "Java_com_nativesmash_vulkancheck_MainActivity_report", "Java_com_nativesmash_app_Compat_report")
    check_c = os.path.join(WORK, "vkcheck.c")
    with open(check_c, "w", encoding="utf-8", newline="\n") as f:
        f.write(c)
    lib = os.path.join(WORK, "lib", "arm64-v8a", "libvkcheck.so")
    b.run([b.CLANG, "-shared", "-fPIC", "-O2", "-Wall", "-Wl,-z,max-page-size=16384", check_c, "-lvulkan", "-llog", "-o", lib])
    # resources and the manifest
    res_zip = os.path.join(WORK, "res.zip")
    b.run([os.path.join(b.BUILD_TOOLS, "aapt2.exe"), "compile", "--dir", os.path.join(WORK, "res"), "-o", res_zip])
    base = os.path.join(WORK, "base.apk")
    b.run([os.path.join(b.BUILD_TOOLS, "aapt2.exe"), "link", "--manifest", os.path.join(SRC, "AndroidManifest-engine.xml" if ENGINE else "AndroidManifest.xml"), "-I", b.PLATFORM,
           "-R", res_zip, "--java", os.path.join(WORK, "gen"), "--min-sdk-version", "29", "--target-sdk-version", "34",
           "--version-code", "1", "--version-name", "0.1", "-o", base])
    # the Java
    sources = glob.glob(os.path.join(SRC, "java", "**", "*.java"), recursive=True) + glob.glob(os.path.join(WORK, "gen", "**", "*.java"), recursive=True)
    if ENGINE:
        sources += glob.glob(os.path.join(SRC, "engine", "java", "**", "*.java"), recursive=True) + glob.glob(os.path.join(SDL_JAVA, "org", "libsdl", "app", "*.java"))
    b.run([b.JAVAC, "-encoding", "UTF-8", "--release", "8", "-Xlint:-options", "-classpath", b.PLATFORM, "-d", os.path.join(WORK, "classes"), *sources])
    classes = glob.glob(os.path.join(WORK, "classes", "**", "*.class"), recursive=True)
    b.run([os.path.join(b.BUILD_TOOLS, "d8.bat"), "--lib", b.PLATFORM, "--min-api", "29", "--output", os.path.join(WORK, "dex"), *classes], env=JAVA_ENV)
    unaligned = os.path.join(WORK, "unaligned.apk")
    with zipfile.ZipFile(base) as zin, zipfile.ZipFile(unaligned, "w", zipfile.ZIP_DEFLATED) as z:
        for item in zin.infolist():
            data = zin.read(item.filename)
            if item.compress_type == zipfile.ZIP_STORED:   # (resources.arsc must stay stored)
                z.writestr(item, data, compress_type=zipfile.ZIP_STORED)
            else:
                z.writestr(item, data)
        z.write(os.path.join(WORK, "dex", "classes.dex"), "classes.dex")
        native = {"libvkcheck.so": lib}
        if ENGINE:
            native.update(engine_libs())
        for name, path in native.items():
            info = zipfile.ZipInfo("lib/arm64-v8a/" + name)
            info.compress_type = zipfile.ZIP_STORED   # (stored, so that zipalign can page-align it)
            with open(path, "rb") as f:
                z.writestr(info, f.read())
    aligned = os.path.join(WORK, "aligned.apk")
    b.run([os.path.join(b.BUILD_TOOLS, "zipalign.exe"), "-f", "-P", "16", "4", unaligned, aligned])
    keystore = os.path.join(b.ANDROID, "debug.keystore")
    apk = os.path.join(out, "NativeSmash-Android.apk" if ENGINE else "NativeSmash-Launcher.apk")
    os.makedirs(out, exist_ok=True)
    b.run([os.path.join(b.BUILD_TOOLS, "apksigner.bat"), "sign", "--ks", keystore, "--ks-pass", "pass:android", "--key-pass", "pass:android",
           "--ks-key-alias", "nativesmash", "--out", apk, aligned], env=JAVA_ENV)
    print(b.run([os.path.join(b.BUILD_TOOLS, "apksigner.bat"), "verify", apk], env=JAVA_ENV).strip() or "signature verified")
    print(f"{apk}: {os.path.getsize(apk) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
