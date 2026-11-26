# menucheck_values.py FONTDATA_KR_BIN SRC_DIR [--json OUT]
#
# SH4 Refined v3.33 MENU333: every value of every Options / Extra Menu row SH4 Refined draws through the game's row
# code (0x5254F0: value left-aligned at x, left arrow at x - 20, right arrow 8 px wide at x + 100), measured with the
# game's font 0 metrics exactly as sh4windowmod.c LabelW does (per glyph width [font+0x10+g] + the two signed kerning
# nibbles of [font+0xF0+g] + the letter spacing 2.5, the last spacing kept; 640-wide units -> canvas px / 1.25).
# FONTDATA_KR_BIN = a read-only copy of the game's data\fontdata_kr.bin (entries 0..3 = fonts 0..3; Konami data: read at
# run time, never written or shipped).
#
# For each row: the widest value, the right arrow's shift under the v3.32 rules (ArrowShiftOld, modelled per row below)
# and under v3.33 (AfRowShift of src\arrowfit_sim.h: the widest value that fits the column decides, at most 36; values
# wider than 130 move the column, LongValW), and whether any value runs under the arrow ("OVERLAP"). The canvas is 512
# wide at every Aspect Ratio (2D is mapped onto it), so one table holds for 4:3, 16:9 and the rest.
# Exit 1 when any value still overlaps with the v3.33 rule.
import json, os, re, struct, sys

AF_SLOT, AF_GAP, AF_MAXSHIFT = 100, 6, 36
AF_COLW = AF_SLOT - AF_GAP + AF_MAXSHIFT
LONGVAL_CHARS = 14

def font_entry(path, k):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d)[0]
    offs = list(struct.unpack_from('<%dI' % n, d, 4)) + [len(d)]
    return d[offs[k]:offs[k + 1]]

def label_w(font, s, sp=2.5):  # sh4windowmod.c LabelW on the Encode()d string (byte = 0x100 - c, space = 00)
    u = 0.0
    for ch in s:
        c = ord(ch)
        b = 0 if c == 0x20 else 0x100 - c
        if b == 0:
            u += font[0x10] + sp
        elif b <= 0xE0:
            g = 0xE0 - b
            k = font[0xF0 + g]
            lo, hi = k & 15, k >> 4
            if lo >= 8: lo -= 16
            if hi >= 8: hi -= 16
            u += font[0x10 + g] + lo + hi + sp
        else:
            u += 13 + sp
    return u / 1.25

def need(w):
    n = int(w + 0.999) + AF_GAP - AF_SLOT
    return n if n > 0 else 0

def row_shift_new(ws, old):
    s = 0
    for w in ws:
        if w <= AF_COLW: s = max(s, need(w))
    return max(old, min(s, AF_MAXSHIFT))

def shift_for(chars): return 36 if chars > 9 else 20 if chars > 8 else 0

def overlaps(w, shift): return w + 2.0 > AF_SLOT + shift

def arr1(txt, name):
    m = re.search(r'\b%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\};' % re.escape(name), txt, re.S)
    return re.findall(r'"([^"]*)"', re.sub(r'//[^\n]*', '', m.group(1))) if m else None

def arr2(txt, name):
    m = re.search(r'\b%s\s*\[[^\]]*\]\s*\[[^\]]*\]\s*=\s*\{(.*?)\};' % re.escape(name), txt, re.S)
    if not m: return None
    body = re.sub(r'//[^\n]*', '', m.group(1))
    return [[x for x in re.findall(r'"([^"]*)"', r) if x] for r in re.findall(r'\{([^{}]*)\}', body)]

