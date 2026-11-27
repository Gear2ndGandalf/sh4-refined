r"""v3.58 MN358: the Options value box on every page of the plan, measured in the game's own font metrics.

usage: py -3 tools\valbox_check.py [FONTDATA_KR_BIN] [FONTS_DIR]
  FONTDATA_KR_BIN  a read-only copy of the game's data\fontdata_kr.bin (default: the GOG game folder's); its font 0
                   metrics are read at run time and never written anywhere (Konami data, not shipped)
  FONTS_DIR        the folder with Refined's .sh4font sheets (default: the game folder's fonts\)

For each page of staging\options_layout\tools\layout_data.py (the sandbox copy first): the widest value of its value /
combo rows and its widest label, measured exactly as sh4windowmod.c LabelW does, and the box src\valbox_sim.h VbxMake makes
(ported below): the two arrows' x on every row of the page. The HD menu fonts are drawn into the stock metrics (font.inc:
"every width, kerning pair, baseline and cap height kept"); this tool checks each sheet's stored metrics CRC against the
stock font's, so one measure holds for the stock font and every HD font. Exit 1 when a plan value does not fit its page's
box or a sheet was made for other metrics.
"""
import importlib.util, os, struct, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GAME = os.environ.get('SH4_GAME_DIR', r'C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4')

# src\valbox_sim.h
X0, ARROW_W, L0, R0, PAD, R_MAX, LABEL_X, LABEL_GAP = 364, 8, -12, 100, 6, 136, 40, 10
SPAN0 = R0 - L0


def ceil_i(w):
    i = int(w)
    return i + 1 if i < w else i


def vbx_make(widest, label):
    need = (ceil_i(widest) if widest > 0 else 0) + 2 * PAD
    span = max(need, SPAN0)
    l, r = L0, L0 + span
    lmin = ceil_i(LABEL_X + max(label, 0.0)) - X0 + LABEL_GAP + ARROW_W
    if r > R_MAX:
        r = R_MAX
        l = r - span
    if l < lmin:
        l = lmin if lmin < L0 else L0
    return dict(l=l, r=r, move=L0 - l, shift=(r - l) - SPAN0, need=need, fits=r - l >= need)


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


def metrics_crc(font):  # font_sim.h FnMetricsCrc
    return zlib.crc32(font[0xF0:0xF0 + 0xE0], zlib.crc32(font[0x10:0x10 + 0xE0], 0)) & 0xFFFFFFFF


def sheet_crcs(path):  # font_sim.h FnSheetParse: 64-byte header, then per font {index, glyphW, glyphH, crc, ...}
    b = open(path, 'rb').read()
    if b[:4] != b'SH4F':
        return None
    out, p = {}, 64
    fonthdr = ((16 + 0xE0) + 15) & ~15
    sheet = (16 * 104) * (14 * 136) // 2
    for _ in range(struct.unpack_from('<I', b, 12)[0]):
        idx, _, _, crc = struct.unpack_from('<4I', b, p)
        out[idx] = crc
        p += fonthdr + sheet
    return out


def find_layout():
    d = ROOT
    for _ in range(6):
        p = os.path.join(d, 'staging', 'options_layout', 'tools', 'layout_data.py')
        if os.path.isfile(p):
            return p
        d = os.path.dirname(d)
    raise SystemExit('layout_data.py not found above ' + ROOT)


def main():
    fd = sys.argv[1] if len(sys.argv) > 1 else os.path.join(GAME, 'data', 'fontdata_kr.bin')
    fdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(GAME, 'fonts')
    spec = importlib.util.spec_from_file_location('layout_data', find_layout())
    L = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(L)
    fonts = [font_entry(fd, 0), font_entry(fd, 1)]
    bad = 0
    print('fonts: the stock font 0 metrics crc %08X, font 1 %08X' % (metrics_crc(fonts[0]), metrics_crc(fonts[1])))
    if os.path.isdir(fdir):
        for fn in sorted(os.listdir(fdir)):
            if not fn.lower().endswith('.sh4font'):
                continue
            c = sheet_crcs(os.path.join(fdir, fn))
            ok = c is not None and c.get(0) == metrics_crc(fonts[0])
            bad += not ok
            print('  %-28s font 0 crc %s: %s' % (fn, '%08X' % c.get(0, 0) if c else '?', 'the stock metrics (the same widths)'
                                                if ok else 'OTHER METRICS (widths may differ)'))
    else:
        print('  (no fonts folder %s: the sheets not checked)' % fdir)
    font = fonts[0]
    print('%-12s %4s %7s %7s  %-16s %s' % ('page', 'rows', 'widest', 'label', 'arrows (L / R)', 'widest value'))
    for p in L.PAGES:
        wv, wl, which, rows = 0.0, 0.0, '', 0
        for r in p['rows']:
            wl = max(wl, label_w(font, r['name']))
            if r['kind'] in ('value', 'combo') and r['values']:
                rows += 1
                for v in r['values']:
                    w = label_w(font, v['label'])
                    if w > wv:
                        wv, which = w, '%s: %s' % (r['name'], v['label'])
        b = vbx_make(wv, wl)
        left, right = X0 + b['l'] - ARROW_W, X0 + b['r']
        bad += not b['fits']
        print('%-12s %4d %7.1f %7.1f  %3d / %3d %-6s %s%s' % (p['id'], rows, wv, wl, left, right,
              '(stock)' if not b['move'] and not b['shift'] else '', which, '' if b['fits'] else '  DOES NOT FIT'))
    print('%s' % ('OK: every page fits its box; one arrow x per page' if not bad else 'PROBLEMS: %d' % bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
