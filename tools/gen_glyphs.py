# gen_glyphs.py PACK_DIR OUT_H [PREVIEW_DIR]
#
# SH4 Refined v3.11.0: bakes the Button Config pad glyphs (staging/button_glyphs/PLAN.md) from Kenney's "Input
# Prompts" 1.5A (CC0, www.kenney.nl). PACK_DIR = the unpacked pack (the folders "Xbox Series", "PlayStation Series",
# "Nintendo Switch" with their "Default" 64 x 64 PNGs). Writes OUT_H (src/glyphs_data.h): per family the bottom half
# (y 128..255) of the game's 256 x 256 DXT3 icon atlas, mip levels 0..3, as DXT3 blocks in row order. Cell k (0..31)
# lies at x = 32 (k % 8), y = 128 + 32 (k / 8) of level 0; icon number nn = 0x20 + k.
# Pure Python (zlib only), deterministic. PREVIEW_DIR (optional) gets decoded previews for a visual check.
import os, struct, sys, zlib

# cell order (buttoncfg.inc relies on it): 0-3 D-pad Up / Right / Down / Left, 4-7 LS Up / Down / Left / Right,
# 8-11 RS Up / Down / Left / Right, 12-23 virtual buttons 0..11 (A B X Y LB RB LT RT View Menu LS-click RS-click
# positions), 24.. family extras.
FAMILIES = [
    ('XBOX', 'Xbox Series', [
        'xbox_dpad_up_outline', 'xbox_dpad_right_outline', 'xbox_dpad_down_outline', 'xbox_dpad_left_outline',
        'xbox_stick_l_up', 'xbox_stick_l_down', 'xbox_stick_l_left', 'xbox_stick_l_right',
        'xbox_stick_r_up', 'xbox_stick_r_down', 'xbox_stick_r_left', 'xbox_stick_r_right',
        'xbox_button_a', 'xbox_button_b', 'xbox_button_x', 'xbox_button_y', 'xbox_lb', 'xbox_rb', 'xbox_lt', 'xbox_rt',
        'xbox_button_view', 'xbox_button_menu', 'xbox_ls', 'xbox_rs',
    ]),
    ('PS', 'PlayStation Series', [
        'playstation_dpad_up_outline', 'playstation_dpad_right_outline', 'playstation_dpad_down_outline',
        'playstation_dpad_left_outline',
        'playstation_stick_l_up', 'playstation_stick_l_down', 'playstation_stick_l_left', 'playstation_stick_l_right',
        'playstation_stick_r_up', 'playstation_stick_r_down', 'playstation_stick_r_left', 'playstation_stick_r_right',
        'playstation_button_cross', 'playstation_button_circle', 'playstation_button_square',
        'playstation_button_triangle', 'playstation_trigger_l1', 'playstation_trigger_r1', 'playstation_trigger_l2',
        'playstation_trigger_r2',
        # v3.25.0 (staging/feedback_v325/PLAN.md 2): Kenney's PS meta-button icons are printed words or an anonymous
        # pill at cell size, so the DualSense Create / Options cells are composed in the PS face-disc style (see compose);
        # buttoncfg.inc shows them for the DualSense only (DS4 / DS3 keep their text)
        '@ps_create', '@ps_options',
        'playstation_button_l3', 'playstation_button_r3',
    ]),
    ('NINTENDO', 'Nintendo Switch', [
        'switch_dpad_up_outline', 'switch_dpad_right_outline', 'switch_dpad_down_outline', 'switch_dpad_left_outline',
        'switch_stick_l_up', 'switch_stick_l_down', 'switch_stick_l_left', 'switch_stick_l_right',
        'switch_stick_r_up', 'switch_stick_r_down', 'switch_stick_r_left', 'switch_stick_r_right',
        # positional: the virtual pad's A / B / X / Y places are Nintendo's B / A / Y / X
        'switch_button_b', 'switch_button_a', 'switch_button_y', 'switch_button_x', 'switch_button_l',
        'switch_button_r', 'switch_button_zl', 'switch_button_zr', 'switch_button_minus', 'switch_button_plus',
        'switch_stick_l_press', 'switch_stick_r_press',
    ]),
]
CELLS = 32
ART, ART_X, ART_Y = 24, 4, 6  # the glyph is drawn 24 x 24 texels inside its 32 x 32 cell at (4, 6): the game draws a
                              # cell 28 canvas units tall but Button Config rows are ~24 apart (live test), and a cell
                              # sits ~3 units above the text centre, so the art is smaller and 2 texels lower
