"""v3.59 FR359: checks every stock byte string of src/wv359_table.h and the cut-scene hook sites against the stock exe.

Usage: py -3 -I tools/fr359_sitecheck.py [path-to-stock-sh4.exe]
Default exe: sh4.exe (a copy of your stock GOG v7 exe, read only). Exit code 0 = all match.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TABLE = os.path.join(HERE, '..', 'src', 'wv359_table.h')
EXE = sys.argv[1] if len(sys.argv) > 1 else 'sh4.exe'

# the cut-scene sites of ratephase.inc / sh4windowmod.c (va, stock bytes)
EXTRA = [
    (0x5609FB, '8B 86 D8 00 00 00', 'cut snap: mov eax,[esi+0D8h] after t'),
    (0x560A6E, 'D9 05 50 7C 2E 01', 'stream step: fld [12E7C50h] (v3.2 hook, now the clock)'),
]


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    n = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    out = []
    at = pe + 24 + opt
    for i in range(n):
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, at + i * 40 + 8)
        out.append((base + va, max(vsize, rsize), raw, rsize))
    return out


def read(data, secs, va, n):
    for sva, size, raw, rsize in secs:
        if sva <= va < sva + size:
            off = va - sva
            if off + n > rsize:
                return None  # uninitialised (bss)
            return data[raw + off: raw + off + n]
    return None


def main():
    data = open(EXE, 'rb').read()
    secs = sections(data)
    rows = []
    text = open(TABLE, 'r', encoding='utf-8').read()
    for m in re.finditer(r'\{\s*(0x[0-9A-Fa-f]+),\s*(?:\d+,\s*RT_\w+,\s*)?(\d+),\s*\{([^}]*)\}', text):
        va = int(m.group(1), 16)
        n = int(m.group(2))
        bs = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(3))]
        rows.append((va, bytes(bs[:n]), 'table'))
    for va, hexs, what in EXTRA:
        rows.append((va, bytes(int(x, 16) for x in hexs.split()), what))
    bad = 0
    for va, want, what in rows:
        got = read(data, secs, va, len(want))
        ok = got == want
        bad += not ok
        print('%08X %-30s %s %s' % (va, want.hex(' '), 'ok' if ok else 'MISMATCH (exe %s)' % (got.hex(' ') if got else '?'), what))
    print('%d sites, %d mismatches' % (len(rows), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
