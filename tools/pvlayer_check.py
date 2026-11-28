"""v3.59 PV359: checks an F7 picture set (the picture .dds and its .heat / .spirit layer files) as SH4 Refined reads it.

The format is src\\pvlayer_sim.h's (staging\\v359_feedback\\w1_preview\\PLAN.md): the picture's DDS header carries "PV59" and
a 32-bit stamp at bytes 32..39; each layer file is a 32-byte header ("PVLY", version 1, kind 1 heat / 2 spirit, width,
height, the picture's stamp, payload bytes, CRC-32 of the payload) and runs of (black run, literal count, literal BGRX
pixels) covering width x height. Use it before the next install promotes a new F7 shot to the stock picture:

    py -3 -I tools\\pvlayer_check.py "<your Silent Hill 4 folder>\\preview.dds"
    py -3 -I tools\\pvlayer_check.py <picture.dds> --expect both     (exit 1 unless both layers are usable)

Reads only; writes nothing. Exit 0 = the expectation holds (default: the picture reads; layers are reported)."""
import os
import struct
import sys
import zlib

MAGIC = 0x594C5650
TAG = 0x39355650
MAX_PIX = 1920 * 1080
KINDS = {1: 'heat', 2: 'spirit'}


def layer_path(pic, kind):
    base, ext = os.path.splitext(pic)
    if ext.lower() != '.dds':
        base = pic
    return base + '.' + KINDS[kind]


def read_picture(path):
    with open(path, 'rb') as f:
        d = f.read(128)
    if len(d) < 128 or d[:4] != b'DDS ' or struct.unpack_from('<I', d, 4)[0] != 124:
        return None
    h, w = struct.unpack_from('<II', d, 12)
    tag, stamp = struct.unpack_from('<II', d, 32)
    return w, h, stamp if tag == TAG else 0


def check_layer(path, kind, w, h, stamp):
    """(verdict, lit pixels, bytes): verdict 'used' or why not (the same order as PvlCheck)"""
    if not os.path.exists(path):
        return 'no file', 0, 0
    with open(path, 'rb') as f:
        d = f.read()
    n = len(d)
    if n < 32:
        return 'not a layer file (header)', 0, n
    magic, ver, k, lw, lh, st, ln, crc = struct.unpack_from('<8I', d, 0)
    if magic != MAGIC or ver != 1:
        return 'not a layer file (header)', 0, n
    if k != kind:
        return ("the other layer's file" if k in KINDS else 'not a layer file (header)'), 0, n
    if lw < 16 or lh < 16 or lw > 8192 or lh > 8192 or lw * lh > MAX_PIX:
        return 'not a layer file (header)', 0, n
    if (lw, lh) != (w, h):
        return "not the picture's size (%dx%d, the picture %dx%d)" % (lw, lh, w, h), 0, n
    if not stamp:
        return 'the picture has no stamp (taken before v3.59)', 0, n
    if st != stamp:
        return "not this picture's (stamp %08X, the picture %08X)" % (st, stamp), 0, n
    if ln != n - 32:
        return 'damaged (runs)', 0, n
    if zlib.crc32(d[32:]) & 0xFFFFFFFF != crc:
        return 'damaged (CRC)', 0, n
    at, pix, lit, total = 32, 0, 0, lw * lh
    while pix < total:
        if at + 8 > n:
            return 'damaged (runs)', 0, n
        r, l = struct.unpack_from('<II', d, at)
        at += 8
        if r > total - pix or l > total - pix - r or l * 4 > n - at or (r == 0 and l == 0):
            return 'damaged (runs)', 0, n
        pix += r + l
        lit += l
        at += l * 4
    if at != n:
        return 'damaged (runs)', 0, n
    return 'used', lit, n


def main(argv):
    args = [a for a in argv[1:] if not a.startswith('--')]
    expect = None
    if '--expect' in argv:
        i = argv.index('--expect')
        expect = argv[i + 1] if i + 1 < len(argv) else None
        args = [a for a in args if a != expect]
    if len(args) != 1 or expect not in (None, 'both', 'none', 'heat', 'spirit'):
        print(__doc__)
        return 2
    pic = args[0]
    try:
        p = read_picture(pic)
    except OSError as e:
        print('picture %s: cannot be read (%s)' % (pic, e))
        return 1
    if not p:
        print('picture %s: not a DDS' % pic)
        return 1
    w, h, stamp = p
    print('picture %s: %dx%d, stamp %s' % (pic, w, h, '%08X' % stamp if stamp else 'none (taken before v3.59: no layer can match it)'))
    used = set()
    for kind in (1, 2):
        lp = layer_path(pic, kind)
        verdict, lit, n = check_layer(lp, kind, w, h, stamp)
        extra = ' (%d bytes, %.1f %% of the picture lit)' % (n, 100.0 * lit / (w * h)) if verdict == 'used' else ''
        print('  %s layer %s: %s%s' % (KINDS[kind], lp, verdict, extra))
        if verdict == 'used':
            used.add(KINDS[kind])
    if expect is None:
        return 0
    want = {'both': {'heat', 'spirit'}, 'none': set(), 'heat': {'heat'}, 'spirit': {'spirit'}}[expect]
    ok = used == want if expect in ('both', 'none') else want <= used
    print('expect %s: %s' % (expect, 'OK' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
