"""SH4 Refined v3.0 (true 60): builds the per-tick tables from the research JSONs.

Inputs (S = shared src): research/framerate/v2/fps_tail.json (v2.0 tail) + v5/true60_monsters.json, true60_misc.json,
true60_companions.json (if present). Every orig is re-read from re_kit/sh4.exe.
Outputs (into the given src folder):
  fps_tail_table.h  - the v2.0 table plus every rate-scaled v5 row (same RateTail format; area tags t60_*)
  true60_fixed.h    - rate-independent patches: clip-seconds call redirects (kT60Clip) and byte patches (kT60Bytes)
  true60_rows.txt   - a list of what went where, and what was left for hand-written hooks
"""
import json, os, sys, struct

S = os.environ.get('SH4_RESEARCH', 'research_src')  # the author's research folder (re_kit\sh4.exe, research\framerate\v5); not in this repository
OUT = sys.argv[1]
EXE = open(os.path.join(S, 're_kit', 'sh4.exe'), 'rb').read()


def file_off(va):
    # PE sections
    pe = struct.unpack_from('<I', EXE, 0x3C)[0]
    n = struct.unpack_from('<H', EXE, pe + 6)[0]
    opt = struct.unpack_from('<H', EXE, pe + 20)[0]
    sec = pe + 24 + opt
    rva = va - 0x400000
    for i in range(n):
        vsz, vaddr, rsz, raddr = struct.unpack_from('<IIII', EXE, sec + i * 40 + 8)
        if vaddr <= rva < vaddr + max(vsz, rsz):
            if rva - vaddr >= rsz:
                return None  # bss
            return raddr + rva - vaddr
    return None


def exe_bytes(va, n):
    o = file_off(va)
    return None if o is None else EXE[o:o + n]


def hb(s):
    return bytes(int(b, 16) for b in s.split())


tail = json.load(open(os.path.join(S, r'research\framerate\v2\fps_tail.json')))
have = {int(x['va'], 16) for x in tail}
rows, clip, raw, hooks, skipped = [], [], [], [], []


def add_tail(va, orig, off, otype, new, area, const_va=None, note=''):
    va = int(va, 16) if isinstance(va, str) else va
    ob = hb(orig) if isinstance(orig, str) else orig
    got = exe_bytes(va, len(ob))
    if got is not None and got != ob:
        sys.exit('orig mismatch at %X: json %s exe %s' % (va, ob.hex(' '), got.hex(' ')))
    if va in have:
        skipped.append('%X already in fps_tail (%s)' % (va, area)); return
    have.add(va)
    e = {'va': '0x%X' % va, 'orig': ob.hex(' ').upper(), 'operand_offset': off, 'operand_type': otype,
         'new': {k: new[k] for k in ('60', '120', '180')}, 'area': area}
    if const_va: e['const_va'] = const_va
    rows.append(e)


# ---- monsters
M = json.load(open(os.path.join(S, r'research\framerate\v5\true60_monsters.json')))
for x in M['sites'] + M['extra']:
    f = x.get('fix') or {}
    k = f.get('kind', 'none')
    if k == 'none':
        continue
    if k == 'hook':
        hooks.append('monsters %s %s' % (f['va'], x.get('what', '')[:80])); continue
    subs = f['sites'] if 'sites' in f else [f]
    for s in subs:
        if s.get('operand_type') in ('f32', 'i8', 'i32', 'ptr_f32'):
            add_tail(s['va'], s['orig'], s['operand_offset'], s['operand_type'], s['new'], 't60_monsters', s.get('const_va'))
        else:
            hooks.append('monsters (unhandled) %s' % json.dumps(s)[:120])

# ---- misc
X = json.load(open(os.path.join(S, r'research\framerate\v5\true60_misc.json')))
for x in X['sites']:
    k, area = x['kind'], 't60_' + x['area']
    if k == 'none':
        continue
    if x.get('confidence') == 'low':
        skipped.append('%s low confidence (%s)' % (x['va'], area)); continue
    if area == 't60_player_clip_time' and k == 'hook':
        ob = hb(x['orig'])
        assert ob[0] == 0xE8 and exe_bytes(int(x['va'], 16), 5) == ob
        tgt = int(x['va'], 16) + 5 + struct.unpack('<i', ob[1:5])[0]
        assert tgt == 0x55AE60, hex(tgt)
        clip.append((int(x['va'], 16), ob)); continue
    if k == 'explicit':
        news = {x['new'][r] for r in ('60', '120', '180')}
        assert len(news) == 1, x
        ob = hb(x['orig']); assert exe_bytes(int(x['va'], 16), len(ob)) == ob
        raw.append((int(x['va'], 16), ob, hb(news.pop()), x['area'])); continue
    if k == 'hook':
        hooks.append('misc %s %s' % (x['va'], (x.get('what') or '')[:90])); continue
    add_tail(x['va'], x['orig'], x['operand_offset'], x['operand_type'], x['new'], area, x.get('const_va'))

