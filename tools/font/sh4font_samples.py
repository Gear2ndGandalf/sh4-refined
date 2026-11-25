r"""sh4font_samples.py: SH4 Refined v3.43 FN343, side-by-side samples of the HD menu fonts.

Draws an Options page (Option 4 rows, values and a font-1 description) the way the game does: every glyph is a
24 x 32 (font 0) / 20 x 26 (font 1) quad in 640 x 480 layout units at pen x, advance = width + kerning nibbles + 2.5,
a shadow first at +2, +2, bilinear filtering. Stock = the game's own glyphs at 1 texel a unit (decoded from the user's
data\fontdata_kr.bin: LOCAL COMPARISON ONLY, these PNGs are not part of any package); HD = a .sh4font sheet at 4 texels
a unit. Panels: the page at 1440 x 1080 (what a 1080p screen shows), stock above, HD below, then one row at 3.2 px a
unit (a 2048 x 1536 screen), stock left, HD right.

usage: python -I sh4font_samples.py --game <game folder> --sheet <x.sh4font> --out <png>
"""
import argparse, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fontlib import StockFont, png_write, STOCK_ALPHA, NGLYPH  # noqa: E402

QUAD = {0: (24, 32), 1: (20, 26)}


def read_sheet(path):
    d = open(path, 'rb').read()
    if d[:4] != b'SH4F':
        raise ValueError('not a .sh4font')
    ver, scale, nf, cw, chh, cols, rows = struct.unpack_from('<7I', d, 4)
    name = d[32:64].split(b'\0')[0].decode('ascii')
    if ver != 1 or nf != 2 or cols * rows < NGLYPH:
        raise ValueError('format')
    W, H = cols * cw, rows * chh
    p = 64
    fonts = {}
    for _ in range(nf):
        fi, gw, gh, crc = struct.unpack_from('<4I', d, p)
        have = d[p + 16:p + 16 + NGLYPH]
        hl = 16 + NGLYPH
        hl += (-hl) % 16
        pix = d[p + hl:p + hl + W * H // 2]
        p += hl + W * H // 2
        fonts[fi] = {'crc': crc, 'have': have, 'pix': pix, 'W': W, 'gw': gw, 'gh': gh}
    return {'name': name, 'scale': scale, 'cw': cw, 'ch': chh, 'cols': cols, 'fonts': fonts}


class Tex:
    """A glyph source: alpha(i, tx, ty) in texels of the glyph's own cell, 0..1."""

    def __init__(self, stock=None, sheet=None, font=0):
        self.stock, self.sheet, self.font = stock, sheet, font
        self.cache = {}

    def scale(self):
        return self.sheet['scale'] if self.sheet else 1

    def cell(self, i):
        if i in self.cache:
            return self.cache[i]
        if self.sheet:
            f = self.sheet['fonts'][self.font]
            cw, ch, W = self.sheet['cw'], self.sheet['ch'], f['W']
            ox, oy = (i % self.sheet['cols']) * cw, (i // self.sheet['cols']) * ch
            rows = []
            for y in range(ch):
                r = []
                for x in range(cw):
                    j = (oy + y) * W + ox + x
                    b = f['pix'][j >> 1]
                    r.append(((b >> 4) if j & 1 else (b & 15)) / 15.0)
                rows.append(r)
        else:
            g = self.stock.glyph(i)
            rows = [[0.0] * 26 for _ in range(34)]
            for y, r in enumerate(g):
                for x, v in enumerate(r):
                    rows[y][x] = STOCK_ALPHA[v] / 255.0
        self.cache[i] = rows
        return rows

    def sample(self, i, u, v):
        """Bilinear sample at texel coordinates (u, v) (texel centres at +0.5), clamped to the cell."""
        c = self.cell(i)
        h, w = len(c), len(c[0])
        u -= 0.5
        v -= 0.5
        x0, y0 = int(u // 1), int(v // 1)
        fx, fy = u - x0, v - y0

        def at(x, y):
            if 0 <= x < w and 0 <= y < h:
                return c[y][x]
            return 0.0
        a = at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx
        b = at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx
        return a * (1 - fy) + b * fy


def draw_text(img, W, H, s, ox, oy, tex, stock, text, x, y, col, shadow=True):
    """Draw text at layout (x, y) on an RGB float image scaled s px a unit, offset (ox, oy) units."""
    qw, qh = QUAD[tex.font]
    k = tex.scale()
    for pas in ((2, 2, (0.12, 0.12, 0.12)), (0, 0, col)) if shadow else ((0, 0, col),):
        dx, dy, c = pas
        pen = x
        for ch in text:
            i = ch.encode('cp1252')[0] - 0x20
            if not (0 <= i < NGLYPH):
                continue
            gx, gy = pen + dx, y + dy
            px0, px1 = int((gx - ox) * s), int((gx + qw - ox) * s) + 1
            py0, py1 = int((gy - oy) * s), int((gy + qh - oy) * s) + 1
            for py in range(max(0, py0), min(H, py1)):
                v = ((py + 0.5) / s + oy - gy) * k
                if v < 0 or v > qh * k:
                    continue
                row = img[py]
                for px in range(max(0, px0), min(W, px1)):
                    u = ((px + 0.5) / s + ox - gx) * k
                    if u < 0 or u > qw * k:
                        continue
                    a = tex.sample(i, u, v)
                    if a <= 0:
                        continue
                    p = row[px]
                    row[px] = (p[0] + (c[0] - p[0]) * a, p[1] + (c[1] - p[1]) * a, p[2] + (c[2] - p[2]) * a)
            pen += stock.advance(i)


PAGE = [("Display Mode", "Borderless"), ("Aspect Ratio", "16:9"), ("1st Person FOV", "Stock"),
        ("3rd Person FOV", "Wide"), ("Frame Rate", "60 fps"), ("Renderer", "Direct3D 11"),
        ("Menu Font", "%s"), ("Exit", "")]
DESC = "The font of menus, memos and subtitles: Stock, or a high-resolution look-alike (SIL OFL)."


def panel(s, ox, oy, w_units, h_units, fonts, stocks, menu_name, rows=None):
    W, H = int(w_units * s), int(h_units * s)
    img = [[(0.055 + 0.03 * (y / H), 0.05 + 0.02 * (y / H), 0.05) for _ in range(W)] for y in range(H)]
    draw_text(img, W, H, s, ox, oy, fonts[0], stocks[0], 'Option 4', 40, 52, (0.85, 0.85, 0.85))
    for r, (lab, val) in enumerate(PAGE):
        if rows is not None and r not in rows:
            continue
        sel = r == 6
        col = (1.0, 1.0, 0.55) if sel else (0.82, 0.82, 0.82)
        y = 96 + 32 * r
        draw_text(img, W, H, s, ox, oy, fonts[0], stocks[0], lab, 40, y, col)
        if val:
            draw_text(img, W, H, s, ox, oy, fonts[0], stocks[0], val.replace('%s', menu_name), 364, y, col)
    if rows is None:
        draw_text(img, W, H, s, ox, oy, fonts[1], stocks[1], DESC, 40, 372, (0.78, 0.78, 0.78))
    return img, W, H


def to_bytes(img):
    out = bytearray()
    for row in img:
        for r, g, b in row:
            out += bytes((min(255, int(r * 255 + 0.5)), min(255, int(g * 255 + 0.5)), min(255, int(b * 255 + 0.5))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--game', required=True)
    ap.add_argument('--sheet', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    kr = os.path.join(a.game, 'data', 'fontdata_kr.bin')
    stocks = {e: StockFont(kr, e) for e in (0, 1)}
    sheet = read_sheet(a.sheet)
    st = {e: Tex(stock=stocks[e], font=e) for e in (0, 1)}
    hd = {e: Tex(sheet=sheet, font=e) for e in (0, 1)}
    # the 4:3 page at 1080p (2.25 px a unit), the part with text (x 20..620, y 40..410)
    s = 1080 / 480.0
    shown = sheet['name'] if len(sheet['name']) <= 14 else sheet['name'].split()[0]  # the value column's room
    p1, W, H1 = panel(s, 20, 40, 600, 370, st, stocks, shown)
    p2, _, H2 = panel(s, 20, 40, 600, 370, hd, stocks, shown)
    # one row at 2048 x 1536 (3.2 px a unit): stock left, HD right
    z = 1536 / 480.0
    zw = W // 2
    q1, QW, QH = panel(z, 30, 182, zw / z, 46, st, stocks, sheet['name'], rows=[3])
    q2, _, _ = panel(z, 30, 182, zw / z, 46, hd, stocks, sheet['name'], rows=[3])
    sep = [[(0.35, 0.35, 0.35)] * W for _ in range(6)]
    zoom = [q1[y][:QW] + [(0.35, 0.35, 0.35)] * (W - 2 * QW) + q2[y][:QW] for y in range(QH)]
    img = p1 + sep + p2 + sep + zoom
    png_write(a.out, W, len(img), to_bytes(img), 3)
    print('%s: %d x %d (stock above, %s below; the row at 2048 x 1536: stock left, HD right)' % (a.out, W, len(img), sheet['name']))


if __name__ == '__main__':
    main()
