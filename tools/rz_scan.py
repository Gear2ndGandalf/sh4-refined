"""v3.48 RZ348: the item pickups of the room scripts, read from the stock exe (offline; the game is never touched).

Ports the room-script walk (0x52C4C0 SceneScriptSelect / 0x52CA00 SceneRoomScriptRun / 0x52C530 SceneScriptCommand, as
tools/em_script_dump.py does for the enemies) and lists every kind-4 record (an item pickup) with what Refined's
Randomizer needs: the item the record gets (ObjInfo +0x1C; commands 0x0A arg 2, 0x0F, 0x29: the last one wins) and the
script word that holds it, the model / texture override (+0x14 / +0x18: 0x0A args 0-1, 0x0B / 0x0D / 0x24, 0x0C / 0x0E),
its own event record (+0x54: 0x18, 0x20 arg 0), +0x58 (0x1C, 0x20 arg 1), the taken bit (+0x40: 0x1B, 0x25), the story
flags (+0x44 / +0x48: 0x19 / 0x26), the difficulty mask (+0x20: 0x0A arg 3, 0x10, 0x2A) and the create / task functions.

The plain-pickup rule (src/rando_sim.h RzIsPlain): kind 4, the stock event-object functions (cmd 1's set, no cmd 2-7),
no event record, no model / texture override, exactly one item word, item 1..0x0B and not 6 / 7.

    py -3 tools/rz_scan.py [--exe path] [--list] [--header]
--list prints every pickup; --header prints the per-visit table for src/rando_sim.h. Reads only the exe.
"""
import argparse
import collections
import hashlib
import struct
import sys

BASE = 0x400000
TAB1, TAB2 = 0x6553A4, 0x6553D8
EV_UPDATE, EV_CREATE, EV_DESTROY, EV_DRAW = 0x4CF330, 0x4CF4B0, 0x4CF0C0, 0x4CF350
STOCK_MD5 = '15a85581'
NAMES = {1: 'Pistol Bullets', 2: 'Revolver Bullets', 3: 'Silver Bullets', 4: 'Holy Candle', 5: 'Sword of Obedience',
         8: 'Saint Medallion', 9: 'Nutrition Drink', 0x0A: 'Ampoule', 0x0B: 'Portable Medical Kit'}


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

    def section(self, va):
        for name, start, size, rp, rs in self.sec:
            if start <= va < start + size:
                return name
        return None

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
    return {'kind': 0, 'funcs': 'set', 'item': 0, 'itemva': [], 'model': 0, 'tex': 0, 'record': 0, 'w58': 0, 'taken': 0,
            'need': 0, 'forbid': 0, 'diff': 0, 'ttype': 0, 'recva': []}


def step(r, tag, args, p):
    av = lambda i: p + 4 + 4 * i  # the VA of argument i
    if tag == 1:
        r['kind'] = args[0]
    elif tag in (2, 3, 4, 5, 6, 7):
        r['funcs'] = 'cmd%X' % tag
    elif tag == 0x0A:
        r['model'], r['tex'], r['item'], r['diff'] = args[0], args[1], args[2], args[3]
        r['itemva'].append(av(2))
    elif tag in (0x0B, 0x0D, 0x24):
        r['model'] = args[0]
    elif tag in (0x0C, 0x0E):
        r['tex'] = args[0]
    elif tag in (0x0F, 0x29):
        r['item'] = args[0]
        r['itemva'].append(av(0))
    elif tag in (0x10, 0x2A):
        r['diff'] = args[0]
    elif tag == 0x18:
        r['record'] = args[0]
        r['recva'].append(av(0))
    elif tag == 0x1C:
        r['w58'] = args[0]
    elif tag == 0x20:
        r['record'], r['w58'] = args[0], args[1]
        r['recva'].append(0)  # (v3.48 RZ348 r2: a 0x20 record is never patched)
    elif tag in (0x1B, 0x25):
        r['taken'] = args[0]
    elif tag == 0x19:
        r['need'] = args[0]
    elif tag == 0x26:
        r['forbid'] = args[0]
    elif tag == 0x1A:
        r['ttype'] = args[0] & 0xFFFF


