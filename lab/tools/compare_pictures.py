"""Compares the lockstep pictures of two runs (docs/NETPLAY.md): python compare_pictures.py <folder A> <folder B>.

For every lockstep_<frame>.bmp in both: whether the files are identical, the share of pixels that differ
(a channel off by more than 24), and the box they are in. Prints a table, and no picture leaves the PC."""
import os
import struct
import sys


def read_bmp(path):
    with open(path, 'rb') as f:
        data = f.read()
    off, = struct.unpack_from('<I', data, 10)
    w, h = struct.unpack_from('<ii', data, 18)
    bpp, = struct.unpack_from('<H', data, 28)
    if bpp != 24:
        raise ValueError(f'{path}: {bpp} bits a pixel')
    h = abs(h)
    row = (w * 3 + 3) & ~3
    return w, h, row, data[off:off + row * h], data


def compare(a, b):
    wa, ha, row, pa, ra = read_bmp(a)
    wb, hb, _, pb, rb = read_bmp(b)
    if (wa, ha) != (wb, hb):
        return f'different sizes {wa}x{ha} / {wb}x{hb}'
    if ra == rb:
        return 'IDENTICAL'
    diff = 0
    x0, y0, x1, y1 = wa, ha, -1, -1
    for y in range(0, ha, 2):           # every other line and column: fast enough, and plenty to see
        base = y * row
        for x in range(0, wa, 2):
            i = base + x * 3
            if abs(pa[i] - pb[i]) > 24 or abs(pa[i + 1] - pb[i + 1]) > 24 or abs(pa[i + 2] - pb[i + 2]) > 24:
                diff += 1
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    total = ((wa + 1) // 2) * ((ha + 1) // 2)
    if not diff:
        return 'almost identical (only tiny colour differences)'
    return f'{100.0 * diff / total:.2f}% of pixels differ, in the box x {x0}-{x1}, y {y0}-{y1} (of {wa}x{ha})'


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    a, b = sys.argv[1], sys.argv[2]
    names = sorted(n for n in os.listdir(a) if n.startswith('lockstep_') and n.endswith('.bmp'))
    shown = 0
    for n in names:
        if os.path.exists(os.path.join(b, n)):
            print(f'{n}: {compare(os.path.join(a, n), os.path.join(b, n))}')
            shown += 1
    print(f'{shown} pictures compared ({len(names)} in {a})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
