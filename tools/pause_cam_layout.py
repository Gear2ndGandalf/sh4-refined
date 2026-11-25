"""pause_cam_layout.py FONTDATA_KR_BIN SRC_DIR [OUT_SVG]

SH4 Refined v3.37 CP337: the layout check (and an SVG mock-up) of the pause menu's Extra Menu > Camera page
(extramenu.inc pm_cam_draw), measured with the game's font 0 metrics the way sh4windowmod.c LabelW does (per glyph width
[font+0x10+g] + the two signed kerning nibbles of [font+0xF0+g] + the letter spacing 2.5; 640-wide units -> canvas
px / 1.25). FONTDATA_KR_BIN = a read-only copy of the game's data\\fontdata_kr.bin (Konami data: read here, never
written or shipped). The pause text is drawn on the 512 x 448 canvas (Menus43 keeps it 4:3 in the middle of wide shapes,
4:3 fills the screen with it), so one check holds for every Aspect Ratio.

The page's numbers are read from extramenu.inc (PM_CAM_STEP / PM_CAM_MID / PM_CAM_HDR, pm_eo_x, the hint texts) and the
rows, values and descriptions from camera.inc (kCamRowLabel, kCamRowDesc, the value lists and ranges). Every label must
start right of x 8, every value's ">" end left of x 504, the centred lines be 480 px at most (16 .. 496), the last line end above y 440.
Exit 1 when something does not fit. Not the game's glyphs in the SVG: boxes of the measured widths, the text beside."""
import re, struct, sys

def font_entry(path, k):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d)[0]
    offs = list(struct.unpack_from('<%dI' % n, d, 4)) + [len(d)]
    return d[offs[k]:offs[k + 1]]

def label_w(font, s, sp=2.5):  # LabelW: the last spacing kept, as the code does
    u = 0.0
    for ch in s:
        c = ord(ch)
        if c == 0x20:
            u += font[0x10] + sp
            continue
        g = c - 0x20
        if g < 0 or g >= 0xE0:
            u += 13 + sp
            continue
        k = font[0xF0 + g]
        lo, hi = k & 15, k >> 4
        if lo >= 8: lo -= 16
        if hi >= 8: hi -= 16
        u += font[0x10 + g] + lo + hi + sp
    return u / 1.25

def c_strings(block):
    return [bytes(m, 'ascii').decode('unicode_escape') for m in re.findall(r'"((?:[^"\\]|\\.)*)"', block)]

def array(src, name):
    m = re.search(r'%s\s*\[[^\]]*\](?:\[[^\]]*\])?\s*=\s*\{(.*?)\};' % re.escape(name), src, re.S)
    if not m: sys.exit('no %s' % name)
    return m.group(1)

def floats(block):
    return [float(x.rstrip('f')) for x in re.findall(r'-?\d+\.\d+f|-?\d+f', block)]