def room_records(img, p):
    """0x52CA00's flush rule: the first command runs before the loop; command 1 flushes the object before it; the end
    of the room (command 0 or 0xF0F0F0F0) flushes the last one."""
    out, r, n, first, guard = [], new_rec(), 0, True, 0
    while guard < 100000:
        guard += 1
        w = img.u32(p)
        if not first and (w == 0xF0F0F0F0 or (w & 0xFFFF) == 0):
            break
        tag, num = w & 0xFFFF, w >> 16
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


# v3.48 RZ348 round 2: weapons. A pickup message script of the stock shape (the twin's event_scripts report 5):
# BANK 1; ASK a; CALL_ON_SHOW 50ABE0; CASE 0; NEXT; CASE 1; STOP; SAY b; GIVE item; STOP
def std_script(img, p, item):
    w = [img.u32(p + 4 * i) for i in range(16)]
    return (w[0] == 0xFFFF0401 and w[1] == 1 and w[2] == 0xFFFF0102 and w[4] == 0xFFFF0404 and w[5] == 0x50ABE0 and
            w[6] == 0xFFFF0201 and w[7] == 0xFFFF0302 and w[8] == 0xFFFF0202 and w[9] == 0xFFFF0301 and w[10] == 0xFFFF0101
            and w[12] == 0xFFFF0403 and w[13] == item and w[14] == 0xFFFF0301)


# an event record that adds nothing to the default pickup record 0x6184E4 but its script: key 0, no condition, RUN_MSG
# init 0x4CD5D0 / update 0x4CD680, no late / finish / files, args[0] a stock-shaped script giving `item`, args[1..3] 0
def simple_record(img, rec, item):
    if not (0x618000 <= rec < 0x694B78):
        return False
    w = [img.u32(rec + 4 * i) for i in range(11)]
    return (w[0] == 0 and w[1] == 0 and w[2] == 0x4CD5D0 and w[3] == 0x4CD680 and w[4] == 0 and w[5] == 0 and w[6] == 0
            and w[8] == 0 and w[9] == 0 and w[10] == 0 and std_script(img, w[7], item))


def default_script(img, item):  # ItemGetPickupScript's table 0x62DD18 (40 x {script, item}), stock-shaped
    for i in range(40):
        if img.u32(0x62DD18 + 8 * i + 4) == item:
            return std_script(img, img.u32(0x62DD18 + 8 * i), item)
    return False


def model_file(img, item):  # ItemGetModelFile's table 0x62D5E0 (153 x {model, texture, item}): the file of the model
    for i in range(153):
        if img.u32(0x62D5E0 + 12 * i + 8) == item:
            return img.u32(0x62D5E0 + 12 * i) & 0xFFF
    return -1


WNAMES = {0x10: 'Pistol', 0x11: 'Revolver', 0x12: 'Steel Pipe', 0x13: 'Paper-Cutting Knife', 0x14: 'Aluminium Bat',
          0x15: 'Driver', 0x17: '3-Wood', 0x19: '3-Iron', 0x1B: '4-Iron', 0x1D: '5-Iron', 0x1F: '6-Iron', 0x21: '7-Iron',
          0x23: '8-Iron', 0x25: '9-Iron', 0x27: 'Pitching Wedge', 0x29: 'Sand Wedge', 0x2B: 'Putter', 0x2D: 'Wine Bottle',
          0x2F: 'Spade', 0x30: 'Rusty Axe', 0x31: 'Pickaxe of Despair', 0x32: 'Stun Gun', 0x33: 'Bug Spray', 0x34: 'Torch',
          0x35: 'Chainsaw'}
