"""v3.49 RZ349: the enemy placements of the room scripts, read from the stock exe (offline; the game is never touched).

Ports the room-script walk (0x52C4C0 SceneScriptSelect / 0x52CA00 SceneRoomScriptRun / 0x52C530 SceneScriptCommand)
and lists every kind-2 record (an enemy placement) with what Refined's Monsters randomizer needs: the type (command 7
arg 0 = the type index; command 6 = a pointer to a row of the type table 0x615348) and the script word that holds it,
the mode word ObjInfo +0x14 (desc+0x24; commands 0x0A arg 0, 0x0B, 0x0D, 0x24; bits 0-3 spawn mode, 8-11 variant,
12-15 the type's own bits: the Tremer's room box, the Gum Head's item) and its words, the id override +0x1C (0x0A arg 2,
0x0F, 0x29), the difficulty mask +0x18 (0x0A arg 1, 0x0C, 0x0E), the kill index +0x20 (0x0A arg 3, 0x10, 0x2A), the
story flags +0x44 / +0x48 (0x19 / 0x26), and commands 2-5 (single functions).

The rule (src/rzmon_sim.h RmIsSource): kind 2; exactly one type command (7) and no 2-6 or 0x0A; a type of the source
set (02, 03, 07, 08, 09, 0A); exactly one mode-word command with spawn mode 0, bits 4-7 0 or 1 (0x20..0x60 turn the
body onto a wall or ceiling, 0x45DBC0), bits 12-15 only for a Tremer (its room box; a Gum Head's are its item); no id
override; no story flag; difficulty mask 0x1F or none; one position; stage 2..8. v3.51 SN351: + the event dogs (a Sniffer
Dog in spawn mode 6 / 7, upright, no bits 12-15: asleep until story flag 0x130; Refined rolls them once it is set).

    py -3 tools/rzmon_scan.py [--exe path] [--list] [--header] [--summary]
--list prints every enemy placement; --header prints the per-visit table for src/rzmon_sim.h. Reads only the exe.
"""
import argparse
import collections
import hashlib
import struct
import sys

BASE = 0x400000
TAB1, TAB2 = 0x6553A4, 0x6553D8
TYPE_TAB = 0x615348
VARIANTS = 0x5B8BA0  # int per type: the number of variants (EnmTypeKeys' bound)
STOCK_MD5 = '15a85581'
SOURCE = {0x02, 0x03, 0x07, 0x08, 0x09, 0x0A}
# gEnemyVariantCounts 0x5B8BA0 (the stat rows per type; EnmTypeKeys' bound), read from the exe in main(); a source's variant
# must be below it (Hummer v2 / v3, Whitestool v1, Twin Victim v2 are special: left alone), except the Tremer, whose
# variants 1 / 2 are sizes of the one model (TremerCreate 0x48B940)
VARIANT_N = {}
NAMES = {1: 'type 01', 2: 'Whitestool', 3: 'Hummer', 4: 'mm', 5: 'Wall Man', 6: 'Wheelchair', 7: 'Gum Head',
         8: 'Twin Victim', 9: 'Tremer', 0x0A: 'Sniffer Dog', 0x0B: 'victim ghost', 0x0C: 'ghost 0C', 0x0D: 'ghost 0D',
         0x0E: 'Andrew', 0x0F: 'Richard', 0x10: 'Walter', 0x11: 'the Patient'}


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
            self.sec.append((name.rstrip(b'\0').decode('ascii', 'replace'), BASE + va, max(vs, rs), rp, rs))

    def u32(self, va):
        for name, start, size, rp, rs in self.sec:
            if start <= va < start + size:
                off = va - start
                if off + 4 <= rs:
                    return struct.unpack_from('<I', self.data, rp + off)[0]
                return 0
        raise ValueError('%08X outside the image' % va)


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


def new_rec():
    return {'kind': 0, 'type': -1, 'typeva': 0, 'tcmds': 0, 'cmd6': 0, 'funcs': 0, 'mode': 0, 'modeva': [], 'over': 0,
            'overs': 0, 'diff': 0x1F, 'diffs': 0, 'kill': 0, 'need': 0, 'forbid': 0, 'pos': (0, 0, 0), 'posn': 0,
            'yva': 0, 'cmd0a': 0}


def type_of_row(ptr):
    if ptr < TYPE_TAB or (ptr - TYPE_TAB) % 16:
        return -1
    return (ptr - TYPE_TAB) // 16


