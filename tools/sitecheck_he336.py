"""v3.36 HE336: checks every byte-checked Site of src/hitpool.inc and src/recevict.inc against the GOG v7 exe (read only).

    python tools/sitecheck_he336.py [exe]

Default exe: the re_kit copy (sh4.exe, md5 15a85581...). Exit 0 when every site matches.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), 'src')
EXE = os.environ.get('SH4_EXE', 'sh4.exe')  # a copy of your stock GOG v7 exe
SITE = re.compile(r'\{\s*0x([0-9A-Fa-f]+),\s*\{([^}]*)\},\s*(\d+)\s*\}')


def reader(path):
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    ns = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    secs = []
    for i in range(ns):
        o = pe + 24 + opt + i * 40
        vsz, va, rsz, raw = struct.unpack_from('<IIII', d, o + 8)
        secs.append((va, vsz, raw))

    def rd(a, n):
        r = a - 0x400000
        for va, vsz, raw in secs:
            if va <= r < va + vsz:
                return d[raw + r - va:raw + r - va + n]
        return None
    return rd


def main(argv):
    rd = reader(argv[0] if argv else EXE)
    bad = n = 0
    for name in ('hitpool.inc', 'recevict.inc'):
        text = open(os.path.join(SRC, name), 'r').read()
        for m in SITE.finditer(text):
            va = int(m.group(1), 16)
            want = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(2)))
            ln = int(m.group(3))
            n += 1
            got = rd(va, ln)
            ok = len(want) == ln and got == want
            bad += not ok
            print('%s %s %08X len %d%s' % ('ok  ' if ok else 'BAD ', name, va, ln,
                                           '' if ok else ' (want %s, exe %s)' % (want.hex(' '), got.hex(' ') if got else None)))
    print('%d site(s), %d bad' % (n, bad))
    return 1 if bad or not n else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
