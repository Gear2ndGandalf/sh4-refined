"""v3.49 RZ349L: the Randomizer's looks, read from the stock exe (offline; the game is never touched).

1. Ghosts: every kind-2 placement of the room scripts (0x52C4C0 / 0x52CA00 / 0x52C530's rules, as tools/em_script_dump.py
   and tools/rz_scan.py walk them) with the rule of src/rzlooks_sim.h RzgMayMove: a victim (0B any variant, 0C v1, 0D v1),
   spawn mode 0, its own ghost id FFFF00nn, made by cmd 7 alone (no cmd 2-6), exactly one cmd 8 (the position words that
   the Randomizer writes and puts back). The type is also taken from the create function (cmd 3 / 6 / 7 against the type
   table 0x615348) to show that no victim is made another way.
2. The outfit: the bytes of SaveGetNurseUnlocked 0x56F590 (the unlock flag 0x13D1A50) and the file lists that hold
   eil.bin 0xF000F058 (the companion's Dress; eol.bin = + 1).

    py -3 tools/rzl_scan.py [--exe path] [--list] [--header]
--list prints every victim; --header prints the numbers src/rzlooks_sim.h and test/rzlooks_sim_test.c use.
"""
import argparse
import hashlib
import struct
import sys

BASE = 0x400000
TAB1, TAB2 = 0x6553A4, 0x6553D8
TYPE_TAB = 0x615348


class Image:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        so = pe + 24 + struct.unpack_from('<H', d, pe + 20)[0]
        self.sec = []
        for i in range(nsec):
            name, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * i)
            self.sec.append((BASE + va, max(vs, rs), rp, rs))

    def raw(self, va, n):
        for start, size, rp, rs in self.sec:
            if start <= va < start + size:
                off = va - start
                return self.data[rp + off:rp + off + n]
        raise ValueError('%08X outside the image' % va)

    def u32(self, va):
        for start, size, rp, rs in self.sec:
            if start <= va < start + size:
                off = va - start
                if off + 4 <= rs:
                    return struct.unpack_from('<I', self.data, rp + off)[0]
                return 0
        return None


