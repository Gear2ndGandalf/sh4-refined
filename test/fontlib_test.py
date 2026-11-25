r"""v3.44 FN344-FONT-1 / FN344-COMP-1: tools/font/fontlib.py refuses hostile glyph records (crafted fonts, no file needed).
Run: python -I test/fontlib_test.py   (prints one line per check, then "N failure(s)"; exit code = failures)"""
import os, struct, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'font'))
import fontlib  # noqa: E402

fails = 0


def check(ok, what):
    global fails
    print(('ok    ' if ok else 'FAIL  ') + what)
    fails += 0 if ok else 1


def font(recs):
    """A TrueType reader over just a glyf + loca (long offsets) built from the given glyph records."""
    glyf, loca = b'', [0]
    for r in recs:
        if len(r) % 2:
            r += b'\0'
        glyf += r
        loca.append(len(glyf))
    tt = fontlib.TrueType.__new__(fontlib.TrueType)
    tt.d = glyf + struct.pack('>%dI' % len(loca), *loca)
    tt.t = {'glyf': (0, len(glyf)), 'loca': (len(glyf), 4 * len(loca))}
    tt.locfmt, tt.nglyphs = 1, len(recs)
    return tt


LEAF = struct.pack('>hhhhh', 1, 0, 0, 0, 0) + struct.pack('>HH', 0, 0) + bytes([1]) + bytes([0, 0]) * 2  # 1 point


def composite(child, n):
    r = struct.pack('>hhhhh', -1, 0, 0, 0, 0)
    for j in range(n):
        r += struct.pack('>HHbb', 0x20 if j < n - 1 else 0, child, 0, 0)  # MORE_COMPONENTS on all but the last
    return r


def refused(tt, g, want):
    try:
        tt.contours(g)
        return False, 'accepted'
    except fontlib.FontError as e:
        return want in str(e), str(e)


# a 6-deep tree, 64 components at each depth (64^6 leaves before the fix): the total budget stops it at once
tt = font([composite(k + 1, 64) for k in range(6)] + [LEAF])
t0 = time.time()
ok, why = refused(tt, 0, 'components')
check(ok and time.time() - t0 < 1.0, 'a crafted 6-deep, 64-wide composite tree is refused fast (%s)' % why)
check(tt.contours(6) == [[(0, 0, 1)]], 'its leaf still decodes')
# 64 components in one record are fine, a 65th announced is refused (never a partial glyph)
check(len(font([composite(1, 64), LEAF]).contours(0)) == 64, '64 components in one record: decoded')
ok, why = refused(font([composite(1, 65), LEAF]), 0, '64 components')
check(ok, 'a 65th component announced: refused (%s)' % why)
# a simple glyph whose record claims more than its loca length
short = struct.pack('>hhhhh', 1, 0, 0, 0, 0) + struct.pack('>H', 5)
tt2 = fontlib.TrueType.__new__(fontlib.TrueType)
tt2.d = short + b'\xff' * 64 + struct.pack('>2I', 0, len(short))
tt2.t = {'glyf': (0, len(short) + 64), 'loca': (len(short) + 64, 8)}
tt2.locfmt, tt2.nglyphs = 1, 1
ok, why = refused(tt2, 0, 'loca length')
check(ok, 'a record past its loca length: refused (%s)' % why)
# a composite whose component record is cut short by its loca length
cut = composite(1, 2)[:-2]
ok, why = refused(font([cut, LEAF]), 0, 'loca length')
check(ok, 'a component record cut short by its loca length: refused (%s)' % why)
# the point budget counts every component: 3 x 8000 points > 20000
big = struct.pack('>hhhhh', 1, 0, 0, 0, 0) + struct.pack('>HH', 7999, 0) + bytes([1 | 2 | 4 | 16 | 32]) * 8000 + bytes(16000)
ok, why = refused(font([composite(1, 3), big]), 0, 'points')
check(ok, 'points over the budget across components: refused (%s)' % why)
check(len(font([big]).contours(0)[0]) == 8000, 'one 8000-point glyph alone: decoded')
print('%d failure(s)' % fails)
sys.exit(fails)