HENRY_WEAPONS = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x29, 0x2B, 0x2D, 0x2F,
                 0x30, 0x31, 0x32, 0x33, 0x34, 0x35]  # (not the broken ones 0x16..0x2C even / 0x2E, not Eileen's 0x80-0x84)


def is_weapon_spot(img, r):
    return (r['kind'] == 4 and r['funcs'] == 'set' and r['model'] == 0 and r['tex'] == 0 and len(r['itemva']) == 1 and
            len(r['recva']) == 1 and r['recva'][0] and r['item'] in HENRY_WEAPONS and model_file(img, r['item']) == 0x57 and
            simple_record(img, r['record'], r['item']))


def is_plain(r):
    return (r['kind'] == 4 and r['funcs'] == 'set' and r['record'] == 0 and r['model'] == 0 and r['tex'] == 0
            and len(r['itemva']) == 1 and 1 <= r['item'] <= 0x0B and r['item'] not in (6, 7))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default='sh4.exe')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--header', action='store_true')
    a = ap.parse_args()
    md5 = hashlib.md5(open(a.exe, 'rb').read()).hexdigest()
    img = Image(a.exe)
    print('# exe %s md5 %s (stock %s...: %s)' % (a.exe, md5, STOCK_MD5, 'yes' if md5.startswith(STOCK_MD5) else 'NO'))
    seen = {}
    visits = []  # (stage, visit, script pointer)
    for stage in range(13):
        p1, p2 = img.u32(TAB1 + 4 * stage), img.u32(TAB2 + 4 * stage)
        visits.append((stage, 0, p1))
        if p2 != p1:
            visits.append((stage, 1, p2))
    kind4 = plain = 0
    items_all = collections.Counter()
    items_r302 = collections.Counter()
    excluded = collections.Counter()
    why = collections.Counter()
    flagged = 0
    table = []
    wspots, wtable = [], []  # v3.48 RZ348 r2: the weapon spots (a simple record of their own)
    for stage, visit, p in visits:
        if p in seen:
            print('# stage %d visit %d: the script of stage %d visit %d (%08X): counted once' % (stage, visit, *seen[p], p))
            continue
        seen[p] = (stage, visit)
        sec = img.section(p)
        n_plain = collections.Counter()
        n_weap = collections.Counter()
        rows = []
        for room, rp in sorted(rooms_of(img, p).items()):
            for n, r in room_records(img, rp):
                if r['kind'] != 4:
                    continue
                kind4 += 1
                ok = is_plain(r)
                if not ok and is_weapon_spot(img, r):  # v3.48 RZ348 r2
                    wspots.append((stage, visit, room, n, r['item'], r['taken'], r['record']))
                    n_weap[r['item']] += 1
                if ok:
                    plain += 1
                    items_all[r['item']] += 1
                    n_plain[r['item']] += 1
                    if stage in (1, 0x0A):
                        items_r302[r['item']] += 1
                    if r['need'] or r['forbid']:
                        flagged += 1
                    rows.append((room, n, r['item'], r['itemva'][0], r['taken']))
                    assert img.section(r['itemva'][0]) == '.data', hex(r['itemva'][0])
                else:
                    excluded[r['item']] += 1
                    w = []
                    if r['funcs'] != 'set': w.append('funcs')
                    if r['record']: w.append('record')
                    if r['model'] or r['tex']: w.append('model')
                    if len(r['itemva']) != 1: w.append('items%d' % len(r['itemva']))
                    if not (1 <= r['item'] <= 0x0B) or r['item'] in (6, 7): w.append('item')
                    why['+'.join(w)] += 1
                if a.list:
                    print('%X\t%d\t%3d\t%3d\t%s\titem %02X @%08X\ttaken %d\tmodel %X tex %X rec %08X w58 %X need %X forbid %X '
                          'diff %X tt %X %s' % (stage, visit, room, n, 'PLAIN' if ok else '-', r['item'],
                                               r['itemva'][-1] if r['itemva'] else 0, r['taken'], r['model'], r['tex'],
                                               r['record'], r['w58'], r['need'], r['forbid'], r['diff'], r['ttype'],
                                               r['funcs']))
        table.append((stage, visit, p, sec, sum(n_plain.values()), dict(n_plain), rows))
        wtable.append((stage, visit, sum(n_weap.values()), dict(n_weap)))
    print('# kind-4 records %d (distinct scripts), plain %d (with a story-flag condition: %d), others %d' % (
        kind4, plain, flagged, kind4 - plain))
    print('# plain by item: ' + ', '.join('%02X %s x%d' % (i, NAMES.get(i, '?'), c) for i, c in items_all.most_common()))
    print('# plain in Room 302 (stages 1 / 0A): %d (%s)' % (sum(items_r302.values()),
          ', '.join('%02X x%d' % (i, c) for i, c in items_r302.most_common())))
    print('# excluded by reason: ' + ', '.join('%s %d' % kv for kv in why.most_common()))
    print('# excluded items: ' + ', '.join('%02X x%d' % kv for kv in sorted(excluded.items())))
    for stage, visit, p, sec, n, byitem, rows in table:
        print('# stage %X visit %d script %08X (%s): %d plain  %s' % (stage, visit, p, sec, n,
              ' '.join('%02X:%d' % kv for kv in sorted(byitem.items()))))
    # every pickup model must be resident: item_model.bin (0xF000F003) and wp_model.bin (0xF000F057, the Sword) in the world
    # list (index 0 of [0x62D514 + 4 * k], k = stage + 13 in the 2nd half; enemies.inc EnmResident)
    for k in range(26):
        tab = img.u32(0x62D514 + 4 * k)
        if not tab:
            continue
        lst = img.u32(tab)
        ids = []
        for i in range(80):
            v = img.u32(lst + 4 * i)
            if v == 0x0FFFFFFF:
                break
            ids.append(v)
        print('# world list stage %X half %d: item_model.bin %s, wp_model.bin %s' % (
            k % 13, k // 13, 'yes' if 0xF000F003 in ids else 'NO', 'yes' if 0xF000F057 in ids else 'NO'))
    # v3.48 RZ348 r2: the weapon spots, the weapons a plain spot can take (a stock-shaped default script, wp_model.bin)
    print('# weapon spots (simple record, the weapon in wp_model.bin): %d' % len(wspots))
    for w in wspots:
        print('#   stage %X visit %d room %d n %d: %02X %s (taken bit %d, record %08X)' % (
            w[0], w[1], w[2], w[3], w[4], WNAMES.get(w[4], '?'), w[5], w[6]))
    for wi in HENRY_WEAPONS:
        recs = sorted({w[6] for w in wspots if w[4] == wi})
        print('# weapon %02X %-22s model file %X, default script %s, simple records %s' % (
            wi, WNAMES.get(wi, '?'), model_file(img, wi), 'yes' if default_script(img, wi) else 'no',
            ' '.join('%08X' % r for r in recs) or '-'))
    for stage, visit, n, byitem in wtable:
        if n:
            print('# stage %X visit %d: %d weapon spot(s)  %s' % (stage, visit, n, ' '.join('%02X:%d' % kv for kv in sorted(byitem.items()))))
    if a.header:
        print('// tools/rz_scan.py --header (stock exe md5 %s): plain pickups per world visit (stage, visit) and their items' % md5)
        print('// (plain pickups, weapon spots, counts of items 1..0x0B; the runtime scan of the live script must give the same)')
        print('static const RzVisitCount kRzStock[] = {')
        for (stage, visit, p, sec, n, byitem, rows), wrow in zip(table, wtable):
            cnt = [byitem.get(i, 0) for i in range(1, 12)]
            print('    { 0x%X, %d, 0x%08X, %3d, %d, { %s } },' % (stage, visit, p, n, wrow[2], ', '.join('%d' % c for c in cnt)))
        print('};')


if __name__ == '__main__':
    main()