def step(r, tag, args, p):
    av = lambda i: p + 4 + 4 * i
    if tag == 1:
        r['kind'] = args[0]
    elif tag in (2, 3, 4, 5):
        r['funcs'] += 1
    elif tag == 6:
        r['cmd6'] += 1
        r['type'] = type_of_row(args[0])
        r['typeva'] = av(0)
        r['tcmds'] += 1
    elif tag == 7:
        r['type'] = args[0]
        r['typeva'] = av(0)
        r['tcmds'] += 1
    elif tag == 8:
        r['pos'] = tuple(struct.unpack('<f', struct.pack('<I', a))[0] for a in args[:3])
        r['posn'] += 1
        r['yva'] = av(1)
    elif tag == 0x0A:
        r['cmd0a'] += 1
        r['mode'] = args[0]
        r['modeva'].append(av(0))
        r['diff'] = args[1]
        r['diffs'] += 1
        r['over'] = args[2]
        r['overs'] += 1
        r['kill'] = args[3]
    elif tag in (0x0B, 0x0D, 0x24):
        r['mode'] = args[0]
        r['modeva'].append(av(0))
    elif tag in (0x0C, 0x0E):
        r['diff'] = args[0]
        r['diffs'] += 1
    elif tag in (0x0F, 0x29):
        r['over'] = args[0]
        r['overs'] += 1
    elif tag in (0x10, 0x2A):
        r['kill'] = args[0]
    elif tag == 0x19:
        r['need'] = args[0]
    elif tag == 0x26:
        r['forbid'] = args[0]


def walk_room(img, p, room):
    """the runner's rule from gRoomScripts[room]: a list of (n, record) in creation order"""
    out = []
    r = new_rec()
    first = True
    n = 0
    guard = 0
    while True:
        guard += 1
        if guard > 400000:
            raise RuntimeError('runaway room %d' % room)
        w = img.u32(p)
        tag, num = w & 0xFFFF, w >> 16
        if not first and (w == 0xF0F0F0F0 or tag == 0):
            break
        if tag == 1 and not first:
            out.append((n, r))
            n += 1
            r = new_rec()
        args = [img.u32(p + 4 + 4 * i) for i in range(num)]
        step(r, tag, args, p)
        first = False
        p += 4 * (num + 1)
    out.append((n, r))
    return out


def event_dog(r):
    """v3.51 SN351: src/rzmon_sim.h RmEventDogMode (a Sniffer Dog asleep until story flag 0x130)"""
    m = r['mode']
    return r['type'] == 0x0A and m <= 0xFFFF and (m & 0xF) in (6, 7) and (m & 0xF0) <= 0x10 and not (m >> 12) & 0xF


def why_not(stage, r):
    """'' = a source placement (src/rzmon_sim.h RmIsSource); else the reason"""
    if r['kind'] != 2:
        return 'not an enemy'
    if r['tcmds'] != 1 or r['cmd6'] or r['funcs'] or r['cmd0a']:
        return 'functions not from one command 7'
    if r['type'] not in SOURCE:
        return 'type never changed'
    if not (2 <= stage <= 8):
        return 'stage outside 2..8'
    if len(r['modeva']) != 1:
        return 'not one mode-word command'
    if (r['mode'] & 0xF or r['mode'] & ~0xFFFF) and not event_dog(r):  # v3.51 SN351: + the event dogs
        return 'event spawn mode'
    if (r['mode'] & 0xF0) > 0x10:
        return 'on a wall or ceiling'
    if r['type'] == 7 and (r['mode'] >> 12) & 0xF:
        return 'a Gum Head with an item'
    if r['type'] != 9 and (r['mode'] >> 12) & 0xF:
        return 'type bits 12-15'
    if r['overs']:
        return 'id override'
    if r['need'] or r['forbid']:
        return 'story flag'
    if r['diffs'] > 1 or (r['diffs'] and r['diff'] != 0x1F):
        return 'difficulty mask'
    if r['posn'] != 1:
        return 'not one position'
    if r['type'] != 9 and (r['mode'] >> 8) & 0xF >= VARIANT_N[r['type']]:
        return 'a variant past the stat rows'
    return ''