W, H = 256, 128  # the bottom half of level 0
LEVELS = 4


def png_read(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', path
    p, idat, plte, trns = 8, b'', None, None
    while p < len(d):
        n, t = struct.unpack('>I4s', d[p:p + 8])
        c = d[p + 8:p + 8 + n]
        p += 12 + n
        if t == b'IHDR':
            w, h, bd, ct, cm, fm, il = struct.unpack('>IIBBBBB', c)
            assert il == 0 and (bd == 8 or (ct in (0, 3) and bd in (1, 2, 4))), (path, bd, il)
        elif t == b'PLTE':
            plte = c
        elif t == b'tRNS':
            trns = c
        elif t == b'IDAT':
            idat += c
        elif t == b'IEND':
            break
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    raw = zlib.decompress(idat)
    st = (w * ch * bd + 7) // 8  # bytes per row; the filter unit is one byte when bd < 8
    out = bytearray(h * st)
    prev = bytearray(st)
    if bd < 8: ch = 1
    q = 0
    for y in range(h):
        f = raw[q]
        line = bytearray(raw[q + 1:q + 1 + st])
        q += 1 + st
        for i in range(st):
            a = line[i - ch] if i >= ch else 0
            b = prev[i]
            c = prev[i - ch] if i >= ch else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                line[i] = (line[i] + pr) & 255
        out[y * st:(y + 1) * st] = line
        prev = line
    if bd < 8:  # unpack the samples, most significant bits first
        s = bytearray(w * h)
        per = 8 // bd
        for y in range(h):
            for x in range(w):
                byte = out[y * st + x // per]
                s[y * w + x] = (byte >> (8 - bd * (x % per + 1))) & ((1 << bd) - 1)
        if ct == 0: s = bytearray(v * 255 // ((1 << bd) - 1) for v in s)
        out = s
    rgba = bytearray(w * h * 4)
    for i in range(w * h):
        if ct == 6: px = out[4 * i:4 * i + 4]
        elif ct == 2: px = out[3 * i:3 * i + 3] + b'\xff'
        elif ct == 4: px = bytes([out[2 * i]] * 3 + [out[2 * i + 1]])
        elif ct == 0: px = bytes([out[i]] * 3 + [255])
        else:
            k = out[i]
            px = plte[3 * k:3 * k + 3] + bytes([trns[k] if trns and k < len(trns) else 255])
        rgba[4 * i:4 * i + 4] = px
    return w, h, rgba


def down2(w, h, px):
    """2 x 2 box filter of premultiplied RGBA floats [r, g, b, a] (a 0..1)."""
    o = []
    for y in range(h // 2):
        for x in range(w // 2):
            s = [0.0, 0.0, 0.0, 0.0]
            for dy in (0, 1):
                for dx in (0, 1):
                    q = px[(2 * y + dy) * w + 2 * x + dx]
                    for j in range(4): s[j] += q[j]
            o.append([v / 4 for v in s])
    return w // 2, h // 2, o


def resample(w, h, px, n):
    """Area-average (box) resample of premultiplied RGBA floats from w x h to n x n."""
    def weights(src, dst):
        out = []
        for i in range(dst):
            a, b = i * src / dst, (i + 1) * src / dst
            ws, k = [], int(a)
            while k < b and k < src:
                ov = min(b, k + 1) - max(a, k)
                if ov > 1e-9: ws.append((k, ov / (b - a)))
                k += 1
            out.append(ws)
        return out
    wx, wy = weights(w, n), weights(h, n)
    o = []
    for y in range(n):
        for x in range(n):
            s = [0.0, 0.0, 0.0, 0.0]
            for sy, fy in wy[y]:
                for sx, fx in wx[x]:
                    q = px[sy * w + sx]
                    f = fx * fy
                    for j in range(4): s[j] += q[j] * f
            o.append(s)
    return o


def to565(r, g, b):
    return (round(r * 31 / 255) << 11) | (round(g * 63 / 255) << 5) | round(b * 31 / 255)


def from565(c):
    return ((c >> 11 & 31) * 255 / 31, (c >> 5 & 63) * 255 / 63, (c & 31) * 255 / 31)


def dxt3(w, h, px):
    """px: premultiplied floats. Returns DXT3 blocks, row order. Colour = unpremultiplied; transparent texels white."""
    out = bytearray()
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            al, cols = 0, []
            for i in range(16):
                r, g, b, a = px[(by + i // 4) * w + bx + i % 4]
                q = max(0, min(15, int(a * 15 + 0.5)))
                al |= q << (4 * i)
                cols.append((r / a, g / a, b / a) if a > 1e-6 else None)
            seen = [c for c in cols if c is not None] or [(255.0, 255.0, 255.0)]
            # endpoints: the two texels farthest apart (fine for these two-tone icons)
            best = (-1, seen[0], seen[0])
            for i in range(len(seen)):
                for j in range(i, len(seen)):
                    dd = sum((seen[i][k] - seen[j][k]) ** 2 for k in range(3))
                    if dd > best[0]: best = (dd, seen[i], seen[j])
            c0, c1 = to565(*best[1]), to565(*best[2])
            if c0 < c1: c0, c1 = c1, c0
            if c0 == c1:
                idx = 0
            else:
                p0, p1 = from565(c0), from565(c1)
                pal = [p0, p1, tuple((2 * a + b) / 3 for a, b in zip(p0, p1)), tuple((a + 2 * b) / 3 for a, b in zip(p0, p1))]
                idx = 0
                for i, c in enumerate(cols):
                    if c is None: c = (255.0, 255.0, 255.0)
                    k = min(range(4), key=lambda m: sum((c[t] - pal[m][t]) ** 2 for t in range(3)))
                    idx |= k << (2 * i)
            out += struct.pack('<QHHI', al, c0, c1, idx)
    return bytes(out)


def dxt3_decode(w, h, blk, bg):
    rgba = bytearray(w * h * 4)
    p = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            al, c0, c1, idx = struct.unpack_from('<QHHI', blk, p)
            p += 16
            p0, p1 = from565(c0), from565(c1)
            pal = [p0, p1, tuple((2 * a + b) / 3 for a, b in zip(p0, p1)), tuple((a + 2 * b) / 3 for a, b in zip(p0, p1))]
            for i in range(16):
                a = (al >> (4 * i) & 15) / 15
                c = pal[idx >> (2 * i) & 3]
                k = ((by + i // 4) * w + bx + i % 4) * 4
                rgba[k:k + 4] = bytes([int(c[j] * a + bg[j] * (1 - a) + 0.5) for j in range(3)] + [255])
    return rgba


def png_write(fn, w, h, rgba, scale=1):
    if scale > 1:
        big = bytearray(w * scale * h * scale * 4)
        for y in range(h * scale):
            for x in range(w * scale):
                s = ((y // scale) * w + x // scale) * 4
                d = (y * w * scale + x) * 4
                big[d:d + 4] = rgba[s:s + 4]
        w, h, rgba = w * scale, h * scale, big
    raw = b''.join(b'\0' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))

    def ch(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(fn, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                         ch(b'IDAT', zlib.compress(raw, 9)) + ch(b'IEND', b''))


DISC_C, DISC_R = 32.0, 24.0  # Kenney's 64 x 64 face discs (cross, circle, Xbox menu) span texels 8..55 (centre 32.0
                              # in texel-edge coordinates)
# The Create mark, drawn here (Kenney's own has hair-thin rays that vanish at the 24-texel art size): a capsule with
# three bold rays above it, as capsules (x0, y0, x1, y1, radius) in 64-texel space, cut out of the disc.
CREATE_MARK = [(32.0, 33.0, 32.0, 41.0, 5.5),                  # the button
               (32.0, 21.0, 32.0, 17.0, 2.4),                  # the rays: up, and +-40 degrees about (32, 28)
               (27.50, 22.64, 24.93, 19.57, 2.4), (36.50, 22.64, 39.07, 19.57, 2.4)]


def capsule_in(px, py, c):
    x0, y0, x1, y1, r = c
    dx, dy = x1 - x0, y1 - y0
    t = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / (dx * dx + dy * dy)))
    return (px - x0 - t * dx) ** 2 + (py - y0 - t * dy) ** 2 <= r * r


def compose(pack, nm):
    """v3.25.0: the composed PS meta-button sources, 64 x 64 RGBA (white, alpha shapes)."""
    if nm == '@ps_options':  # the DualSense Options marking is three bars: Kenney's Xbox Menu disc is exactly that
        return png_read(os.path.join(pack, 'Xbox Series', 'Default', 'xbox_button_menu.png'))
    assert nm == '@ps_create', nm
    rgba, ss = bytearray(64 * 64 * 4), 4
    for y in range(64):
        for x in range(64):
            cov = 0
            for sy in range(ss):
                for sx in range(ss):
                    px, py = x + (sx + 0.5) / ss, y + (sy + 0.5) / ss
                    if (px - DISC_C) ** 2 + (py - DISC_C) ** 2 > DISC_R ** 2: continue
                    if any(capsule_in(px, py, c) for c in CREATE_MARK): continue  # the mark is cut out
                    cov += 1
            rgba[4 * (y * 64 + x):4 * (y * 64 + x) + 4] = bytes([255, 255, 255, (cov * 255 + 8) // (ss * ss)])
    return 64, 64, rgba


def bake(pack, folder, names):
    sheet = [[0.0, 0.0, 0.0, 0.0] for _ in range(W * H)]
    for k, nm in enumerate(names):
        assert k < CELLS
        if nm is None: continue  # no glyph: the cell stays transparent, buttoncfg.inc keeps the name
        if nm.startswith('@'): w, h, rgba = compose(pack, nm)
        else: w, h, rgba = png_read(os.path.join(pack, folder, 'Default', nm + '.png'))
        assert (w, h) == (64, 64), (nm, w, h)
        px = []
        for i in range(w * h):
            r, g, b, a = rgba[4 * i:4 * i + 4]
            a /= 255
            px.append([r * a, g * a, b * a, a])
        px = resample(w, h, px, ART)
        ox, oy = 32 * (k % 8) + ART_X, 32 * (k // 8) + ART_Y
        for y in range(ART):
            for x in range(ART):
                sheet[(oy + y) * W + ox + x] = px[y * ART + x]
    levels, w, h, px = [], W, H, sheet
    for lv in range(LEVELS):
        levels.append((w, h, dxt3(w, h, px)))
        if lv + 1 < LEVELS:
            w, h, px = down2(w, h, px)
    return levels


def main():
    pack, out_h = sys.argv[1], sys.argv[2]
    prev = sys.argv[3] if len(sys.argv) > 3 else None
    lines = ['// Generated by tools/gen_glyphs.py from Kenney "Input Prompts" 1.5A (CC0, www.kenney.nl); the PlayStation',
             '// Create cell is drawn by the script itself (v3.25.0). Do not edit.',
             '// Per family: the bottom half (y 128..255) of the 256 x 256 DXT3 icon atlas, mip levels 0..3, block rows',
             '// in order (level L: (256 >> L) / 4 blocks per row, (128 >> L) / 4 rows). Cell k = icon 0x20 + k.',
             '#define GLYPH_LEVELS %d' % LEVELS]
    for key, folder, names in FAMILIES:
        levels = bake(pack, folder, names)
        blob = b''.join(b for _, _, b in levels)
        lines.append('#define GLYPH_%s_MASK 0x%08Xu  // bit k = cell k has a glyph' % (key, sum(1 << k for k, nm in enumerate(names) if nm)))
        lines.append('static const unsigned char kGlyph%s[%d] = {' % (key.title(), len(blob)))
        for i in range(0, len(blob), 32):
            lines.append('    ' + ','.join('0x%02X' % b for b in blob[i:i + 32]) + ',')
        lines.append('};')
        if prev:
            os.makedirs(prev, exist_ok=True)
            for lv, (w, h, b) in enumerate(levels):
                png_write(os.path.join(prev, 'glyphs_%s_L%d.png' % (key.lower(), lv)), w, h,
                          dxt3_decode(w, h, b, (40, 40, 50)), 3 if lv == 0 else 1)
    text = '\n'.join(lines) + '\n'
    with open(out_h, 'w', newline='\n') as f:
        f.write(text)
    print('wrote', out_h, len(text), 'chars')


if __name__ == '__main__':
    main()
