"""v3.58 FI358: an SVG sheet of Reveal Method's bigger tar portal (fadein_sim.h FiBigMake), for a look before the game.

Reads the layouts that test\\fadein_sim_test.c prints with --dump (four arrivals: a victim on a wall and in the air, Andrew by
his type's box, a crouching body) and draws, per arrival, in the portal's plane (u across, v down, world units):
the target (dashed), every piece as the ellipse the game's grid / Refined's disc covers (the first in dark red, the ones
around it in near black, numbered in the order they open), and the body's box (blue). No game art: outlines only.

Usage:  test\\out\\fadein_sim_test.exe --dump > layouts.txt
        python tools\\portal_sheet.py layouts.txt portal_sheet.svg
"""
import sys


def read(path):
    cases = []
    with open(path, encoding='ascii') as f:
        for line in f:
            w = line.split()
            if not w:
                continue
            if w[0] == 'CASE':
                v = [float(x) for x in w[2:11]]
                cases.append({'name': w[1].replace('_', ' '), 'sx': v[0], 'sy': v[1], 'tx': v[2], 'ty': v[3], 'du': v[4],
                              'dv': v[5], 'top': v[6], 'bottom': v[7], 'half': v[8], 'sets': int(w[11]), 'pieces': []})
            elif w[0] == 'PIECE' and cases:
                cases[-1]['pieces'].append((int(w[1]), float(w[2]), float(w[3]), float(w[4]), float(w[5])))
    return cases


def sheet(cases):
    cell_w, cell_h, k = 360, 520, 0.8  # one arrival a cell, 0.8 px a world unit
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="Segoe UI, Arial" font-size="12">'
           % (cell_w * len(cases), cell_h + 40),
           '<rect width="100%" height="100%" fill="#d8d4cc"/>',
           '<text x="10" y="18" font-size="14">v3.58 FI358: the tar portal laid out for the arriving body (plane units; '
           'dashed = 1.24 x the body\'s height; red = the first portal, v3.57\'s size)</text>']
    for i, c in enumerate(cases):
        ox, oy = cell_w * i + cell_w / 2, 40 + cell_h / 2

        def X(u):
            return ox + u * k

        def Y(v):
            return oy + v * k
        out.append('<text x="%d" y="%d">%s (%d pieces, %d blot sets)</text>' % (cell_w * i + 10, 40 + 16, c['name'],
                                                                                 len(c['pieces']), c['sets']))
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="#555" stroke-dasharray="6 4"/>'
                   % (X(c['du'] - c['tx']), Y(c['dv'] - c['ty']), 2 * c['tx'] * k, 2 * c['ty'] * k))
        for (n, u, v, s, op) in sorted(c['pieces'], key=lambda p: -p[0]):
            first = n == 0
            out.append('<ellipse cx="%.1f" cy="%.1f" rx="%.1f" ry="%.1f" fill="%s" fill-opacity="%.2f" stroke="#000" '
                       'stroke-width="0.8"/>' % (X(u), Y(v), s * c['sx'] * k, s * c['sy'] * k,
                                                  '#5a0808' if first else '#111', 0.55 if first else 0.35))
            out.append('<text x="%.1f" y="%.1f" fill="#fff" text-anchor="middle">%d</text>' % (X(u), Y(v) + 4, n))
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="#1d4fd8" stroke-width="2"/>'
                   % (X(-c['half']), Y(c['top']), 2 * c['half'] * k, (c['bottom'] - c['top']) * k))
        out.append('<text x="%d" y="%d">body %.0f u tall; portal %.0f x %.0f u (%.2f x)</text>'
                   % (cell_w * i + 10, 40 + cell_h - 8, c['bottom'] - c['top'], 2 * c['tx'], 2 * c['ty'],
                      2 * c['ty'] / max(1.0, c['bottom'] - c['top'])))
    out.append('</svg>')
    return '\n'.join(out) + '\n'


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    cases = read(sys.argv[1])
    if not cases:
        print('no CASE lines in %s' % sys.argv[1])
        return 1
    with open(sys.argv[2], 'w', encoding='ascii', newline='\n') as f:
        f.write(sheet(cases))
    print('%d arrival(s) -> %s' % (len(cases), sys.argv[2]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