def main():
    if len(sys.argv) < 3: sys.exit(__doc__)
    font = font_entry(sys.argv[1], 0)
    src = sys.argv[2]
    em = open(src + '/extramenu.inc', encoding='ascii').read()
    cam = open(src + '/camera.inc', encoding='ascii').read()
    step = int(re.search(r'#define PM_CAM_STEP (0x[0-9A-Fa-f]+)', em).group(1), 16)
    mid = int(re.search(r'#define PM_CAM_MID (0x[0-9A-Fa-f]+)', em).group(1), 16)
    hdr = int(re.search(r'#define PM_CAM_HDR (0x[0-9A-Fa-f]+)', em).group(1), 16)
    eox = int(re.search(r'pm_eo_x = (0x[0-9A-Fa-f]+)', em).group(1), 16)
    hint1 = re.search(r'pm_str_camhint, sizeof pm_str_camhint, "([^"]*)"', em).group(1)
    hint2 = re.search(r'pm_str_camhint2, sizeof pm_str_camhint2, "([^"]*)"', em).group(1)
    labels = c_strings(array(cam, 'kCamRowLabel'))
    descs = c_strings(array(cam, 'kCamRowDesc'))
    lo, hi, st = floats(array(cam, 'kCamLo')), floats(array(cam, 'kCamHi')), floats(array(cam, 'kCamStep'))
    names = re.findall(r'\{([^{}]*)\}', array(cam, 'kCamOptName'))
    rows = len(labels) - 1
    vals = []
    for r in range(rows):
        if st[r] > 0:
            n = int((hi[r] - lo[r]) / st[r] + 0.5) + 1
            v = ['%g' % (lo[r] + i * st[r]) for i in range(n)] + ['Custom', '1999.5']
        else:
            v = c_strings(names[r]) + (['Custom'] if r == 2 else [])
        vals.append(v)
    n = rows + 1
    y0 = mid - (n - 1) * step // 2
    ylast = y0 + (n - 1) * step
    bad = []
    out = []
    lab_r, lt_x, val_x = eox - 0x0A, eox - 4, eox + 0x12
    out.append('Camera page: %d rows, step %d, first row y %d, last y %d, header y %d; label right edge %d, "<" at %d, value at %d'
               % (n, step, y0, ylast, y0 - hdr, lab_r, lt_x, val_x))
    lt_w = label_w(font, '<')
    if lt_x + lt_w > val_x - 1: bad.append('"<" (%.1f px) runs into the value column' % lt_w)
    for r in range(rows):
        lw = label_w(font, labels[r])
        vw = max(label_w(font, v) for v in vals[r])
        widest = max(vals[r], key=lambda v: label_w(font, v))
        gt_end = val_x + int(vw + 0.5) + 6 + label_w(font, '>')
        out.append('  %-18s label %5.1f px (from x %5.1f)   widest value %-8s %5.1f px, ">" ends at %5.1f' %
                   (labels[r], lw, lab_r - lw, widest, vw, gt_end))
        if lab_r - lw < 8: bad.append('label %s starts at %.1f' % (labels[r], lab_r - lw))
        if gt_end > 504: bad.append('row %s: ">" ends at %.1f' % (labels[r], gt_end))
    lines = [(d, ylast + 0x22) for d in descs] + [(hint1, ylast + 0x22 + 0x18), (hint2, ylast + 0x22 + 0x30)]
    for t, y in lines:
        w = label_w(font, t)
        out.append('  centred %5.1f px (%5.1f .. %5.1f) at y %d: %s' % (w, 256 - w / 2, 256 + w / 2, y, t))
        if w > 480: bad.append('line too wide (%.1f px): %s' % (w, t))
    if ylast + 0x22 + 0x30 + 26 > 440: bad.append('the last line ends at %d' % (ylast + 0x22 + 0x30 + 26))
    print('\n'.join(out))
    if len(sys.argv) > 3:
        S = 2
        svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 512 448">' % (512 * S, 448 * S),
               '<rect width="512" height="448" fill="#1a1a20"/>',
               '<style>text{font-family:Arial;font-size:15px} .s{font-size:12px}</style>']
        def txt(x, y, t, col, anchor='start', cls=''):
            svg.append('<text x="%.1f" y="%.1f" fill="%s" text-anchor="%s" class="%s">%s</text>' %
                       (x, y + 15, col, anchor, cls, t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')))
        txt(256, y0 - hdr, 'Camera', '#c0c0c0', 'middle')
        sample = ['OTS', '800', 'Centre', '330', '17', '0.1', '0.1', 'Off', 'Off', 'On', '50', 'Off']
        for i in range(n):
            y = y0 + i * step
            col = '#ffffff' if i == 1 else '#808080'
            if i == rows:
                txt(256, y, labels[rows], col, 'middle')
                continue
            txt(lab_r, y, labels[i], col, 'end')
            txt(val_x, y, sample[i], col)
            if i == 1:
                txt(lt_x, y, '<', '#c0c0c0')
                txt(val_x + label_w(font, sample[i]) + 6, y, '>', '#c0c0c0')
        txt(256, ylast + 0x22, descs[1], '#a0a0a0', 'middle', 's')
        txt(256, ylast + 0x22 + 0x18, hint1, '#707070', 'middle', 's')
        txt(256, ylast + 0x22 + 0x30, hint2, '#707070', 'middle', 's')
        svg.append('</svg>')
        open(sys.argv[3], 'w', encoding='ascii').write('\n'.join(svg) + '\n')
        print('wrote', sys.argv[3])
    if bad:
        print('DOES NOT FIT:\n  ' + '\n  '.join(bad))
        sys.exit(1)
    print('fits')

main()