# ---- companions (optional; same row shape as misc or monsters' fix)
cp = os.path.join(S, r'research\framerate\v5\true60_companions.json')
if os.path.exists(cp) and '--no-companions' not in sys.argv:
    C = json.load(open(cp))
    items = C if isinstance(C, list) else (C.get('sites') or C.get('rows') or [])
    OT = {'imm_f32': 'f32', 'f32': 'f32', 'i8': 'i8', 'i16': 'i16', 'i32': 'i32', 'ptr_f32': 'ptr_f32'}
    for x in items:
        k = x.get('kind', 'none')
        if k in ('none', 'leave') or not x.get('va'):
            continue
        ot = OT.get(x.get('operand_type'))
        if k == 'hook' or not ot or not isinstance(x.get('new'), dict):
            hooks.append('%s %s %s %s' % (x.get('area'), x['va'], k, (x.get('what') or '')[:90])); continue
        if x.get('confidence') == 'low':
            skipped.append('%s low confidence (%s)' % (x['va'], x.get('area'))); continue
        new = dict(x['new'])
        if '180' not in new:  # the companion audit gives 60 / 120: 180 by the same rule
            b, v60 = float(x['base_value']), float(new['60'])
            if k == 'float_scale': new['180'] = b * 30 / 180
            elif k == 'tick_threshold': new['180'] = v60 * 3
            elif k == 'imm_scale_int': new['180'] = max(1, round(b / 6))
            elif k == 'float_pow':
                retain = b > 0 and abs(v60 - b ** 0.5) < 1e-6  # x *= b (also b > 1: growth) vs x += (t - x) * b
                new['180'] = b ** (1 / 6) if retain else 1 - (1 - b) ** (1 / 6)
                assert isinstance(new['180'], float), x
            else:
                hooks.append('%s %s (no 180 rule for %s)' % (x.get('area'), x['va'], k)); continue
            if ot in ('i8', 'i16', 'i32'): new['180'] = round(new['180'])
        add_tail(x['va'], x['orig'], x['operand_offset'], ot, new, 't60_' + x.get('area', 'companion'),
                 x.get('const_va'))

json.dump(tail + rows, open(os.path.join(OUT, 'fps_tail_all.json'), 'w'), indent=1)
with open(os.path.join(OUT, 'true60_fixed.h'), 'w', newline='\n') as f:
    f.write('// Generated by gen_true60.py from research/framerate/v5 (true60_misc.json): do not edit by hand.\n')
    f.write('// Rate-independent (equal to the stock behaviour at 30), installed once in FrameMode 0.\n')
    f.write('// kT60Clip: `call 0x55AE60` (1/fps) used to turn a clip frame into seconds -> call ClipFrameSeconds (1/30).\n')
    f.write('static const struct { DWORD va; BYTE orig[5]; } kT60Clip[] = {\n')
    for va, ob in clip:
        f.write('    { 0x%06X, { %s } },\n' % (va, ', '.join('0x%02X' % b for b in ob)))
    f.write('};\n')
    f.write('// kT60Bytes: byte patches (the fps == 30 branch of `fps == 30 ? 28 : round(fps * 0.9333)` clip-frame tests).\n')
    f.write('static const struct { DWORD va; int len; BYTE orig[4]; BYTE now[4]; const char *what; } kT60Bytes[] = {\n')
    for va, ob, nb, area in raw:
        f.write('    { 0x%06X, %d, { %s }, { %s }, "%s" },\n' % (va, len(ob), ', '.join('0x%02X' % b for b in ob),
                                                            ', '.join('0x%02X' % b for b in nb), area))
    f.write('};\n')
with open(os.path.join(OUT, 'true60_rows.txt'), 'w') as f:
    f.write('tail rows added: %d\n' % len(rows))
    for e in rows: f.write('  %s %s %s %s\n' % (e['va'], e['operand_type'], e['area'], e['new']))
    f.write('clip redirects: %d\nbyte patches: %d\nhooks by hand:\n' % (len(clip), len(raw)))
    for h in hooks: f.write('  ' + h + '\n')
    f.write('skipped:\n')
    for s in skipped: f.write('  ' + s + '\n')
print('tail rows', len(rows), 'clip', len(clip), 'raw', len(raw), 'hooks', len(hooks), 'skipped', len(skipped))