def main():
    font = font_entry(sys.argv[1], 0)
    src = sys.argv[2]
    rd = lambda f: open(os.path.join(src, f), encoding='latin-1').read() if os.path.exists(os.path.join(src, f)) else ''
    sh, gp, pc, hb, nc = rd('sh4windowmod.c'), rd('gameplay.inc'), rd('playercheats.inc'), rd('hudbars.inc'), rd('noclip.inc')
    rows = []  # (page, label, values, old rule (fn of value -> shift), longval rule (fn of value -> bool))
    def add(page, label, vals, old=lambda v: 0, longval=lambda v: False):
        rows.append((page, label, [v for v in vals if v is not None], old, longval))
    dm = arr1(sh, 'kDmName')
    add('Option 4', 'Display Mode', dm, old=lambda v, m=max(shift_for(len(x)) for x in dm): m)
    rank = arr1(sh, 'kRank') + ['Display 5']
    monw = max((len(x) * 10.4 for x in rank if len(x) > LONGVAL_CHARS), default=0)  # v3.53 MERGE: OL1's short ranks (Main / Second ...): no long name
    add('Option 4', 'Monitor (by rank; LongValW at the widest)', rank,
        old=lambda v, m=max(shift_for(len(x)) for x in rank): m, longval=lambda v: True)
    add('Option 4', 'Aspect Ratio', arr1(sh, 'kAspName'))
    add('Option 4', '1st / 3rd Person FOV', arr1(sh, 'kFovName'))
    add('Option 4', 'Frame Rate', arr1(sh, 'kFpsName'))
    # v3.39 PR339: V-Sync / Anti-Aliasing / Anisotropic and Performance are gone (the sources of v3.38 and before still have
    # them); Renderer (the manifest's labels as Graphics Plus ships them, then the state values with the widest
    # recognised-only names) and Opening (opening_sim.h kOpName)
    if arr2(sh, 'kGfxName'):
        gfxl = ['VSync', 'Anti-Aliasing', 'Anisotropic']
        for i, v in enumerate(arr2(sh, 'kGfxName')): add('Option 4', gfxl[i], v)
        add('Option 4', 'Performance', ['Off', 'On'])  # v3.37 CP337 (g_itemIds)
    else:
        add('Option 4', 'Renderer (shipped manifest + states)', ['Direct3D 11', 'Direct3D 11 (16-bit)', 'Direct3D 12', 'Stock',
            'Graphics Plus: d3d11fd_rt10', 'Graphics Plus: d3d12_10bit', 'Custom', 'Unavailable'])
        add('Option 4', 'Opening', arr1(rd('opening_sim.h'), 'kOpName'))
    add('Option 2', 'Vibration', arr1(sh, 'kVibName'))
    subl = ['Size', 'Colour', 'Edge', 'Box', 'Effect', 'Exit']
    for i, v in enumerate(arr2(sh, 'kSubName')):
        o = (lambda x: 36) if i in (2, 3) else (lambda x: shift_for(len(x))) if i in (4, 5) else (lambda x: 0)
        add('Subtitle Style', subl[i], v, old=o)
    fxl = ['Noise Strength', 'Noise Type', 'Noise Colour', 'Filter', 'Cut-scenes']
    for i, v in enumerate(arr2(sh, 'kFxName')):
        if i == 0: o, lv = (lambda x: 36), (lambda x: False)
        elif i in (1, 2): o, lv = (lambda x: 36 if len(x) > 8 else 0), (lambda x: False)
        elif i == 3: o, lv = (lambda x: 60 if len(x) > 14 else 36), (lambda x: len(x) > LONGVAL_CHARS)
        else: o, lv = (lambda x: 0), (lambda x: False)
        add('Screen Effects', fxl[i], v, old=o, longval=lv)
    add('Option 5', 'Item Shine', ['Off', 'On'])
    gpl = arr1(gp, 'kGpLabel')
    gpn = arr2(gp, 'kGpName')
    # v3.37 MN337: One-Hit Kill (3) is the Player folder's top row
    pages = {0: 'Gameplay: Player', 1: 'Gameplay: Player', 2: 'Gameplay: Player', 3: 'Gameplay: Player',
             4: 'Gameplay: Special', 5: 'Gameplay: Special', 6: 'Gameplay: Special', 7: 'Gameplay: Special',
             8: 'Gameplay: Enemies', 9: 'Gameplay: Enemies', 10: 'Gameplay: Enemies', 11: 'Gameplay: Enemies'}
    for i, v in enumerate(gpn):
        lv = (lambda x: len(x) > LONGVAL_CHARS) if gpl[i] == 'Enemies Follow' else (lambda x: False)
        add(pages.get(i, 'Option 5'), gpl[i], v, longval=lv)
    bld = arr1(gp, 'kBldName')
    if bld: add('Gameplay: Player', 'Blood Colour', bld)
    pcl, pcn = arr1(pc, 'kPcLabel'), arr2(pc, 'kPcName')
    if pcl and pcn:
        for i, v in enumerate(pcn): add('Gameplay: Player', pcl[i], v)
    hbl, hbn = arr1(hb, 'kHbLabel'), arr2(hb, 'kHbShown') or arr2(hb, 'kHbName')  # v3.37 MN337: the shown names (Player)
    if hbl and hbn:  # (Curse Meter's 4th value, Both, is in the table); v3.37 MN337: Curse Meter on Option 5's top level
        for i, v in enumerate(hbn): add('Gameplay: Enemies' if i == 0 else 'Option 5', hbl[i], v)
    if nc: add('Option 5', 'No Clip', arr1(nc, 'kNcName'))  # v3.37 MN337: Option 5's top row (was Special)
    eml = arr1(sh, 'kEmLabel') or []
    for l in eml: add('Option 6 (Extra Menu)', l, ['Off', 'On'])
    add('Option 6 (Extra Menu)', 'Save Banks', ['Off', 'On'])
    cam = rd('camera.inc')  # v3.33 MERGE: W4 CAM333's rows
    if not cam:
        add('Option 6 (Extra Menu)', 'Save Bank', [str(k) for k in range(1, 10)] + ['-'])
    else:
        rowv = ['OTS', 'Game', 'Playable Eileen', 'Unavailable']
        add('Option 6 (Extra Menu)', 'Camera (was Save Bank)', rowv, old=lambda v: 36 if v == 'Unavailable' else 0,
            longval=lambda v: v == 'Playable Eileen')
        # v3.37 CP337: the Camera page left Options (its settings are the pause menu's Extra Menu > Camera page, drawn by
        # extramenu.inc pm_cam_draw: tools\pause_cam_layout.py measures that one)

    bad_old = bad_new = 0
    out = []
    print('%-22s %-34s %6s %5s %5s  %s' % ('page', 'row', 'widest', 'v332', 'v333', 'values (px) and verdict'))
    for page, label, vals, old, lv in rows:
        ws = [label_w(font, v) for v in vals]
        widest = max(ws) if ws else 0
        o_sh = [old(v) for v in vals]
        moved = [lv(v) for v in vals]
        old_ov = [v for v, w, s, m in zip(vals, ws, o_sh, moved) if not m and overlaps(w, s)]
        old_row = max(o_sh) if o_sh else 0
        new_sh = row_shift_new(ws, 0 if not o_sh else 0)
        new = []
        for v, w, s, m in zip(vals, ws, o_sh, moved):
            if m or w > AF_COLW: new.append(('moved', w))      # the column moves (LongValW): arrow 6 px after the text
            else: new.append((row_shift_new(ws, s), w))
        new_ov = [v for v, (s, w) in zip(vals, new) if s != 'moved' and overlaps(w, s)]
        bad_old += len(old_ov)
        bad_new += len(new_ov)
        verdict = ('OVERLAP v3.33: %s' % new_ov) if new_ov else ('fixed (v3.32 overlapped: %s)' % old_ov) if old_ov else 'ok'
        print('%-22s %-34s %6.1f %5d %5s  %s  -> %s' % (page[:22], label[:34], widest, old_row,
              'col' if all(s == 'moved' for s, w in new) else max([s for s, w in new if s != 'moved'] or [0]),
              ', '.join('%s %.0f' % (v, w) for v, w in zip(vals, ws)), verdict))
        out.append(dict(page=page, row=label, values=[dict(text=v, w=round(w, 1), old_shift=s, new=n[0])
                                                       for v, w, s, n in zip(vals, ws, o_sh, new)],
                        old_overlap=old_ov, new_overlap=new_ov))
    print('\nvalues under the right arrow: v3.32 rules %d, v3.33 rule %d' % (bad_old, bad_new))
    if '--json' in sys.argv:
        json.dump(out, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
    return 1 if bad_new else 0

if __name__ == '__main__':
    sys.exit(main())