def scan(img):
    seen = {}
    rows = []
    for stage in range(13):
        for visit, tab in ((0, TAB1), (1, TAB2)):
            p = img.u32(tab + 4 * stage)
            if visit == 1 and p == img.u32(TAB1 + 4 * stage):
                continue
            key = p
            if key in seen:
                continue
            seen[key] = (stage, visit)
            for room, start in sorted(rooms_of(img, p).items()):
                for n, r in walk_room(img, start, room):
                    if r['kind'] != 2:
                        continue
                    rows.append((stage, visit, p, room, n, r, why_not(stage, r)))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=r'C:/ClaudeCode/CModding/SH4Decomp/orig/sh4.exe')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--header', action='store_true')
    ap.add_argument('--summary', action='store_true')
    a = ap.parse_args()
    raw = open(a.exe, 'rb').read()
    md5 = hashlib.md5(raw).hexdigest()
    img = Image(a.exe)
    for t in range(0x20):
        VARIANT_N[t] = img.u32(VARIANTS + 4 * t) if t < 0x13 else 0
    print('exe %s md5 %s%s' % (a.exe, md5, '' if md5.startswith(STOCK_MD5) else ' (NOT the stock GOG v7 exe)'))
    # the code the game side checks or patches (byte for byte; rzmon.inc / randomizer.inc / enemies.inc)
    sites = [(0x46545D, '68901B00006810F0FC0050E84367FCFF', 'EnemyManagerSave: push 1B90h; push 0FCF010h; push eax; call 42BBB0 '
              '(the save filter re-aims 0x465468)'),
             (0x52CAA9, 'E852FEFFFF', "the runner's create call (the cheat table's 7700 hooks it)"),
             (0x52CB2B, 'E8D0FDFFFF', "the runner's last create call (the cheat table's 7700 hooks it)"),
             (0x4F0AF1, '8B0D945AFD0051E803BF0300', 'the room load: push [FD5A94]; call 52CA00 (enemies.inc EnmWrap)'),
             (0x49B70A, '8983A4000000', "the Gum Head's group-call store (Playable Eileen's guard hooks it)"),
             (0x45C3C0, '8B4424048B8008040000', "the position getter (Playable Eileen's guard hooks it)")]
    for va, hexs, what in sites:
        got = b''.join(struct.pack('<I', img.u32(va + i)) for i in range(0, len(hexs) // 2 + 3, 4))[:len(hexs) // 2]
        print('%08X %s %s' % (va, 'stock' if got.hex().upper() == hexs else 'NOT STOCK (%s)' % got.hex().upper(), what))
    for t in sorted(SOURCE):
        print('type %02X row %08X: update %08X create %08X destroy %08X late %08X' % (
            (t, TYPE_TAB + 16 * t) + tuple(img.u32(TYPE_TAB + 16 * t + 4 * i) for i in range(4))))
    rows = scan(img)
    src = [x for x in rows if not x[6]]
    print('kind-2 placements: %d; source placements: %d (%d of them event dogs)' % (len(rows), len(src),
                                                                                    sum(1 for x in src if event_dog(x[5]))))
    why = collections.Counter(x[6] for x in rows if x[6])
    for k, v in why.most_common():
        print('  not a source: %-36s %d' % (k, v))
    bytype = collections.Counter((x[5]['type'], (x[5]['mode'] >> 8) & 0xF) for x in src)
    print('source by type / variant: ' + ', '.join('%02X v%d x%d' % (t, v, c) for (t, v), c in sorted(bytype.items())))
    print('source mode words: ' + ', '.join('%04X x%d' % (m, c) for m, c in
                                            sorted(collections.Counter(x[5]['mode'] for x in src).items())))
    print('source with a mode-word command: %d, without: %d' % (sum(1 for x in src if x[5]['modeva']),
                                                               sum(1 for x in src if not x[5]['modeva'])))
    print('variant counts (0x5B8BA0): ' + ', '.join('%02X:%d' % (t, img.u32(VARIANTS + 4 * t)) for t in range(1, 0x13)))
    alltypes = collections.Counter((x[5]['type'], (x[5]['mode'] >> 8) & 0xF) for x in rows)
    print('every kind-2 by type / variant: ' + ', '.join('%02X v%d x%d' % (t, v, c) for (t, v), c in sorted(alltypes.items())))
    if a.summary or a.header:
        per = collections.OrderedDict()
        for stage, visit, p, room, n, r, w in rows:
            k = (stage, visit, p)
            per.setdefault(k, [0, 0, collections.Counter()])
            per[k][0] += 1
            if not w:
                per[k][1] += 1
                per[k][2][r['type']] += 1
        if a.summary:
            for (stage, visit, p), (k2, ns, c) in per.items():
                print('stage %X visit %d script %08X: %d kind-2, %d source: %s' %
                      (stage, visit, p, k2, ns, ', '.join('%02X x%d' % (t, n) for t, n in sorted(c.items()))))
        if a.header:
            rm = collections.Counter((x[0], x[1], x[3]) for x in src)
            print('// the most source placements in one room: %d' % max(rm.values()))
            print('// tools/rzmon_scan.py --header (stock exe md5 %s): stage, visit, script, kind-2 placements, source '
                  'placements, by source type 02 03 07 08 09 0A' % md5)
            for (stage, visit, p), (k2, ns, c) in per.items():
                print('    { 0x%X, %d, 0x%08X, %3d, %3d, { %s } },' %
                      (stage, visit, p, k2, ns, ', '.join('%d' % c[t] for t in (2, 3, 7, 8, 9, 0x0A))))
    if a.list:
        for stage, visit, p, room, n, r, w in rows:
            print('%X:%d room %02X n %3d type %02X%s mode %04X%s over %08X diff %X kill %d need %X forbid %X pos %.0f %.0f %.0f %s'
                  % (stage, visit, room, n, r['type'] & 0xFF, ' (cmd6)' if r['cmd6'] else '', r['mode'] & 0xFFFF,
                     '' if r['modeva'] else ' (none)', r['over'], r['diff'], r['kill'], r['need'], r['forbid'],
                     r['pos'][0], r['pos'][1], r['pos'][2], w or ('SOURCE (event dog)' if event_dog(r) else 'SOURCE')))


if __name__ == '__main__':
    sys.exit(main())
