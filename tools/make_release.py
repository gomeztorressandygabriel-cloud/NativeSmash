#!/usr/bin/env python3
"""Builds the zip a player downloads from a NativeSmash release: next to nothing but the installer.

    NativeSmash-<version>.zip
        Install NativeSmash.exe     what the player double-clicks (tools/release/installer_stub.c, with the icon)
        NativeSmash/                everything else: the committed files of this repository, minus tests and the web site

The .exe hides that folder and starts "Install NativeSmash.bat" inside it.

    python tools/make_release.py 0.1.1            -> dist/NativeSmash-0.1.1.zip, made from what is committed (HEAD)

It needs gcc and windres (MinGW) on PATH for the .exe. Nothing of the game, no key and no firmware goes in: the
repository does not have them.
"""
import io
import os
import shutil
import subprocess
import sys
import tarfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STUB_DIR = os.path.join(ROOT, 'tools', 'release')
OUT_DIR = os.path.join(ROOT, 'dist')
WORK_DIR = os.path.join(ROOT, 'build_release')
EXE_NAME = 'Install NativeSmash.exe'
DATA_DIR = 'NativeSmash'
LEFT_OUT = ('tests/', 'site/', '.github/')


def build_stub():
    gcc, windres = shutil.which('gcc'), shutil.which('windres')
    if not gcc or not windres:
        sys.exit('make_release: gcc and windres (MinGW) are needed to build ' + EXE_NAME)
    os.makedirs(WORK_DIR, exist_ok=True)
    res = os.path.join(WORK_DIR, 'stub.res.o')
    exe = os.path.join(WORK_DIR, EXE_NAME)
    subprocess.run([windres, 'installer_stub.rc', '-O', 'coff', '-o', res], cwd=STUB_DIR, check=True)
    subprocess.run([gcc, '-Os', '-s', '-municode', '-mwindows', '-static', os.path.join(STUB_DIR, 'installer_stub.c'),
                    res, '-o', exe], check=True)
    return exe


def committed_files():
    """(path, bytes, mode) of every file of HEAD, from git archive."""
    data = subprocess.run(['git', 'archive', '--format=tar', 'HEAD'], cwd=ROOT, check=True, capture_output=True).stdout
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        for m in tar.getmembers():
            if not m.isfile():
                continue
            if m.name.startswith(LEFT_OUT) or m.name == '.gitattributes':
                continue
            yield m.name, tar.extractfile(m).read()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    version = sys.argv[1].lstrip('v')
    exe = build_stub()
    os.makedirs(OUT_DIR, exist_ok=True)
    out = os.path.join(OUT_DIR, 'NativeSmash-%s.zip' % version)
    count = 0
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(exe, EXE_NAME)
        for name, data in committed_files():
            z.writestr('%s/%s' % (DATA_DIR, name), data)
            count += 1
    print('%s: %s + %d files in %s/ (%.1f MB)' % (out, EXE_NAME, count, DATA_DIR, os.path.getsize(out) / 1e6))


if __name__ == '__main__':
    main()
