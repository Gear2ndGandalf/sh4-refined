"""gen_exports.py: generates the winmm.dll proxy's export table from the real 32-bit system winmm.dll.

    python gen_exports.py [--dll C:\\Windows\\SysWOW64\\winmm.dll] [--lib <SDK x86 WinMM.Lib>] [--out ..\\src]

Writes
  * winmm.def          every export of the real dll at its own ordinal (NONAME ones stay NONAME), plus the mod's
                       own exports SH4WM_Patches (data) and SH4WM_Scale2D at the first free ordinals;
  * winmm_exports.inc  one naked forwarding stub per export (included by sh4windowmod.c).
Each stub jumps straight into the real function (the stack is untouched, so the calling convention does not matter);
the first call of each export resolves it from %windir%\\SysWOW64\\winmm.dll by name (NONAME ones by ordinal). The
stdcall argument size, taken from the SDK import library's decorated names, is used only by the never-expected failure
path (real dll missing), so it returns to the caller with a balanced stack and MMSYSERR_ERROR (1).
The script refuses forwarded or data exports (the Windows 11 winmm.dll has neither; check again on other builds).
Standard library only, Python 3.
"""
import argparse, glob, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
# stdcall argument bytes of exports the SDK import library does not list
EXTRA_ARGS = {'mciExecute': 4, 'mmsystemGetVersion': 0}


def pe_load(path):
    raw = open(path, 'rb').read()
    pe = struct.unpack_from('<I', raw, 0x3c)[0]
    assert raw[pe:pe + 4] == b'PE\0\0'
    machine = struct.unpack_from('<H', raw, pe + 4)[0]
    n = struct.unpack_from('<H', raw, pe + 6)[0]
    so = struct.unpack_from('<H', raw, pe + 20)[0]
    opt = pe + 24
    secs = [struct.unpack_from('<8sIIII', raw, opt + so + 40 * i) + (struct.unpack_from('<I', raw, opt + so + 40 * i + 36)[0],)
            for i in range(n)]

    def off(rva):
        for nm, vs, va, rs, ro, ch in secs:
            if va <= rva < va + max(vs, rs):
                return rva - va + ro
        raise ValueError(hex(rva))
    return raw, machine, opt, secs, off


def cstr(raw, o):
    return raw[o:raw.index(b'\0', o)].decode('ascii')


def exports(path):
    raw, machine, opt, secs, off = pe_load(path)
    assert machine == 0x14c, 'not a 32-bit dll: %s' % path
    va, sz = struct.unpack_from('<II', raw, opt + 96)
    p = off(va)
    base, nfun, nnames, afun, anames, aord = struct.unpack_from('<IIIIII', raw, p + 16)
    funs = [struct.unpack_from('<I', raw, off(afun) + 4 * i)[0] for i in range(nfun)]
    names = {}
    for i in range(nnames):
        names[struct.unpack_from('<H', raw, off(aord) + 2 * i)[0]] = cstr(raw, off(struct.unpack_from('<I', raw, off(anames) + 4 * i)[0]))
    out = []
    for i, f in enumerate(funs):
        if not f:
            continue
        if va <= f < va + sz:
            raise SystemExit('forwarded export %s -> %s: not supported' % (names.get(i, '#%d' % (base + i)), cstr(raw, off(f))))
        code = any(sva <= f < sva + max(vs, rs) and ch & 0x20000000 for nm, vs, sva, rs, ro, ch in secs)
        if not code:
            raise SystemExit('data export %s: not supported' % names.get(i, '#%d' % (base + i)))
        out.append({'ord': base + i, 'name': names.get(i), 'rva': f})
    return out


