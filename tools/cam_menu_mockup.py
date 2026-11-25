"""cam_menu_mockup.py OUTDIR: v3.33 CAM333 layout mock-ups of Option 6 (Extra Menu) with its new Camera row, and of the
Camera page (13 rows), on the Options 512 x 448 canvas at 2x. Row positions are Refined's (LoadSlot: x 40, y 96 + step * i,
SlotRowStep: 8 rows 32, 9 rows 28, 10 rows 26, 11 rows 24, 12-13 rows 22), values at x 364, the description at y 360 or,
when the rows reach it, under the last row (descfit.inc: rows bottom = last row y + 30, gap 4). Colours: white rows, the
cursor row red, submenu labels blue (0x805820 BBGGRR), the greyed Camera row grey. Not the game's font: positions only."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
S = 2
def font(sz):
    for f in ('C:/Windows/Fonts/arialbd.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(f): return ImageFont.truetype(f, sz * S)
    return ImageFont.load_default()
F0, F1, FT = font(17), font(13), font(15)
def step(n): return 32 if n <= 8 else 28 if n == 9 else 26 if n == 10 else 24 if n == 11 else 22
def page(title, rows, cur, desc, caption):
    im = Image.new('RGB', (512 * S, 470 * S), (18, 18, 22))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 512 * S - 1, 448 * S - 1], outline=(70, 70, 80))
    d.text((40 * S, 48 * S), title, font=FT, fill=(230, 230, 230))
    n, st = len(rows), step(len(rows))
    for i, (lab, val, kind) in enumerate(rows):
        y = 96 + st * i
        col = {'w': (235, 235, 235), 'b': (0x20, 0x58, 0x80), 'g': (0x48, 0x48, 0x48)}[kind]
        if i == cur: col = {'w': (200, 30, 30), 'b': (0x06, 0x1C, 0x60), 'g': (0x60, 0x30, 0x30)}[kind]
        d.text((40 * S, y * S), lab, font=F0, fill=col)
        if val:
            vx = 364 - (64 if len(val) > 14 else 0)
            vc = (235, 235, 235) if kind != 'g' else (150, 150, 150)
            d.text((vx * S, y * S), val, font=F0, fill=vc)
            d.text(((vx - 20) * S, y * S), '<', font=F0, fill=(160, 160, 160))
            d.text(((vx + 8 + 10.4 * len(val)) * S, y * S), '>', font=F0, fill=(160, 160, 160))
    bottom = 96 + st * (n - 1) + 30
    dy = max(360, bottom + 4)
    d.text((64 * S, dy * S), desc, font=F1, fill=(220, 220, 200))
    d.line([0, 448 * S, 512 * S, 448 * S], fill=(90, 90, 90))
    d.text((8 * S, 452 * S), caption + '   [rows %d, step %d, last row y %d, description y %d]' % (n, st, 96 + st * (n - 1), dy),
           font=font(9), fill=(255, 220, 120))
    return im

em = [('Storage Chest', 'On', 'w'), ('Save Game', 'On', 'w'), ('Load Game', 'On', 'w'), ('Quick Save', 'On', 'w'),
      ('Quick Load', 'On', 'w'), ('Eileen Outfit', 'On', 'w'), ('Save Banks', 'On', 'w')]
a = page('Option 6 (Extra Menu)', em + [('Camera', 'OTS', 'b'), ('Exit', '', 'w')], 7,
         'Over the shoulder camera; Enter opens its settings.', 'A: SH4 Refined owns the camera (row blue: a submenu; Left / Right OTS / Game)')
b = page('Option 6 (Extra Menu)', em + [('Camera', 'Playable Eileen', 'g'), ('Exit', '', 'w')], 7,
         "Playable Eileen's own camera menu sets it for now.", 'B: Playable Eileen still has its camera (row grey, inert; LongValW moves the value 64 left)')
cam = [('Camera', 'OTS', 'w'), ('Distance', '600', 'w'), ('Side', 'Centre', 'w'), ('Height', '270', 'w'), ('Tilt down', '15', 'w'),
       ('Turn speed', '0.1', 'w'), ('Tilt speed', '0.1', 'w'), ('Horizontal invert', 'Off', 'w'), ('Vertical invert', 'Off', 'w'),
       ('Wall zoom', 'On', 'w'), ('Wall margin', '30', 'w'), ('Fixed cameras', 'Off', 'w'), ('Back', '', 'w')]
c = page('Extra Menu: Camera', cam, 2, 'Shoulder offset: the camera to the left or right.',
         "C: the Camera page with the user's imported values (SH4PlayableEileen.ini [Camera])")
sheet = Image.new('RGB', (a.width * 3 + 40, a.height + 20), (0, 0, 0))
for i, im in enumerate((a, b, c)): sheet.paste(im, (10 + i * (a.width + 10), 10))
os.makedirs(OUT, exist_ok=True)
p = os.path.join(OUT, 'cam_menu_mockup.png')
sheet.save(p)
print(p, sheet.size)