def f32(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def rooms_of(img, p):
    rooms = {}
    guard = 0
    while img.u32(p) != 0xF0F0F0F0 and guard < 400000:
        guard += 1
        w = img.u32(p)
        tag, num = w & 0xFFFF, w >> 16
        if tag == 0:
            rooms[img.u32(p + 4)] = p + 8
        p += 4 * (num + 1)
    return rooms


def objects(img, p, func_type):
    """The runner's flush rule; per object the fields rzlooks_sim.h reads."""
    out = []

    def new():
        return {'kind': 0, 'funcs': 0, 'type': -1, 'ctype': -1, 'create': 0, 'mode': 0, 'id': 0, 'pos': None, 'posva': 0,
                'posw': 0}
    o = new()
    first = True
    guard = 0
    while guard < 100000:
        guard += 1
        w = img.u32(p)
        if not first and (w == 0xF0F0F0F0 or (w & 0xFFFF) == 0):
            break
        tag, num = w & 0xFFFF, w >> 16
        if tag == 1 and not first:
            out.append(o)
            o = new()
        a = [img.u32(p + 4 + 4 * i) for i in range(num)]
        if tag == 1 and num >= 1:
            o['kind'] = a[0]
        elif tag in (2, 3, 4, 5, 6):
            o['funcs'] = 1
            if tag == 3:
                o['create'] = a[0]
            if tag == 6:
                o['create'] = img.u32(a[0] + 4)
        elif tag == 7 and num >= 1:
            o['type'] = a[0] if a[0] < 0x40 else -1
            o['create'] = img.u32(TYPE_TAB + 16 * a[0] + 4)
        elif tag == 8 and num >= 3:
            o['pos'] = (a[0], a[1], a[2])
            o['posva'] = p + 4
            o['posw'] += 1
        elif tag == 0x0A and num >= 3:
            o['mode'], o['id'] = a[0], a[2]
        elif tag in (0x0B, 0x0D, 0x24) and num >= 1:
            o['mode'] = a[0]
        elif tag in (0x0F, 0x29) and num >= 1:
            o['id'] = a[0]
        first = False
        p += 4 * (num + 1)
    out.append(o)
    for x in out:
        x['ctype'] = func_type.get(x['create'], -1)
    return out


def victim(t, v):
    return t == 0x0B or (t in (0x0C, 0x0D) and v == 1)


def story(t, v):
    return (t in (0x0C, 0x0D) and v == 0) or t in (0x0E, 0x0F)


def ghost_id(i):
    return (i & 0xFFFFFF00) == 0xFFFF0000


def may_move(o):
    v = (o['mode'] >> 8) & 0xF
    return (o['kind'] == 2 and o['type'] >= 0 and victim(o['type'], v) and not story(o['type'], v) and (o['mode'] & 0xF) == 0
            and ghost_id(o['id']) and not o['funcs'] and o['posw'] == 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default='C:/ClaudeCode/CModding/SH4Decomp/orig/sh4.exe')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--header', action='store_true')
    a = ap.parse_args()
    img = Image(a.exe)
    print('# exe md5 %s' % hashlib.md5(img.data).hexdigest())
    func_type = {}
    for t in range(0x17):
        func_type.setdefault(img.u32(TYPE_TAB + 16 * t + 4), t)
    seen = {}
    movers = victims = other_way = most_m = most_spots = 0
    rooms_with = 0
    for stage in range(13):
        for half, tab in ((0, TAB1), (1, TAB2)):
            p = img.u32(tab + 4 * stage)
            if p in seen:
                continue
            seen[p] = (stage, half)
            for room, start in sorted(rooms_of(img, p).items()):
                objs = objects(img, start, func_type)
                m = [o for o in objs if may_move(o)]
                spots = [o for o in objs if o['kind'] == 2 and o['posw'] >= 1]
                for n, o in enumerate(objs):
                    if o['kind'] != 2:
                        continue
                    v = (o['mode'] >> 8) & 0xF
                    t = o['type'] if o['type'] >= 0 else o['ctype']
                    if o['type'] < 0 and o['ctype'] >= 0 and victim(o['ctype'], v):
                        other_way += 1
                    if victim(t, v) and not story(t, v) and (o['mode'] & 0xF) == 0 and ghost_id(o['id']):
                        victims += 1
                        if a.list:
                            x, y, z = (f32(u) for u in o['pos']) if o['pos'] else (0, 0, 0)
                            print('%d\t%s\t%d\t%d\t%02X v%X\t%08X\t%s\tpos %08X\t%.0f %.0f %.0f' % (
                                stage, 'AB'[half], room, n, t, v, o['id'], 'moves' if may_move(o) else 'stays',
                                o['posva'], x, y, z))
                movers += len(m)
                rooms_with += bool(m)
                most_m = max(most_m, len(m))
                most_spots = max(most_spots, len(spots))
    print('# %d distinct scripts; victim placements (spawn mode 0, FFFF00nn) %d, may move %d, in %d room(s); at most %d in a '
          'room, at most %d kind-2 placements in a room; victims not made by cmd 7: %d' % (
              len(seen), victims, movers, rooms_with, most_m, most_spots, other_way))
    b = img.raw(0x56F590, 6)
    print('# 0056F590 SaveGetNurseUnlocked: %s (%s)' % (b.hex(' '), 'mov eax,[013D1A50] / ret' if b == bytes.fromhex(
        'a1501a3d01c3') else 'NOT the expected code'))
    for k in range(26):
        tab = img.u32(0x62D514 + 4 * k)
        if not tab:
            continue
        hits = []
        for r in range(64):
            lst = img.u32(tab + 4 * r)
            if lst is None:
                break
            if not lst:
                continue
            ids = []
            for i in range(160):
                x = img.u32(lst + 4 * i)
                if x is None or x == 0x0FFFFFFF:
                    break
                ids.append(x)
            if 0xF000F058 in ids or 0xF000F059 in ids:
                hits.append('world' if r == 0 else 'room %d' % r)
        if hits:
            print('# file lists [%d] (stage %d, %s half): eil.bin in %s' % (k, k % 13, '2nd' if k >= 13 else '1st', ', '.join(hits)))
    if a.header:
        print('#define RZG_STOCK_VICTIMS %d' % victims)
        print('#define RZG_STOCK_MOVERS %d' % movers)
        print('#define RZG_STOCK_ROOMS %d' % rooms_with)
    return 0


if __name__ == '__main__':
    sys.exit(main())