def lib_args(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'!<arch>\n'
    p, out = 8, {}
    while p + 60 <= len(d):
        size = int(d[p + 48:p + 58])
        body = d[p + 60:p + 60 + size]
        if len(body) >= 20:
            s1, s2 = struct.unpack_from('<HH', body, 0)
            if s1 == 0 and s2 == 0xFFFF:
                sym = body[20:body.index(b'\0', 20)].decode()
                m = re.match(r'^_([A-Za-z0-9_]+)@(\d+)$', sym)
                if m:
                    out[m.group(1)] = int(m.group(2))
        p += 60 + size + (size & 1)
    return out


def main():
    ap = argparse.ArgumentParser()
    windir = os.environ.get('WINDIR', r'C:\Windows')
    ap.add_argument('--dll', default=os.path.join(windir, 'SysWOW64', 'winmm.dll'))
    libs = sorted(glob.glob(r'C:\Program Files (x86)\Windows Kits\10\Lib\*\um\x86\WinMM.Lib'))
    ap.add_argument('--lib', default=libs[-1] if libs else None)
    ap.add_argument('--out', default=os.path.join(HERE, '..', 'src'))
    a = ap.parse_args()
    ex = exports(a.dll)
    args = lib_args(a.lib)
    args.update(EXTRA_ARGS)
    by_rva = {}
    for e in ex:
        if e['name']:
            by_rva.setdefault(e['rva'], e['name'])
    rows = []
    for k, e in enumerate(ex):
        alias = e['name'] or by_rva.get(e['rva'])          # a NONAME export that is another export's alias
        if alias not in args:
            raise SystemExit('no argument size for %s (ordinal %d): add it to EXTRA_ARGS' % (alias, e['ord']))
        rows.append((k, e['ord'], e['name'], alias, args[alias]))
    free = max(e['ord'] for e in ex) + 1
    os.makedirs(a.out, exist_ok=True)
    src = os.path.basename(a.dll)
    with open(os.path.join(a.out, 'winmm.def'), 'w', newline='\r\n') as f:
        f.write('; generated by tools/gen_exports.py from %s (%d exports) - do not edit\n' % (src, len(ex)))
        f.write('LIBRARY winmm\nEXPORTS\n')
        for k, o, name, alias, nb in rows:
            if name:
                f.write('    %-32s = PX_%d @%d\n' % (name, k, o))
            else:
                f.write('    %-32s @%d NONAME\n' % ('PX_%d' % k, o))
        f.write('    %-32s @%d DATA\n' % ('SH4WM_Patches', free))
        f.write('    %-32s @%d\n' % ('SH4WM_Scale2D', free + 1))
        f.write('    %-32s @%d DATA\n' % ('SH4R_Extra', free + 2))  # v3.7.0 (was only in the hand-kept .def)  # v3.43 QC343
        f.write('    %-32s @%d\n' % ('SH4R_QuickCharge', free + 3))  # v3.43 QC343: Quick Charge's factor x 100
        f.write('    %-32s @%d\n' % ('SH4R_StickAmount', free + 4))  # v3.47 CT347: Stick Movement's push for Playable Eileen
        f.write('    %-32s @%d DATA\n' % ('SH4R_Room302', free + 5))  # v3.50 R302: Room 302's hand-over block for Playable Eileen
    with open(os.path.join(a.out, 'winmm_exports.inc'), 'w', newline='\r\n') as f:
        f.write('// generated by tools/gen_exports.py from %s (%d exports) - do not edit\n' % (src, len(ex)))
        f.write('// {name or NULL, ordinal, alias for a NONAME export}; PX_STUB(index, stdcall argument bytes)\n')
        f.write('#define PX_COUNT %d\n' % len(ex))
        f.write('#ifdef PX_TABLE\n')
        for k, o, name, alias, nb in rows:
            f.write('    { %s, %d, %s },\n' % ('"%s"' % name if name else 'NULL', o, '"%s"' % alias if not name else 'NULL'))
        f.write('#else\n')
        for k, o, name, alias, nb in rows:
            f.write('PX_STUB(%d, %d)  // @%d %s\n' % (k, nb, o, name or '(NONAME, alias of %s)' % alias))
        f.write('#endif\n')
    print('%s: %d exports (ordinals %d-%d, %d NONAME); SH4WM_Patches @%d -> %s' % (
        a.dll, len(ex), ex[0]['ord'], ex[-1]['ord'], sum(1 for r in rows if not r[2]), free, os.path.abspath(a.out)))


if __name__ == '__main__':
    sys.exit(main())
