#!/usr/bin/env python3
"""Assembles what the Android app needs from the game tree (the PC build's dump/), to put in the app's folder on the phone
(Android/data/com.nativesmash.app/files): dump/modules (the game's code images and the DLC list), dump/fonts and dump/nro, with
romfs.cfg rewritten to the phone's path of romfs.bin. romfs.bin itself (17.6 GB, the game's data) is copied beside dump/ by the
push script. Nothing here is the game's own file that you did not already have: it is made from your copy, for your own phone.

Usage: make_data.py [--port <dir>] [--out <folder>]   (default out: dist/NativeSmash-Android-data)"""
import os
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
APP_FILES = "/sdcard/Android/data/com.nativesmash.app/files"


def arg(name, default):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    port = os.path.abspath(arg("--port", REPO))
    out = os.path.abspath(arg("--out", os.path.join(REPO, "dist", "NativeSmash-Android-data")))
    dump = os.path.join(port, "dump")
    for need in ("modules", "fonts", "nro"):
        if not os.path.isdir(os.path.join(dump, need)):
            sys.exit(f"{os.path.join(dump, need)} is not there: build the game on the PC first (Install NativeSmash)")
    shutil.rmtree(out, ignore_errors=True)
    for need in ("modules", "fonts", "nro"):
        shutil.copytree(os.path.join(dump, need), os.path.join(out, "dump", need))
    with open(os.path.join(out, "dump", "modules", "romfs.cfg"), "w", newline="\n") as f:
        f.write(f"{APP_FILES}/romfs.bin\n0\n")
    total = sum(os.path.getsize(os.path.join(d, n)) for d, _s, names in os.walk(out) for n in names)
    print(f"{out}: {total / 1048576:,.0f} MB")
    print(f"put it on the phone (USB debugging on):  adb push \"{out}\\dump\" {APP_FILES}/")
    print(f"and the game's data:                      adb push \"{os.path.join(port, 'romfs.bin')}\" {APP_FILES}/romfs.bin")


if __name__ == "__main__":
    main()
