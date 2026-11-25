# desc_mockup.py FONTDATA_KR_BIN OUT_DIR
#
# SH4 Refined v3.32.0 DESC332: layout mockups of the Options description fit, 4:3 and 16:9 (Menus43), before (v3.31.0)
# and after. Positions are the game's: rows at x 40, y 96 + step * i (LoadSlot: step 32 / 28 / 26 for <= 8 / 9 / 10+ rows),
# values at x 364 with arrows at 344 / 464, the description at x 64, y 360 (0x526653), font 1 line pitch 26 * 448 / 480.
# Every glyph is placed at the game's own advance (font 0 for rows, font 1 for the description: widths and kerning
# read at run time from a read-only copy of data\fontdata_kr.bin; nothing of it is stored), drawn with Arial: the
# letter shapes are not the game's (no Konami art), the widths and line breaks are. The "after" layout comes from the
# DLL's own logic (descfit_sim.h through test\out\descfit_sim_test.exe --cli). Procedural backdrop.
import os, subprocess, sys
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import desc_measure as M

HERE = os.path.dirname(os.path.abspath(__file__))
CLI = os.path.join(HERE, '..', 'test', 'out', 'descfit_sim_test.exe')
S = 1.5                         # canvas -> picture scale (512 x 448 -> 768 x 672)
ARIAL = r'C:\Windows\Fonts\arial.ttf'


def adv(font, ch, spacing=2.5):
    c = ord(ch)
    if c == 0x20:
        return (font[0x10] + spacing) / 1.25, 0.0
    g = c - 0x20
    k = font[0xF0 + g]
    lo, hi = k & 15, k >> 4
    if lo >= 8: lo -= 16
    if hi >= 8: hi -= 16
    return (font[0x10 + g] + lo + hi + spacing) / 1.25, lo / 1.25


def text(draw, ox, oy, font, pil, s, x, y, colour, spacing=2.5, shadow=True):
    for pas in ((2, 2, (0, 0, 0)), (0, 0, colour)) if shadow else ((0, 0, colour),):
        cx = x + pas[0]
        for ch in s:
            a, kl = adv(font, ch, spacing)
            if ch != ' ':
                draw.text((ox + (cx + kl) * S, oy + (y + pas[1]) * S), ch, font=pil, fill=pas[2])
            cx += a


def fit(fontfile, s, rows_bottom, floor_y):
    p = subprocess.run([CLI, '--cli', fontfile, '416', str(rows_bottom), str(floor_y)], input=(s + '\n').encode('latin-1'),
                       capture_output=True)
    summ, dy, sp, txt, _ = p.stdout.decode('latin-1').rstrip('\n').split('\t')
    return summ, float(dy), float(sp), txt.split('|')


