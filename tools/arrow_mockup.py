# arrow_mockup.py FONTDATA_KR_BIN SRC_DIR OUT_DIR
#
# SH4 Refined v3.33 MENU333: before / after mockups of the Options value arrows (v3.32 QA: the right arrow of Echo
# Doubles sat on "st" of "Shadow Cast"). Positions are the game's (0x5254F0): rows at x 40, y 96 + 32 * i, the value
# left-aligned at x 364, the left arrow (8 x 24 triangle, 0x523620) at x - 20, the right arrow at x + 100 + the shift;
# a value wider than 130 px moves the column LongMove(w) left (LongValW). Every glyph sits at the game's own advance
# (font 0 metrics read at run time from a read-only copy of data\fontdata_kr.bin, LabelW's formula; nothing of it is
# stored) and is drawn with Georgia: the letter shapes are not the game's (no Konami art), the widths are. Procedural
# backdrop. The shifts come from tools\menucheck_values.py (the same rules as src\arrowfit_sim.h).
# Sheets: renders\arrows_gameplay_special.png (the page of the user's screenshot, 4:3 / 16:9 Menus43 / 16:9 stretched,
# before / after) and renders\arrows_all_fixed_rows.png (every row the check found under the arrow, before / after).
import os, sys, json, subprocess
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import menucheck_values as MC

GEORGIA = r'C:\Windows\Fonts\georgia.ttf'
RED, WHITE, SHADOW, ARROW = (200, 70, 70), (225, 225, 225), (0, 0, 0), (235, 235, 235)


def advance(font, ch, sp=2.5):
    c = ord(ch)
    b = 0 if c == 0x20 else 0x100 - c
    if b == 0:
        return (font[0x10] + sp) / 1.25
    g = 0xE0 - b
    k = font[0xF0 + g]
    lo, hi = k & 15, k >> 4
    if lo >= 8: lo -= 16
    if hi >= 8: hi -= 16
    return (font[0x10 + g] + lo + hi + sp) / 1.25


