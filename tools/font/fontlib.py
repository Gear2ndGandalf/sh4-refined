"""fontlib.py: pure-Python helpers for SH4 Refined's HD menu font (v3.43 FN343).

No third-party modules (the rig has no PIL / fontTools / freetype, and installs are not allowed):
  * png_write       a minimal PNG encoder (grey or RGBA)
  * StockFont       the game's own glyph decoder (0x556B60 / TextDecodeGlyph 0x556BC0, SH4Decomp
                    src/text/text_556770.c), used ONLY to measure the stock metrics and to draw the local
                    "stock" comparison samples. Its pixels are never written into a shipped file.
  * TrueType        a small TrueType (glyf) reader: cmap 4 / 12, loca, glyf simple + composite, hmtx, head,
                    hhea, OS/2. Font files from github.com/google/fonts are untrusted data: every offset and
                    count is bounds-checked, recursion and point counts are capped; run with python -I.
  * raster          a non-zero-winding coverage rasteriser (quadratic curves flattened, 4 x 4 subsamples
                    per pixel by exact horizontal span coverage).
"""
import struct, zlib, math

# ------------------------------------------------------------------ PNG
def png_write(path, w, h, data, channels):
    """data: bytes / bytearray, w * h * channels (1 = grey, 3 = RGB, 4 = RGBA)."""
    ct = {1: 0, 3: 2, 4: 6}[channels]
    raw = bytearray()
    stride = w * channels
    for y in range(h):
        raw.append(0)
        raw += data[y * stride:(y + 1) * stride]

    def chunk(t, b):
        c = struct.pack('>I', len(b)) + t + b
        return c + struct.pack('>I', zlib.crc32(t + b) & 0xFFFFFFFF)
    out = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, ct, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(out)


# ------------------------------------------------------------------ the game's font (measurement only)
# Cell sizes D_006999B4 (0x6999B4): font 0 = 24 x 32, font 1 = 20 x 26 (640 x 480 layout units = texels).
CELL = {0: (24, 32), 1: (20, 26)}
NGLYPH = 0xE0          # glyph i = Latin-1 character i + 0x20 (byte-mode byte 0x100 - c, i = 0xE0 - byte)
# The stock palette (0x559EE0): value 0 clear, 1..6 white with alpha 0x5F..0xFF.
STOCK_ALPHA = [0, 0x5F, 0x7F, 0x9F, 0xBF, 0xDF, 0xFF, 0]


class StockFont:
    """One font entry of fontdata_kr.bin (the bank's entry e: the struct the game keeps in [0x1261B98 + 4 e])."""

    def __init__(self, path, entry):
        d = open(path, 'rb').read()
        n = struct.unpack_from('<I', d, 0)[0]
        if not (1 <= n <= 16) or entry >= n:
            raise ValueError('not a font bank')
        o = struct.unpack_from('<I', d, 4 + 4 * entry)[0]
        e = struct.unpack_from('<I', d, 4 + 4 * (entry + 1))[0] if entry + 1 < n else len(d)
        self.f = d[o:e]
        self.entry = entry
        self.widths = bytes(self.f[0x10:0x10 + NGLYPH])
        self.kern = bytes(self.f[0xF0:0xF0 + NGLYPH])
        self.cw, self.ch = CELL[entry]

    def advance(self, i, spacing=2.5):
        """Layout advance of glyph i in 640-wide units (0x5577CE): width + two signed nibbles + spacing."""
        k = self.kern[i]
        lo, hi = k & 15, k >> 4
        lo -= 16 if lo >= 8 else 0
        hi -= 16 if hi >= 8 else 0
        return self.widths[i] + lo + hi + spacing

    def glyph(self, i):
        """Decoded glyph i as rows of pixel values 0..6, width = widths[i], height = cell height."""
        f = self.f
        w, h = self.widths[i], self.ch
        thresholds = []
        p = 0
        while p + 2 <= len(f):
            v = struct.unpack_from('<H', f, p)[0]
            if v == 0:
                break
            thresholds.append(v)
            p += 2
        n = sum(1 for v in thresholds if v <= i)
        code = struct.unpack_from('<H', f, 2 * (i + 0xE8))[0]
        rows = [[0] * w for _ in range(h)]
        if code == 0 or w == 0:
            return rows
        pos = ((n << 16) + code) * 4
        state = {'p': pos, 'bits': 32, 'acc': 0, 'high': 0}

        def rd3():
            # 0x556B60: 3-bit groups, LSB first, from little-endian dwords
            a, nb = state['acc'], state['bits']
            high = 0
            if nb > 29:
                word = struct.unpack_from('<I', f, state['p'])[0] if state['p'] + 4 <= len(f) else 0
                high = (word >> (nb - 29)) & 0xE0000000 if nb - 29 < 32 else 0
                a = (a | (word << (32 - nb))) & 0xFFFFFFFF
                state['p'] += 4
                nb -= 32
            state['bits'] = nb + 3
            r = a & 7
            state['acc'] = ((a >> 3) | high) & 0xFFFFFFFF
            return r
        run = 0
        for y in range(h):
            for x in range(w):
                if run:
                    pix = 0
                    run -= 1
                else:
                    pix = rd3()
                    if pix == 7:
                        pix = 0
                        run = rd3()
                        if run == 0:
                            run = rd3() + 7
                            if run == 7:
                                run = rd3() + 14
                                if run == 14:
                                    run = rd3() + 21
                                    run += rd3() * 8
                                    if run == 21:
                                        run = rd3() + 84
                                        run += rd3() * 8
                                        run += rd3() << 6
                rows[y][x] = pix
        return rows


