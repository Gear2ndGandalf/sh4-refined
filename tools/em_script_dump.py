"""v3.31 ENEMIES: offline port of the room-script walk (0x52C4C0 / 0x52CA00 / 0x52C530) over the stock exe.

Prints every kind-2 (enemy) placement as Refined's EmScanRoom sees it: stage, half, room, object number n (the low
word of the script id), type (from command 6 / 7: the create function against the type table 0x615348), mode word
(desc+0x24), id override (desc+0x2C), kill index (desc+0x30) and the position. Also checks the copy-id packing limits
(room < 64, n < 256) that src/enemies.inc relies on.

    py -3 tools/em_script_dump.py [--exe path] [--summary]
Reads only the exe (default: sh4.exe).
"""
import argparse
import struct
import sys

BASE = 0x400000
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

    def u32(self, va):
        for start, size, rp, rs in self.sec:
            if start <= va < start + size:
                off = va - start
                if off + 4 <= rs:
                    return struct.unpack_from('<I', self.data, rp + off)[0]
                return 0
        raise ValueError('%08X outside the image' % va)


def f32(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def walk(img, table, stage):
    p = img.u32(table + 4 * stage)
    rooms = {}
    if not p:
        return rooms
    guard = 0
    while img.u32(p) != 0xF0F0F0F0 and guard < 200000:
        guard += 1
        w = img.u32(p)
        tag, num = w & 0xFFFF, w >> 16
        if tag == 0:
            rooms[img.u32(p + 4)] = p + 8
        p += 4 * (num + 1)
    return rooms


def room_objects(img, p, func_type):
    """The 0x52CA00 flush rule: command 1 ends the previous object; the end of the room flushes the last."""
    objs = []
    d = None
    n = 0

    def new():
        return {'kind': 0, 'create': 0, 'type': -1, 'mode': 0, 'item': 0, 'kill': 0, 'need': 0, 'forbid': 0, 'pos': (0, 0, 0)}

    def step(d, tag, args):
        if tag == 1:
            d['kind'] = args[0]
        elif tag == 3:
            d['create'] = args[0]
        elif tag == 6:
            d['create'] = img.u32(args[0] + 4)
        elif tag == 7:
            d['create'] = img.u32(TYPE_TAB + 16 * args[0] + 4)
        elif tag == 8:
            d['pos'] = (f32(args[0]), f32(args[1]), f32(args[2]))
        elif tag == 10:
            d['mode'], d['item'], d['kill'] = args[0], args[2], args[3]
        elif tag in (11, 13, 36):
            d['mode'] = args[0]
        elif tag in (15, 41):
            d['item'] = args[0]
        elif tag in (16, 42):
            d['kill'] = args[0]
        elif tag == 25:  # v3.30 EN330: info+0x44, the story flag the placement needs (0x45B5A0)
            d['need'] = args[0]
        elif tag == 38:  # v3.30 EN330: info+0x48, the story flag that stops it (0x45B5C0)
            d['forbid'] = args[0]

    d = new()
    first = True
    guard = 0
    while guard < 100000:
        guard += 1
        w = img.u32(p)
        if not first and (w == 0xF0F0F0F0 or (w & 0xFFFF) == 0):
            break
        tag, num = w & 0xFFFF, w >> 16
        if tag == 1 and not first:
            objs.append((n, d))
            n += 1
            d = new()
        args = [img.u32(p + 4 + 4 * i) for i in range(num)]
        step(d, tag, args)
        first = False
        p += 4 * (num + 1)
    objs.append((n, d))
    for _, o in objs:
        o['type'] = func_type.get(o['create'], -1)
    return objs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default='sh4.exe')
    ap.add_argument('--summary', action='store_true')
    ap.add_argument('--flags', action='store_true', help='v3.30 EN330: also print the story flags (need / forbid)')
    a = ap.parse_args()
    img = Image(a.exe)
    func_type = {}
    for t in range(0x17):
        func_type.setdefault(img.u32(TYPE_TAB + 16 * t + 4), t)
    total = 0
    max_room = max_n = 0
    over = []
    for half, table in ((0, 0x6553A4), (1, 0x6553D8)):
        for stage in range(2, 9):
            for room, p in sorted(walk(img, table, stage).items()):
                for n, o in room_objects(img, p, func_type):
                    if o['kind'] != 2:
                        continue
                    total += 1
                    max_room = max(max_room, room)
                    max_n = max(max_n, n)
                    if room >= 64 or n >= 256:
                        over.append((stage, half, room, n))
                    if not a.summary:
                        print('%d\t%s\t%d\t%d\t%02X\tm%X v%X\t%08X\t%d\t%.0f %.0f %.0f' % (
                            stage, 'AB'[half], room, n, o['type'] & 0xFF, o['mode'] & 0xF, (o['mode'] >> 8) & 0xF,
                            o['item'], o['kill'] if o['kill'] < 0x80000000 else o['kill'] - (1 << 32), *o['pos'])
                            + ('	need %X forbid %X' % (o['need'], o['forbid']) if a.flags else ''))
    print('# kind-2 placements %d, max room %d, max n %d, outside the copy-id packing (room < 64, n < 256): %d %s'
          % (total, max_room, max_n, len(over), over[:8]), file=sys.stderr)


if __name__ == '__main__':
    main()