def backdrop(w, h):
    im = Image.new('RGB', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            v = 18 + int(14 * (1 - abs(y / h - 0.45))) + ((x * 7 + y * 13) % 5)
            px[x, y] = (v, v + 2, v + 6)
    return im


def page(f0, f1, fontfile, rows, cur, value, desc, after, wide, title, floor_y=444.0, preview=None):
    W = int(round(448 * S * 16 / 9)) if wide else int(512 * S)
    H = int(448 * S)
    im = backdrop(W, H)
    d = ImageDraw.Draw(im)
    ox = (W - 512 * S) / 2 if wide else 0
    pil0 = ImageFont.truetype(ARIAL, int(21 * S))
    pil1 = ImageFont.truetype(ARIAL, int(17 * S))
    if wide:  # the 4:3 area of the 2D (Menus43): a faint frame
        d.rectangle([ox, 0, ox + 512 * S - 1, H - 1], outline=(70, 70, 90))
    n = len(rows)
    step = 32 if n <= 8 else 28 if n == 9 else 26
    tw = sum(adv(f0, c)[0] for c in title)
    text(d, ox, 0, f0, pil0, title, 256 - tw / 2, 42, (230, 230, 230))
    for i, lab in enumerate(rows):
        y = 96 + step * i
        col = (200, 40, 40) if i == cur else (230, 230, 230)
        text(d, ox, 0, f0, pil0, lab, 40, y, col)
        if i == cur and value:
            vw = sum(adv(f0, c)[0] for c in value)
            text(d, ox, 0, f0, pil0, value, 404 - vw / 2, y, col)
            for ax, dirn in ((344, -1), (464, 1)):
                pts = [(ox + (ax + (0 if dirn > 0 else 8)) * S, (y + 4) * S), (ox + (ax + (8 if dirn > 0 else 0)) * S, (y + 16) * S),
                       (ox + (ax + (0 if dirn > 0 else 8)) * S, (y + 28) * S)]
                d.polygon(pts, fill=(200, 200, 200))
    if preview:
        pw = sum(adv(f0, c)[0] for c in preview)
        text(d, ox, 0, f0, pil0, preview, 256 - pw / 2, 424 - 30, (240, 220, 120))
    lh = 26 * 448 / 480
    note = ''
    if after:
        summ, dy, sp, lines = fit(fontfile, desc, 96 + step * (n - 1) + 30, floor_y)
        for k, ln in enumerate(lines):
            text(d, ox, 0, f1, pil1, ln, 64, 360 + dy + k * lh, (235, 235, 235), sp)
        note = summ
    else:
        text(d, ox, 0, f1, pil1, desc, 64, 360, (235, 235, 235))
        note = 'one line, %.1f px from x 64: ends at x %.1f' % (M.width_px(f1, desc), 64 + M.width_px(f1, desc))
    if not wide:  # the 4:3 screen ends at canvas x 512: whatever passes it is cut
        pass
    return im, note


def sheet(f0, f1, fontfile, out, name, rows, cur, value, desc, title, floor_y=444.0, preview=None, label=''):
    panels = []
    for wide in (False, True):
        for after in (False, True):
            im, note = page(f0, f1, fontfile, rows, cur, value, desc, after, wide, title, floor_y, preview)
            panels.append((im, '%s, %s: %s' % ('16:9 (Menus43)' if wide else '4:3', 'after (v3.32 DESC332)' if after else 'before (v3.31.0)', note)))
    pad, cap = 16, 30
    w43, w169 = panels[0][0].width, panels[2][0].width
    W = pad * 3 + w43 + w169
    H = pad * 3 + (panels[0][0].height + cap) * 2 + 34
    sh = Image.new('RGB', (W, H), (40, 40, 44))
    d = ImageDraw.Draw(sh)
    cf = ImageFont.truetype(ARIAL, 15)
    d.text((pad, 8), label, font=ImageFont.truetype(ARIAL, 18), fill=(255, 255, 255))
    for idx, (im, c) in enumerate(panels):
        col, row = idx // 2, idx % 2
        x = pad + (0 if col == 0 else w43 + pad)
        y = 34 + pad + row * (im.height + cap + pad)
        sh.paste(im, (x, y + cap))
        d.text((x, y + 6), c, font=cf, fill=(220, 220, 160))
    p = os.path.join(out, name)
    sh.save(p, optimize=True)
    print('wrote', p, sh.size)


def main():
    fontfile, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    f0, f1 = M.font_entry(fontfile, 0), M.font_entry(fontfile, 1)
    player = ['Movement Speed', 'Medallion Durability', 'Regeneration', 'Ammo Saver', 'Dark Pact', 'Grab Breaker', 'Quick Charge',
              'Life Steal', 'Curse Meter', 'Back']
    sheet(f0, f1, fontfile, out, 'desc43_player_curse.png', player, 8, 'Companion',
          "Companion Eileen's own curse, as the game keeps it.", 'Gameplay: Player',
          label='Option 5 > Player (Playable Eileen loaded: 10 rows), Curse Meter = Companion (461.6 px, the widest Player line)')
    enemies = ['One-Hit Kill', 'Enemy Copies', 'Enemies Follow', 'Enemy Health Bars', 'Supply Drops', 'Revive Enemies', 'Back']
    sheet(f0, f1, fontfile, out, 'desc43_enemies_copies.png', enemies, 1, 'Off',
          'More of every monster and ghost; limited by memory.', 'Gameplay: Enemies',
          label='Option 5 > Enemies (7 rows), Enemy Copies (465.6 px, the widest line of v3.31.0)')
    sub = ['Size', 'Colour', 'Edge', 'Box', 'Effect', 'Exit', 'Back']
    sheet(f0, f1, fontfile, out, 'desc43_subtitle_floor_test.png', sub, 0, 'Normal',
          'Test string: a future long line on the page with the subtitle preview.', 'Subtitle Style', floor_y=376.0,
          preview='This is how subtitles look.',
          label='Rule check (not a real string): on Subtitle Style a 2-line description is lifted above the preview line')


if __name__ == '__main__':
    main()