# ------------------------------------------------------------------ TrueType (untrusted input)
class FontError(Exception):
    pass


class TrueType:
    MAX_POINTS = 20000      # v3.44 FN344-FONT-1: the points of one decoded glyph, ALL its components together
    MAX_DEPTH = 6
    MAX_COMPONENTS = 256    # v3.44 FN344-FONT-1: component records visited for one decoded glyph, every depth together

    def __init__(self, path):
        d = open(path, 'rb').read()
        if len(d) < 12 or len(d) > 64 << 20:
            raise FontError('size')
        self.d = d
        tag = d[0:4]
        if tag not in (b'\x00\x01\x00\x00', b'true'):
            raise FontError('not a TrueType (glyf) font: %r' % tag)
        num = self.u16(4)
        if num > 200:
            raise FontError('tables')
        self.t = {}
        for k in range(num):
            o = 12 + 16 * k
            name = d[o:o + 4].decode('latin-1')
            off, ln = self.u32(o + 8), self.u32(o + 12)
            if off + ln > len(d):
                raise FontError('table %s out of range' % name)
            self.t[name] = (off, ln)
        for need in ('head', 'hhea', 'hmtx', 'loca', 'glyf', 'cmap', 'maxp'):
            if need not in self.t:
                raise FontError('no %s table' % need)
        h = self.t['head'][0]
        self.upem = self.u16(h + 18)
        if not (16 <= self.upem <= 16384):
            raise FontError('unitsPerEm')
        self.locfmt = self.s16(h + 50)
        self.nglyphs = self.u16(self.t['maxp'][0] + 4)
        hh = self.t['hhea'][0]
        self.ascender, self.descender = self.s16(hh + 4), self.s16(hh + 6)
        self.nhm = self.u16(hh + 34)
        self.capH = self.xH = None
        if 'OS/2' in self.t:
            o, ln = self.t['OS/2']
            ver = self.u16(o)
            if ver >= 2 and ln >= 90:
                self.xH, self.capH = self.s16(o + 86), self.s16(o + 88)
        self.cmap = self._cmap()
        self.name = self._name()

    def u16(self, o):
        if o < 0 or o + 2 > len(self.d):
            raise FontError('read past end')
        return struct.unpack_from('>H', self.d, o)[0]

    def s16(self, o):
        if o < 0 or o + 2 > len(self.d):
            raise FontError('read past end')
        return struct.unpack_from('>h', self.d, o)[0]

    def u32(self, o):
        if o < 0 or o + 4 > len(self.d):
            raise FontError('read past end')
        return struct.unpack_from('>I', self.d, o)[0]

    def _name(self):
        if 'name' not in self.t:
            return '?'
        o = self.t['name'][0]
        cnt, so = self.u16(o + 2), self.u16(o + 4)
        best = None
        for k in range(min(cnt, 500)):
            r = o + 6 + 12 * k
            pid, eid, lid, nid, ln, off = [self.u16(r + 2 * j) for j in range(6)]
            if nid == 4 and pid == 3 and eid == 1:
                s = self.d[o + so + off:o + so + off + ln]
                try:
                    best = s.decode('utf-16-be')
                except UnicodeDecodeError:
                    pass
                break
        return best or '?'

    def _cmap(self):
        o = self.t['cmap'][0]
        n = self.u16(o + 2)
        sub = None
        for k in range(min(n, 64)):
            pid, eid, off = self.u16(o + 4 + 8 * k), self.u16(o + 6 + 8 * k), self.u32(o + 8 + 8 * k)
            fmt = self.u16(o + off)
            if pid == 3 and eid == 10 and fmt == 12:
                sub = (o + off, 12)
            elif pid == 3 and eid == 1 and fmt == 4 and sub is None:
                sub = (o + off, 4)
        if sub is None:
            raise FontError('no Unicode cmap')
        m = {}
        so, fmt = sub
        if fmt == 4:
            segx2 = self.u16(so + 6)
            ends, starts, deltas, roffs = so + 14, so + 16 + segx2, so + 16 + 2 * segx2, so + 16 + 3 * segx2
            for s in range(segx2 // 2):
                e, st = self.u16(ends + 2 * s), self.u16(starts + 2 * s)
                dl, ro = self.u16(deltas + 2 * s), self.u16(roffs + 2 * s)
                for c in range(st, min(e, 0x2FFF) + 1):
                    if c == 0xFFFF:
                        break
                    if ro == 0:
                        g = (c + dl) & 0xFFFF
                    else:
                        g = self.u16(roffs + 2 * s + ro + 2 * (c - st))
                        if g:
                            g = (g + dl) & 0xFFFF
                    if g:
                        m[c] = g
        else:
            ng = self.u32(so + 12)
            for k in range(min(ng, 10000)):
                st, e, g0 = self.u32(so + 16 + 12 * k), self.u32(so + 20 + 12 * k), self.u32(so + 24 + 12 * k)
                for c in range(st, min(e, 0x2FFF) + 1):
                    m[c] = g0 + c - st
        return m

    def advance(self, g):
        o = self.t['hmtx'][0]
        k = min(g, self.nhm - 1)
        return self.u16(o + 4 * k)

    def _gofs(self, g):
        if not (0 <= g < self.nglyphs):
            raise FontError('glyph id')
        lo = self.t['loca'][0]
        if self.locfmt == 0:
            a, b = 2 * self.u16(lo + 2 * g), 2 * self.u16(lo + 2 * g + 2)
        else:
            a, b = self.u32(lo + 4 * g), self.u32(lo + 4 * g + 4)
        go, gl = self.t['glyf']
        if b < a or b > gl:
            raise FontError('loca')
        return go + a, b - a

    def contours(self, g):
        """List of contours, each a list of (x, y, on) in font units. v3.44 FN344-FONT-1: one budget per decoded glyph
        (components visited and points expanded, every depth together), so a crafted composite tree (64 composites at
        each depth) is refused instead of expanding exponentially."""
        return self._contours(g, 0, {'comps': 0, 'pts': 0})

    def _contours(self, g, depth, budget):
        if depth > self.MAX_DEPTH:
            raise FontError('composite depth')
        o, ln = self._gofs(g)
        if ln == 0:
            return []
        end = o + ln  # v3.44 FN344-FONT-1: every read of this glyph's record stays inside its loca length

        def need(q, n):
            if q < o or q + n > end:
                raise FontError('glyph record past its loca length')
        need(o, 10)
        nc = self.s16(o)
        if nc >= 0:
            need(o + 10, 2 * nc + 2)
            ends = [self.u16(o + 10 + 2 * k) for k in range(nc)]
            npts = ends[-1] + 1 if ends else 0
            budget['pts'] += npts
            if npts > self.MAX_POINTS or budget['pts'] > self.MAX_POINTS:
                raise FontError('points')
            il = self.u16(o + 10 + 2 * nc)
            p = o + 12 + 2 * nc + il
            flags = []
            while len(flags) < npts:
                if p >= end:
                    raise FontError('flags')
                fl = self.d[p]
                p += 1
                flags.append(fl)
                if fl & 8:
                    if p >= end:
                        raise FontError('flags')
                    flags += [fl] * self.d[p]
                    p += 1
            flags = flags[:npts]
            xs, ys = [], []
            for arr, short, same in ((xs, 2, 16), (ys, 4, 32)):
                v = 0
                for fl in flags:
                    if fl & short:
                        if p >= end:
                            raise FontError('coords')
                        dv = self.d[p]
                        p += 1
                        v += dv if fl & same else -dv
                    elif not fl & same:
                        need(p, 2)
                        v += self.s16(p)
                        p += 2
                    arr.append(v)
            out, st = [], 0
            for e in ends:
                if e < st or e >= npts:
                    raise FontError('contour ends')
                out.append([(xs[k], ys[k], flags[k] & 1) for k in range(st, e + 1)])
                st = e + 1
            return out
        # composite
        out = []
        p = o + 10
        for _ in range(64):
            budget['comps'] += 1
            if budget['comps'] > self.MAX_COMPONENTS:
                raise FontError('components')
            need(p, 4)
            fl, gi = self.u16(p), self.u16(p + 2)
            p += 4
            if fl & 1:
                need(p, 4)
                a1, a2 = self.s16(p), self.s16(p + 2)
                p += 4
            else:
                need(p, 2)
                a1, a2 = struct.unpack_from('>bb', self.d, p)
                p += 2
            xx, xy, yx, yy = 1.0, 0.0, 0.0, 1.0
            if fl & 8:
                need(p, 2)
                xx = yy = self.s16(p) / 16384.0
                p += 2
            elif fl & 0x40:
                need(p, 4)
                xx, yy = self.s16(p) / 16384.0, self.s16(p + 2) / 16384.0
                p += 4
            elif fl & 0x80:
                need(p, 8)
                xx, xy, yx, yy = [self.s16(p + 2 * j) / 16384.0 for j in range(4)]
                p += 8
            dx, dy = (a1, a2) if fl & 2 else (0, 0)  # point matching (args as point indices) is not supported
            for c in self._contours(gi, depth + 1, budget):
                out.append([(x * xx + y * yx + dx, x * xy + y * yy + dy, on) for x, y, on in c])
            if not fl & 0x20:
                break
        else:  # v3.44 FN344-COMP-1: a 65th component still announced (MORE_COMPONENTS): refused, never a partial glyph
            raise FontError('more than 64 components in one record')
        return out


def flatten(contours, sx, sy, ox, oy, steps=8):
    """Font-unit contours -> polygons in pixel space: X = ox + x * sx, Y = oy - y * sy (y down)."""
    polys = []
    for c in contours:
        if len(c) < 2:
            continue
        pts = [(ox + x * sx, oy - y * sy, on) for x, y, on in c]
        # make the implied on-curve points explicit
        ex = []
        n = len(pts)
        for k in range(n):
            x0, y0, on0 = pts[k]
            x1, y1, on1 = pts[(k + 1) % n]
            ex.append((x0, y0, on0))
            if not on0 and not on1:
                ex.append(((x0 + x1) / 2, (y0 + y1) / 2, 1))
        # start at an on-curve point
        s = next((k for k, q in enumerate(ex) if q[2]), None)
        if s is None:
            continue
        ex = ex[s:] + ex[:s]
        poly = [(ex[0][0], ex[0][1])]
        k, n = 1, len(ex)
        while k <= n:
            q = ex[k % n]
            if q[2]:
                poly.append((q[0], q[1]))
                k += 1
            else:
                p0 = poly[-1]
                p2 = ex[(k + 1) % n]
                for t in range(1, steps + 1):
                    u = t / steps
                    a, b, cc = (1 - u) ** 2, 2 * u * (1 - u), u * u
                    poly.append((a * p0[0] + b * q[0] + cc * p2[0], a * p0[1] + b * q[1] + cc * p2[1]))
                k += 2
        polys.append(poly)
    return polys


def raster(polys, w, h, sub=4):
    """Coverage 0..1 per pixel (list of rows), non-zero winding, sub scanlines per pixel, exact x spans."""
    edges = []
    for poly in polys:
        n = len(poly)
        for k in range(n):
            x0, y0 = poly[k]
            x1, y1 = poly[(k + 1) % n]
            if y0 == y1:
                continue
            d = 1
            if y0 > y1:
                x0, y0, x1, y1, d = x1, y1, x0, y0, -1
            edges.append((y0, y1, x0, (x1 - x0) / (y1 - y0), d))
    cov = [[0.0] * w for _ in range(h)]
    if not edges:
        return cov
    edges.sort()
    ymin = max(0, int(math.floor(min(e[0] for e in edges))))
    ymax = min(h, int(math.ceil(max(e[1] for e in edges))))
    wgt = 1.0 / sub
    for py in range(ymin, ymax):
        row = cov[py]
        for s in range(sub):
            yy = py + (s + 0.5) / sub
            xs = []
            for y0, y1, x0, dxdy, d in edges:
                if y0 > yy:
                    break
                if yy < y1:
                    xs.append((x0 + (yy - y0) * dxdy, d))
            if not xs:
                continue
            xs.sort()
            wind = 0
            for j in range(len(xs) - 1):
                wind += xs[j][1]
                if wind == 0:
                    continue
                a, b = xs[j][0], xs[j + 1][0]
                if b <= a:
                    continue
                a, b = max(a, 0.0), min(b, float(w))
                if b <= a:
                    continue
                ia, ib = int(a), int(b)
                if ia == ib:
                    row[ia] += (b - a) * wgt
                    continue
                row[ia] += (ia + 1 - a) * wgt
                for x in range(ia + 1, ib):
                    row[x] += wgt
                if ib < w:
                    row[ib] += (b - ib) * wgt
    for row in cov:
        for x in range(w):
            if row[x] > 1.0:
                row[x] = 1.0
    return cov
