r"""sh4font_make.py: SH4 Refined v3.43 FN343, the HD menu font sheets.

Renders the game's glyph set (fonts 0 and 1, Latin-1 / cp1252 glyphs 0x20..0xFF) from an OFL TrueType font into the
layout the game's glyph cache uses, at 4x: every glyph keeps the stock width, kerning and cell (font 0 = 24 x 32,
font 1 = 20 x 26 layout units, baseline and cap height measured from the stock glyphs), so no text moves. Output:
fonts\<Name>.sh4font for the runtime loader (src\font.inc, [Textures] MenuFont = <Name>) and a preview PNG.

The stock font file (the user's own data\fontdata_kr.bin) is read for its METRICS only: widths, kerning, baseline,
cap height. The .sh4font holds a CRC32 of the width + kerning tables (the loader checks it against the live font), never
a stock glyph pixel. Font files are untrusted data: run with python -I.

usage: python -I sh4font_make.py --game "<your Silent Hill 4 folder>" --fonts <third_party\\fonts> --out <fonts>
       [--only Name] [--preview <dir>]

.sh4font (little endian; src\font.inc FnFileHdr / FnFontHdr):
  header 64 bytes: 'SH4F', version 1, scale 4, nfonts 2, cellW 104, cellH 136, cols 16, rows 14, name[32]
  per font (nfonts times): fontIndex u32, glyphW u32, glyphH u32, metricsCrc u32 (CRC32 of the 0xE0 widths + 0xE0
  kerning bytes of the font it was made for), have[0xE0] (1 = the sheet has the glyph), pad to 16, then the sheet:
  (cols * cellW) x (rows * cellH) pixels, 4-bit coverage, two pixels a byte (low nibble = even x), rows top down.
  Glyph i (Latin-1 character i + 0x20) sits in cell (i % 16, i / 16), drawn from the cell's top-left corner like the
  game's own decoder (TextDecodeGlyph 0x556BC0).
"""
import argparse, os, struct, sys, zlib, statistics

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fontlib import StockFont, TrueType, flatten, raster, png_write, NGLYPH  # noqa: E402

SCALE = 4
CELL_W, CELL_H = 26 * SCALE, 34 * SCALE       # the cache page's cell (26 x 34 texels) at 4x
COLS, ROWS = 16, 14                           # 224 glyphs
VERSION = 1

# family name (the ini value) -> source files (relative to --fonts) and per-font emboldening (HD px; 0 = none).
# Cormorant Garamond's variable font defaults to its Light master: emboldened to sit near the stock weight.
FAMILIES = {
    'EBGaramond': {'title': 'EB Garamond', 'ttf': {0: r'EBGaramond\EBGaramond[wght].ttf', 1: r'EBGaramond\EBGaramond[wght].ttf'},
                   'bold': {0: 0.0, 1: 0.35}},
    'CormorantGaramond': {'title': 'Cormorant Garamond', 'ttf': {0: r'CormorantGaramond\CormorantGaramond[wght].ttf',
                                                         1: r'CormorantGaramond\CormorantGaramond[wght].ttf'},
                  'bold': {0: 0.9, 1: 0.9}},
    'CrimsonText': {'title': 'Crimson Text', 'ttf': {0: r'CrimsonText\CrimsonText-Regular.ttf', 1: r'CrimsonText\CrimsonText-Regular.ttf'},
                    'bold': {0: 0.0, 1: 0.2}},
}
# Unicode stand-ins for characters a font lacks (soft hyphen, micro sign)
SUBST = {0xAD: [0x2D], 0xB5: [0x3BC], 0x2010: [0x2D]}


def char_of(i):
    """Glyph i of the game's font -> the Unicode character it shows (cp1252 for 0x80..0x9F, Latin-1 above)."""
    b = bytes([i + 0x20])
    try:
        return ord(b.decode('cp1252'))
    except UnicodeDecodeError:
        return None


def stock_metrics(sf):
    """Baseline row and cap height (rows) of a stock font, from 'H' and 'x'."""
    def ink_rows(c):
        g = sf.glyph(ord(c) - 0x20)
        ys = [y for y, r in enumerate(g) if any(r)]
        return min(ys), max(ys)
    ht, hb = ink_rows('H')
    return hb + 1, hb + 1 - ht      # baseline (first row below the ink), cap height


def metrics_crc(sf):
    return zlib.crc32(sf.widths + sf.kern) & 0xFFFFFFFF


def bbox(contours):
    xs = [p[0] for c in contours for p in c]
    ys = [p[1] for c in contours for p in c]
    return (min(xs), min(ys), max(xs), max(ys)) if xs else None