def backdrop(w, h, seed=0):
    im = Image.new('RGB', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            v = 14 + int(22 * max(0.0, 1 - abs(x / w - 0.5) * 2.2) * (1 - abs(y / h - 0.4))) + ((x * 7 + y * 13 + seed) % 5)
            px[x, y] = (v + 8, v + 2, v)
    return im


class Canvas:
    """the 512 x 448 canvas mapped onto a picture rect (x0, y0, sx, sy): sx != sy = stretched"""
    def __init__(self, im, x0, y0, sx, sy, glyph):
        self.d, self.x0, self.y0, self.sx, self.sy, self.font = ImageDraw.Draw(im), x0, y0, sx, sy, glyph
        self.pil = ImageFont.truetype(GEORGIA, int(19 * sy))
        self.im = im

    def p(self, x, y): return self.x0 + x * self.sx, self.y0 + y * self.sy

    def text(self, s, x, y, col):
        for dx, dy, c in ((2, 2, SHADOW), (0, 0, col)):
            cx = x + dx
            for ch in s:
                if ch != ' ':
                    if self.sx != self.sy:  # stretched: draw the glyph into a temp and scale it horizontally
                        g = Image.new('RGBA', (int(40 * self.sy), int(34 * self.sy)), (0, 0, 0, 0))
                        ImageDraw.Draw(g).text((0, 0), ch, font=self.pil, fill=c)
                        g = g.resize((max(1, int(g.width * self.sx / self.sy)), g.height))
                        X, Y = self.p(cx, y + dy)
                        self.im.paste(g, (int(X), int(Y)), g)
                    else:
                        self.d.text(self.p(cx, y + dy), ch, font=self.pil, fill=c)
                cx += advance(self.font, ch)

    def arrow(self, x, y, right):
        pts = [(x, y), (x + 8, y + 12), (x, y + 24)] if right else [(x, y + 12), (x + 8, y), (x + 8, y + 24)]
        self.d.polygon([self.p(*q) for q in pts], fill=ARROW)

    def frame(self, col=(60, 60, 60)):
        a, b = self.p(0, 0), self.p(512, 448)
        self.d.rectangle([a, b], outline=col)


def width(font, s): return sum(advance(font, c) for c in s)


def draw_page(cv, title, rows, sel, layout):
    """rows: (label, value); layout(row index, value) -> (value x, arrow shift)"""
    cv.text(title, 140, 30, (150, 215, 160))
    cv.arrow(10, 30, False)
    cv.arrow(494, 30, True)
    for i, (lab, val) in enumerate(rows):
        y = 96 + 32 * i
        col = RED if i == sel else WHITE
        cv.text(lab, 40, y, col)
        if val is None: continue
        vx, sh = layout(i, val)
        cv.text(val, vx, y, col)
        if i == sel:
            cv.arrow(vx - 20, y + 4, False)
            cv.arrow(vx + 100 + sh, y + 4, True)


def main():
    fontfile, src, out = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(out, exist_ok=True)
    font = MC.font_entry(fontfile, 0)
    cap = ImageFont.truetype(r'C:\Windows\Fonts\arial.ttf', 16)
    capb = ImageFont.truetype(r'C:\Windows\Fonts\arialbd.ttf', 24)

    def old_layout(rowshift):
        return lambda i, v: (364, rowshift.get(i, 0))

    def new_layout(rowvals, oldshift):
        def f(i, v):
            ws = [width(font, x) for x in rowvals.get(i, [v])]
            w = width(font, v)
            if w > MC.AF_COLW:
                move = max(int(w) - 130, 64)
                return 364 - move, int(w) - 100 + 6
            return 364, MC.row_shift_new(ws, oldshift.get(i, 0))
        return f

    # sheet 1: the user's page, three screen shapes, before / after; Echo Doubles on Shadow Cast and on Shadow Copy
    special_vals = {2: ['Off', 'Shadow Copy', 'Shadow Cast']}
    shapes = [('4:3', 1024, 768, 'fit'), ('16:9 Menus43 (user)', 1366, 768, 'fit'),
              ('16:9 stretched', 1366, 768, 'stretch')]
    panels = []
    for echo in ('Shadow Cast', 'Shadow Copy'):
        rows = [('Death Presence', 'Off'), ('After Shock', 'Long'), ('Echo Doubles', echo), ('Echo Count', '3'),
                ('No Clip', 'On'), ('Back', None)]
        for name, W, H, mode in shapes:
            for tag, lay in (('v3.32', old_layout({})), ('v3.33', new_layout(special_vals, {}))):
                im = backdrop(W, H, len(panels))
                if mode == 'fit':
                    sy = H / 448.0
                    sx = sy * (4 / 3) / (512 / 448.0)
                    x0 = (W - 512 * sx) / 2
                else:
                    sx, sy, x0 = W / 512.0, H / 448.0, 0
                cv = Canvas(im, x0, 0, sx, sy, font)
                draw_page(cv, 'Gameplay: Special', rows, 2, lay)
                im = im.resize((int(W * 0.5), int(H * 0.5)), Image.LANCZOS)
                panels.append(('%s  %s  Echo = %s' % (tag, name, echo), im))
    colw = max(p[1].width for p in panels)
    rowh = max(p[1].height for p in panels) + 40
    sheet = Image.new('RGB', (colw * 2 + 30, 70 + rowh * (len(panels) // 2)), (24, 24, 28))
    d = ImageDraw.Draw(sheet)
    d.text((10, 10), 'Options value arrows: Option 5 > Gameplay: Special (left v3.32, right v3.33). Glyph advances = the '
                     'game\'s font 0 metrics; letter shapes Georgia.', font=capb, fill=(240, 240, 240))
    for k, (title, im) in enumerate(panels):
        x = 10 + (k % 2) * (colw + 10)
        y = 60 + (k // 2) * rowh
        d.text((x, y), title, font=cap, fill=(255, 210, 120) if title.startswith('v3.32') else (150, 230, 150))
        sheet.paste(im, (x, y + 30))
    p1 = os.path.join(out, 'arrows_gameplay_special.png')
    sheet.save(p1)

    # sheet 2: every row the check found under the arrow, selected, at 4:3 (canvas px x 1.5), before / after
    cases = [('Gameplay: Special', 'Echo Doubles', ['Off', 'Shadow Copy', 'Shadow Cast'], 0),
             ('Gameplay: Player', 'Curse Meter', ['Off', 'On', 'Companion', 'Both'], 0),
             ('Gameplay: Player', 'Enemy Health Bars', ['Off', 'Locked Target', 'All Near'], 0),
             ('Subtitle Style', 'Edge', ['Shadow', 'None', 'Outline', 'Outline+Shadow'], 36),
             ('Gameplay: Enemies', 'Supply Drops', ['Off', 'Normal', 'Generous'], 0),
             ('Screen Effects', 'Filter', ['Night Vision', 'Spirit Vision', 'Night Vision Mono'], 36)]
    S = 1.5
    tiles = []
    for page, lab, vals, old in cases:
        for v in vals:
            if width(font, v) <= 60: continue
            pair = []
            for tag in ('v3.32', 'v3.33'):
                im = backdrop(int(512 * S), int(56 * S), len(tiles))
                cv = Canvas(im, 0, -80 * S, S, S, font)
                w = width(font, v)
                if tag == 'v3.32':
                    if lab in ('Filter',) and len(v) > 14:
                        vx, sh = 364 - max(int(w) - 130, 64), int(w) - 94
                    else:
                        vx, sh = 364, old
                else:
                    vx, sh = new_layout({0: vals}, {0: old})(0, v)
                draw_page(cv, '', [(lab, v)], 0, lambda i, val: (vx, sh))
                over = MC.overlaps(w, sh) if vx == 364 else False
                pair.append((tag, im, over, sh, vx))
            tiles.append((page, lab, v, w, pair))
    th = int(56 * S) + 34
    sheet = Image.new('RGB', (int(512 * S) * 2 + 30, 60 + th * len(tiles)), (24, 24, 28))
    d = ImageDraw.Draw(sheet)
    d.text((10, 10), 'Every value the check found under the right arrow (selected row, 4:3, canvas x 1.5): v3.32 | v3.33',
           font=capb, fill=(240, 240, 240))
    for k, (page, lab, v, w, pair) in enumerate(tiles):
        y = 50 + k * th
        for j, (tag, im, over, sh, vx) in enumerate(pair):
            x = 10 + j * (int(512 * S) + 10)
            txt = '%s  %s > %s = "%s" (%.0f px): %s' % (tag, page, lab, v, w,
                  ('OVERLAPS the arrow' if over else 'clear') + ('' if vx == 364 else ', column moved %d px' % (364 - vx))
                  + ('' if vx != 364 else ', arrow %+d' % sh))
            d.text((x, y), txt, font=cap, fill=(255, 120, 110) if over else (150, 230, 150))
            sheet.paste(im, (x, y + 28))
    p2 = os.path.join(out, 'arrows_all_fixed_rows.png')
    sheet.save(p2)
    print(p1)
    print(p2)


if __name__ == '__main__':
    main()
