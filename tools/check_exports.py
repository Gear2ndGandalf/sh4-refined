"""check_exports.py: compares the built proxy's export table with the real 32-bit system winmm.dll.

    python check_exports.py [proxy.dll] [real winmm.dll]

Every export of the real dll must exist in the proxy at the same ordinal with the same name (or NONAME), the proxy
may add only SH4WM_Patches, SH4WM_Scale2D, (v3.7.0) SH4R_Extra, (v3.43.0) SH4R_QuickCharge, (v3.47.0) SH4R_StickAmount and (v3.50.0) SH4R_Room302, and the proxy must import nothing but KERNEL32 and USER32 (in particular not winmm).
Exit code 0 when all holds.
"""
import os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gen_exports import pe_load, cstr  # noqa: E402


def export_table(path):
    raw, machine, opt, secs, off = pe_load(path)
    va, sz = struct.unpack_from('<II', raw, opt + 96)
    p = off(va)
    base, nfun, nnames, afun, anames, aord = struct.unpack_from('<IIIIII', raw, p + 16)
    names = {}
    for i in range(nnames):
        names[struct.unpack_from('<H', raw, off(aord) + 2 * i)[0]] = cstr(raw, off(struct.unpack_from('<I', raw, off(anames) + 4 * i)[0]))
    out = {}
    for i in range(nfun):
        if struct.unpack_from('<I', raw, off(afun) + 4 * i)[0]:
            out[base + i] = names.get(i)
    return machine, out


def import_dlls(path):
    raw, machine, opt, secs, off = pe_load(path)
    va, sz = struct.unpack_from('<II', raw, opt + 96 + 8)
    p, dlls = off(va), []
    while True:
        name = struct.unpack_from('<I', raw, p + 12)[0]
        if not name:
            break
        dlls.append(cstr(raw, off(name)))
        p += 20
    return dlls


def main():
    proxy = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'out', 'winmm.dll')
    real = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'SysWOW64', 'winmm.dll')
    mp, px = export_table(proxy)
    mr, rl = export_table(real)
    bad = 0
    if mp != 0x14c:
        print('proxy is not x86'); bad += 1
    for o, n in sorted(rl.items()):
        if px.get(o, '<missing>') != n:
            print('ordinal %d: real %s, proxy %s' % (o, n, px.get(o, '<missing>'))); bad += 1
    extra = {o: n for o, n in px.items() if o not in rl}
    if sorted(extra.values()) != ['SH4R_Extra', 'SH4R_QuickCharge', 'SH4R_Room302', 'SH4R_StickAmount', 'SH4WM_Patches',
                                  'SH4WM_Scale2D']:  # v3.43 QC343  # v3.47 CT347: + SH4R_StickAmount  # v3.50 R302: + SH4R_Room302
        print('unexpected extra exports:', extra); bad += 1
    imps = import_dlls(proxy)
    if sorted(d.upper() for d in imps) != ['KERNEL32.DLL', 'USER32.DLL']:
        print('unexpected imports:', imps); bad += 1
    print('%s: %d exports (real %d: all at the same ordinal and name%s), extra %s, imports %s -> %s' % (
        os.path.abspath(proxy), len(px), len(rl), '' if not bad else ' NOT', extra, imps, 'OK' if not bad else 'FAIL'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
