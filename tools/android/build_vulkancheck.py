#!/usr/bin/env python3
"""Builds NativeSmash-VulkanCheck.apk (tools/android/vulkancheck): a tiny app that shows what a phone's Vulkan driver offers
against what DXVK needs. Uses the Android tools in deps/android (the NDK, a JDK, and the SDK's build-tools and platform),
installed by hand as docs/ANDROID.md says. The APK is signed with a debug key made here (deps/android/debug.keystore).

Usage: build_vulkancheck.py [--out <folder>]"""
import glob
import os
import shutil
import subprocess
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ANDROID = os.path.join(REPO, "deps", "android")
SRC = os.path.join(REPO, "tools", "android", "vulkancheck")
WORK = os.path.join(REPO, "build", "android-vulkancheck")


def first(pattern):
    found = sorted(glob.glob(pattern))
    if not found:
        sys.exit(f"not found: {pattern} (see docs/ANDROID.md)")
    return found[-1]


NDK = first(os.path.join(ANDROID, "ndk", "android-ndk-*"))
JDK = first(os.path.join(ANDROID, "jdk", "jdk-*"))
BUILD_TOOLS = first(os.path.join(ANDROID, "sdk", "build-tools", "*"))
PLATFORM = first(os.path.join(ANDROID, "sdk", "platforms", "android-*", "android.jar"))
CLANG = os.path.join(NDK, "toolchains", "llvm", "prebuilt", "windows-x86_64", "bin", "aarch64-linux-android29-clang.cmd")
JAVAC, KEYTOOL = os.path.join(JDK, "bin", "javac.exe"), os.path.join(JDK, "bin", "keytool.exe")


def run(args, **kw):
    r = subprocess.run(args, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit(f"failed: {' '.join(args)}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def main():
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else os.path.join(REPO, "dist")
    shutil.rmtree(WORK, ignore_errors=True)
    for d in ("lib/arm64-v8a", "classes", "dex"):
        os.makedirs(os.path.join(WORK, d))
    lib = os.path.join(WORK, "lib", "arm64-v8a", "libvkcheck.so")
    run([CLANG, "-shared", "-fPIC", "-O2", "-Wall", "-Wl,-z,max-page-size=16384", os.path.join(SRC, "vkcheck.c"),
         "-lvulkan", "-llog", "-o", lib])
    java = os.path.join(SRC, "com", "nativesmash", "vulkancheck", "MainActivity.java")
    run([JAVAC, "--release", "8", "-Xlint:-options", "-classpath", PLATFORM, "-d", os.path.join(WORK, "classes"), java])
    classes = glob.glob(os.path.join(WORK, "classes", "**", "*.class"), recursive=True)
    run([os.path.join(BUILD_TOOLS, "d8.bat"), "--lib", PLATFORM, "--min-api", "29", "--output", os.path.join(WORK, "dex"), *classes],
        env={**os.environ, "JAVA_HOME": JDK})
    base = os.path.join(WORK, "base.apk")
    run([os.path.join(BUILD_TOOLS, "aapt2.exe"), "link", "--manifest", os.path.join(SRC, "AndroidManifest.xml"), "-I", PLATFORM,
         "--min-sdk-version", "29", "--target-sdk-version", "34", "-o", base])
    unaligned = os.path.join(WORK, "unaligned.apk")
    with zipfile.ZipFile(base) as zin, zipfile.ZipFile(unaligned, "w", zipfile.ZIP_DEFLATED) as z:
        for item in zin.infolist():
            z.writestr(item, zin.read(item.filename))
        z.write(os.path.join(WORK, "dex", "classes.dex"), "classes.dex")
        info = zipfile.ZipInfo("lib/arm64-v8a/libvkcheck.so")
        info.compress_type = zipfile.ZIP_STORED   # (stored, so that zipalign can page-align it)
        with open(lib, "rb") as f:
            z.writestr(info, f.read())
    aligned = os.path.join(WORK, "aligned.apk")
    run([os.path.join(BUILD_TOOLS, "zipalign.exe"), "-f", "-P", "16", "4", unaligned, aligned])
    keystore = os.path.join(ANDROID, "debug.keystore")
    if not os.path.exists(keystore):
        run([KEYTOOL, "-genkeypair", "-keystore", keystore, "-storepass", "android", "-keypass", "android", "-alias", "nativesmash",
             "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000", "-dname", "CN=NativeSmash debug"])
    apk = os.path.join(out, "NativeSmash-VulkanCheck.apk")
    os.makedirs(out, exist_ok=True)
    run([os.path.join(BUILD_TOOLS, "apksigner.bat"), "sign", "--ks", keystore, "--ks-pass", "pass:android", "--key-pass", "pass:android",
         "--ks-key-alias", "nativesmash", "--out", apk, aligned], env={**os.environ, "JAVA_HOME": JDK})
    print(run([os.path.join(BUILD_TOOLS, "apksigner.bat"), "verify", "--verbose", apk], env={**os.environ, "JAVA_HOME": JDK}).strip())
    print(f"{apk}: {os.path.getsize(apk) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