def render_font(tt, sf, bold):
    """224 glyph coverage maps (lists of rows, CELL sized) or None where the sheet has no glyph."""
    base, capr = stock_metrics(sf)
    cap = tt.capH or tt.upem * 0.65
    k = capr * SCALE / cap                       # HD px per font unit: cap heights match
    gh = sf.ch * SCALE
    # global horizontal factor: the family's letters against the stock ink widths (never wider than 1.0)
    ratios = []
    for c in 'ABCDEFGHKLMNOPRSTUVWXYZabcdeghknopqsuvwxyz':
        i = ord(c) - 0x20
        g = tt.cmap.get(ord(c))
        if g is None or not sf.widths[i]:
            continue
        bb = bbox(tt.contours(g))
        if bb and bb[2] > bb[0]:
            ratios.append(sf.widths[i] * SCALE / ((bb[2] - bb[0]) * k))
    cx = min(1.0, max(0.8, statistics.median(ratios))) if ratios else 1.0
    out, info = [], {'k': k, 'cx': cx, 'base': base, 'cap': capr, 'squeezed': 0, 'missing': []}
    for i in range(NGLYPH):
        w = sf.widths[i]
        u = char_of(i)
        if not w or u is None:
            out.append(None)
            continue
        g = None
        for cand in [u] + SUBST.get(u, []):
            g = tt.cmap.get(cand)
            if g is not None:
                break
        if g is None:
            info['missing'].append(i + 0x20)
            out.append(None)
            continue
        cont = tt.contours(g)
        tw = w * SCALE
        if not cont:                              # a blank glyph (space, nbsp)
            out.append([[0.0] * CELL_W for _ in range(CELL_H)])
            continue
        x0, y0, x1, y1 = bbox(cont)
        inkw = max(1e-6, x1 - x0)
        sx = k * cx
        if inkw * sx > tw - 2 * bold:            # squeeze to the stock width (emboldening grows the ink)
            sx = max(1e-6, (tw - 2 * bold)) / inkw
            info['squeezed'] += 1
        ox = (tw - inkw * sx) / 2.0 - x0 * sx
        polys = flatten(cont, sx, k, ox, base * SCALE, steps=10)
        if bold > 0:
            cov = None
            offs = [(0, 0)] + [(bold * dx, bold * dy) for dx, dy in
                               ((1, 0), (-1, 0), (0, 1), (0, -1), (0.7, 0.7), (-0.7, 0.7), (0.7, -0.7), (-0.7, -0.7))]
            for dx, dy in offs:
                c = raster([[(px + dx, py + dy) for px, py in p] for p in polys], CELL_W, gh)
                if cov is None:
                    cov = c
                else:
                    for y in range(gh):
                        a, b = cov[y], c[y]
                        for x in range(CELL_W):
                            if b[x] > a[x]:
                                a[x] = b[x]
        else:
            cov = raster(polys, CELL_W, gh)
        cov += [[0.0] * CELL_W for _ in range(CELL_H - gh)]
        for row in cov:                           # nothing right of the 24-unit quad (+1 texel of filter room)
            for x in range(min(CELL_W, (24 + 1) * SCALE), CELL_W):
                row[x] = 0.0
        out.append(cov)
    return out, info


def build(name, fam, fonts_dir, kr, out_dir, preview_dir):
    secs, infos = [], []
    for e in (0, 1):
        sf = StockFont(kr, e)
        tt = TrueType(os.path.join(fonts_dir, fam['ttf'][e]))
        glyphs, info = render_font(tt, sf, fam['bold'][e])
        info['font'] = tt.name
        infos.append(info)
        W, H = COLS * CELL_W, ROWS * CELL_H
        nib = bytearray(W * H)
        have = bytearray(NGLYPH)
        for i, cov in enumerate(glyphs):
            if cov is None:
                continue
            have[i] = 1
            ox, oy = (i % COLS) * CELL_W, (i // COLS) * CELL_H
            for y in range(CELL_H):
                row = cov[y]
                base = (oy + y) * W + ox
                for x in range(CELL_W):
                    v = row[x]
                    if v > 0:
                        nib[base + x] = min(15, int(v * 15 + 0.5))
        packed = bytes((nib[j] | (nib[j + 1] << 4)) for j in range(0, len(nib), 2))
        hdr = struct.pack('<IIII', e, sf.cw, sf.ch, metrics_crc(sf)) + bytes(have)
        hdr += b'\0' * ((-len(hdr)) % 16)
        secs.append(hdr + packed)
        if preview_dir:
            png_write(os.path.join(preview_dir, '%s_font%d_sheet.png' % (name, e)), W, H,
                      bytes(min(255, v * 17) for v in nib), 1)
    nm = fam['title'].encode('ascii')[:31]
    head = b'SH4F' + struct.pack('<IIIIIII', VERSION, SCALE, 2, CELL_W, CELL_H, COLS, ROWS) + nm + b'\0' * (32 - len(nm))
    assert len(head) == 64
    path = os.path.join(out_dir, name + '.sh4font')
    with open(path, 'wb') as f:
        f.write(head)
        for s in secs:
            f.write(s)
    # the family's licence travels with the sheet (OFL 1.1: the licence accompanies every copy)
    lic = os.path.join(fonts_dir, os.path.dirname(fam['ttf'][0]), 'OFL.txt')
    with open(lic, 'rb') as f:
        txt = f.read()
    with open(os.path.join(out_dir, name + '.OFL.txt'), 'wb') as f:
        f.write(txt)
    return path, infos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', required=True)
    ap.add_argument('--fonts', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--only')
    ap.add_argument('--preview')
    a = ap.parse_args()
    kr = os.path.join(a.game, 'data', 'fontdata_kr.bin')
    os.makedirs(a.out, exist_ok=True)
    for name, fam in FAMILIES.items():
        if a.only and name != a.only:
            continue
        path, infos = build(name, fam, a.fonts, kr, a.out, a.preview)
        print('%s -> %s (%d bytes)' % (name, path, os.path.getsize(path)))
        for e, inf in enumerate(infos):
            print('  font %d from %s: %.4f px/unit, x %.3f, baseline %d, cap %d rows, %d squeezed, missing %s'
                  % (e, inf['font'], inf['k'], inf['cx'], inf['base'], inf['cap'], inf['squeezed'],
                     [hex(c) for c in inf['missing']]))


if __name__ == '__main__':
    main()
