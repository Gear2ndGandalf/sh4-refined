# desc_measure.py FONTDATA_KR_BIN SRC_DIR [MAX_W]
#
# SH4 Refined v3.32.0 DESC332: measures every Options description string SH4 Refined adds with the game's own
# glyph metrics (font 1, the description font of 0x524160), the way the layout 0x557660 does: per glyph
# width [font + 0x10 + g] + the two signed kerning nibbles of [font + 0xF0 + g] + the letter spacing (2.5),
# space = [font + 0x10] + spacing, the last spacing taken off, 640-wide units -> canvas px / 1.25 (KB 117).
# FONTDATA_KR_BIN = a read-only copy of the game's data\fontdata_kr.bin (file 0x1E1, entries 0..3 = fonts 0..3);
# its metrics are read at run time and never written anywhere (Konami data, not shipped).
# Prints one line per string: width (canvas px), the fit the DLL would choose (descfit_sim.h, built as
# test\out\descfit_sim_test.exe --cli, if built), and the text. Strings are found by their place in the sources (see FIND).
import os, re, struct, subprocess, sys

def font_entry(path, k):
    d = open(path, 'rb').read()
    n = struct.unpack_from('<I', d)[0]
    offs = list(struct.unpack_from('<%dI' % n, d, 4)) + [len(d)]
    return d[offs[k]:offs[k + 1]]

def width_px(font, s, spacing=2.5):
    u = 0.0
    for ch in s:
        c = ord(ch)
        if c == 0x20:
            u += font[0x10] + spacing
            continue
        g = c - 0x20
        if g < 0 or g >= 0xE0:
            u += 13 + spacing
            continue
        k = font[0xF0 + g]
        lo, hi = k & 15, k >> 4
        if lo >= 8: lo -= 16
        if hi >= 8: hi -= 16
        u += font[0x10 + g] + lo + hi + spacing
    if s:
        u -= spacing
    return u / 1.25

LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
# arrays of description lines, and Encode() targets that are descriptions
DESC_ARRAYS = re.compile(r'\b(k\w*Desc\w*|kDescTxt|kPcValDesc|kHbDescTxt|kEmDesc)\s*\[')
DESC_TARGET = re.compile(r'Encode\(\s*(g_txt\w*Desc\w*|g_txt\w*D\b|g_txt\w*D\[|g_txt\w*RowD|g_txtSbD\[|g_txtGp\[GPM_(?:D\w*|\w*D\d|DPNA|DRNA)\b)')

def find(src):
    out = []
    for fn in sorted(os.listdir(src)):
        if not fn.endswith(('.c', '.inc')):
            continue
        lines = open(os.path.join(src, fn), encoding='utf-8', errors='replace').read().split('\n')
        in_arr = 0
        for i, line in enumerate(lines):
            code = line.split('//')[0] if '"' not in line.split('//')[0] else line
            if DESC_ARRAYS.search(line) and '=' in line and '{' in line:
                in_arr = 1
            if in_arr or DESC_TARGET.search(line):
                for m in LIT.finditer(code.split('  //')[0]):
                    t = m.group(1).replace('\\"', '"').replace("\\'", "'")
                    if len(t) >= 12 and ' ' in t and '%' not in t:
                        out.append((fn, i + 1, t))
            if in_arr and ';' in LIT.sub('\'\'', line).split('//')[0]:
                in_arr = 0
    # sprintf'd lines: "Makes blood colour %s." for the Blood Colour values 3.. (gameplay.inc kBldName)
    gp = open(os.path.join(src, 'gameplay.inc'), encoding='utf-8', errors='replace').read()
    m = re.search(r'kBldName\[[^\]]*\]\s*=\s*\{([^}]*)\}', gp)
    if m:
        names = LIT.findall(m.group(1))
        for nm in names[3:]:
            out.append(('gameplay.inc', 0, 'Makes blood colour %s.' % nm.lower()))
    seen, uniq = set(), []
    for f, l, t in out:
        if t not in seen:
            seen.add(t)
            uniq.append((f, l, t))
    return uniq

def main():
    fontfile, src = sys.argv[1], sys.argv[2]
    maxw = float(sys.argv[3]) if len(sys.argv) > 3 else 420.0
    f1 = font_entry(fontfile, 1)
    cli = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'test', 'out', 'descfit_sim_test.exe')
    rows = []
    for fn, ln, t in find(src):
        rows.append((width_px(f1, t), fn, ln, t))
    rows.sort(key=lambda r: -r[0])
    fits = {}
    if os.path.exists(cli):
        inp = '\n'.join(r[3] for r in rows) + '\n'
        p = subprocess.run([cli, '--cli', fontfile, str(maxw)], input=inp.encode('latin-1'), capture_output=True)
        for line in p.stdout.decode('latin-1').splitlines():
            parts = line.split('\t')
            if len(parts) >= 2:
                fits[parts[-1]] = parts[0]
    print('max width %.0f canvas px (x 64 .. %.0f); %d description strings' % (maxw, 64 + maxw, len(rows)))
    for w, fn, ln, t in rows:
        flag = 'OVER' if w > maxw else '    '
        print('%s %6.1f  %-18s %-36s %s' % (flag, w, '%s:%d' % (fn, ln), fits.get(t, ''), t))

if __name__ == '__main__':
    main()
