"""inv337_gen.py: v3.37 INV337. Translates the two Cheat Engine scripts in tools/inv337 (Refined's copies of the
cheat table's Extend Toolbelt v6, entry 7310, and Stackable Items v3, entry 7360) into MSVC inline asm:
src/inventory_cave.inc (one naked function per script, its data block, the exe sites to patch with their stock bytes).

    python -I tools/inv337_gen.py [--exe <stock SILENT HILL 4.exe>] [--check]

--exe: the stock GOG v7 exe (md5 15a85581...; default the SH4Decomp copy orig/sh4.exe, read only).
--check: translate and verify only; the .inc on disk must equal the new output (exit 1 if not).

Checks (any failure = exit 1, nothing written):
  * every aobscanmodule pattern matches exactly once in the exe's .text;
  * every [DISABLE] restore equals the exe bytes at its site, and every [ENABLE] patch has the same length;
  * every patch target is a code label of the script, every jmp / call out of the cave is a whole address;
  * with all of Refined's patches applied to an image of the exe, every cheat-table script of the two (7310 and 7360)
    has at least one aobscan that no longer matches (Cheat Engine then refuses the whole script).
CE numbers are hex; they become 0x... here. Absolute memory operands become ds:[...] with the operand size.
"""
import argparse, hashlib, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(HERE, 'inv337')
OUT = os.path.join(ROOT, 'src', 'inventory_cave.inc')
OUT_RDATA = os.path.join(ROOT, 'test', 'inventory_rdata.h')  # the exe's item tables 5BF100..5BF300 (test/inventory_test.c)
EXE = 'sh4.exe'  # a copy of your stock GOG v7 SILENT HILL 4.exe (md5 15a85581...)
SCRIPTS = [('pk', 'pockets_inv337.cea', 'Extended Toolbelt (CT 7310 Extend Toolbelt v6)'),
           ('st', 'stackable_inv337.cea', 'Stackable Items (CT 7360 Stackable Items v3)')]
EXTERN = {'inv_tblive': 'g_invTbLive', 'inv_ston': 'g_invStOn'}  # Refined's own flags (inventory.inc)
REGS = set('eax ebx ecx edx esi edi ebp esp ax bx cx dx si di bp sp al ah bl bh cl ch dl dh'.split())
R32 = set('eax ebx ecx edx esi edi ebp esp'.split())
R16 = set('ax bx cx dx si di bp sp'.split())
R8 = set('al ah bl bh cl ch dl dh'.split())
HEXRE = re.compile(r'^[0-9A-Fa-f]+$')
ERR = []


def err(msg):
    ERR.append(msg)


# ------------------------------------------------------------------ exe
class Exe:
    def __init__(self, path):
        self.raw = open(path, 'rb').read()
        self.md5 = hashlib.md5(self.raw).hexdigest()
        e = struct.unpack_from('<I', self.raw, 0x3C)[0]
        n = struct.unpack_from('<H', self.raw, e + 6)[0]
        so = e + 24 + struct.unpack_from('<H', self.raw, e + 20)[0]
        self.secs = []
        for i in range(n):
            vs, va, rs, rp = struct.unpack_from('<IIII', self.raw, so + i * 40 + 8)
            self.secs.append((va + 0x400000, max(vs, rs), rp, rs))
        va, vs, rp, rs = self.secs[0]
        self.tva = va
        self.text = bytearray(self.raw[rp:rp + rs])

    def read(self, va, n):
        for sva, size, rp, rs in self.secs:
            if sva <= va < sva + size:
                return bytes(self.raw[rp + va - sva:rp + va - sva + n])
        raise KeyError('%08X' % va)


def aob_rx(pat):
    return re.compile(b''.join(b'.' if t == '??' else re.escape(bytes([int(t, 16)])) for t in pat.split()), re.S)


def scan(text, base, pat):
    return [base + m.start() for m in aob_rx(pat).finditer(bytes(text))]


# ------------------------------------------------------------------ parse
def num(tok):
    return int(tok, 16)


def strip_comment(line):
    i = line.find('//')
    return (line[:i], line[i + 2:].strip()) if i >= 0 else (line, '')


def split_ops(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


class Script:
    def __init__(self, tag, fname, title, exe):
        self.tag, self.fname, self.title = tag, fname, title
        text = open(os.path.join(SRC, fname), encoding='utf-8').read()
        text = re.sub(r'\{\$lua\}.*?\{\$asm\}', '', text, flags=re.S)  # Lua blocks: done in C (inventory.inc)
        text = re.sub(r'^\{.*?^\}', '', text, count=1, flags=re.S | re.M)  # the header comment
        en, dis = re.split(r'^\[DISABLE\]$', text, flags=re.M)
        self.lines = en.split('\n')
        self.dis = dis
        self.aob = {}
        for m in re.finditer(r'aobscanmodule\((\w+),[^,]+,([0-9A-Fa-f? ]+)\)', en):
            hits = scan(exe.text, exe.tva, m.group(2))
            if len(hits) != 1:
                err('%s: aob %s matches %d times' % (fname, m.group(1), len(hits)))
                continue
            self.aob[m.group(1)] = (hits[0], m.group(2))
        self.allocs = {}
        for m in re.finditer(r'alloc\((\w+),\$([0-9A-Fa-f]+)\)', en):
            self.allocs[m.group(1)] = num(m.group(2))
        self.parse_body()
        self.parse_patches(exe)

    def parse_body(self):
        lines = self.lines
        mem = self.tag + '_mem:'
        i = next(k for k, l in enumerate(lines) if l.strip() == mem)
        # data block: labels followed by dd / db, until a label followed by code
        self.data = []  # (label, offset)
        self.blob = bytearray()
        self.dlabel = {}
        pend = []
        i += 1
        while True:
            l, _ = strip_comment(lines[i])
            l = l.strip()
            if not l:
                i += 1
                continue
            m = re.match(r'^(\w+):$', l)
            if m:
                # look ahead: data or code?
                j = i + 1
                while not strip_comment(lines[j])[0].strip():
                    j += 1
                nxt = strip_comment(lines[j])[0].strip()
                if re.match(r'^(dd|db)\b', nxt) or re.match(r'^\w+:$', nxt):
                    pend.append(m.group(1))
                    i += 1
                    continue
                break
            m = re.match(r'^(dd|db)\s+(.*)$', l)
            if not m:
                err('%s: unexpected data line %r' % (self.fname, l))
                break
            for p in pend:
                self.dlabel[p] = len(self.blob)
            pend = []
            if m.group(1) == 'dd':
                self.blob += struct.pack('<I', num(m.group(2).strip()))
            else:
                self.blob += bytes(num(x) for x in m.group(2).split())
            i += 1
        for p in pend:
            self.dlabel[p] = len(self.blob)
        while len(self.blob) % 4:
            self.blob.append(0)
        self.code_start = i
        end = next(k for k in range(i, len(lines)) if re.match(r'^// -+ patches', lines[k].strip()))
        self.code = lines[i:end]
        self.patch_lines = lines[end + 1:]
        self.clabels = []
        for l in self.code:
            m = re.match(r'^(\w+):\s*(//.*)?$', l.strip())
            if m:
                self.clabels.append(m.group(1))
        if len(set(self.clabels)) != len(self.clabels):
            err('%s: duplicate code labels' % self.fname)

    def site(self, ref):
        m = re.match(r'^(\w+)(?:\+([0-9A-Fa-f]+))?$', ref)
        if not m or m.group(1) not in self.aob:
            return None
        return self.aob[m.group(1)][0] + (num(m.group(2)) if m.group(2) else 0)

    def parse_patches(self, exe):
        self.patches = []
        cur = None
        for raw in self.patch_lines:
            l, _ = strip_comment(raw)
            l = l.strip()
            if not l:
                continue
            m = re.match(r'^(\w+(?:\+[0-9A-Fa-f]+)?):$', l)
            if m:
                va = self.site(m.group(1))
                if va is None:
                    err('%s: patch site %s unknown' % (self.fname, m.group(1)))
                    continue
                cur = {'ref': m.group(1), 'va': va, 'kind': None, 'target': None, 'imm': 0, 'len': 0}
                self.patches.append(cur)
                continue
            m = re.match(r'^(jmp|call)\s+(\w+)$', l)
            if m and cur and cur['kind'] is None:
                if m.group(2) not in self.clabels:
                    err('%s: patch %s targets %s (not a code label)' % (self.fname, cur['ref'], m.group(2)))
                cur['kind'] = 'JMP' if m.group(1) == 'jmp' else 'CALL'
                cur['target'] = m.group(2)
                cur['len'] = 5
                continue
            if l == 'nop' and cur and cur['kind'] in ('JMP', 'CALL'):
                cur['len'] += 1
                continue
            m = re.match(r'^dd\s+([0-9A-Fa-f]+)$', l)
            if m and cur and cur['kind'] is None:
                cur['kind'] = 'IMM32'
                cur['imm'] = num(m.group(1))
                cur['len'] = 4
                continue
            err('%s: unexpected patch line %r' % (self.fname, l))
        rest = {}
        for m in re.finditer(r'^(\w+(?:\+[0-9A-Fa-f]+)?):\s*\ndb ([0-9A-Fa-f ]+)$', self.dis, re.M):
            rest[m.group(1)] = bytes(num(x) for x in m.group(2).split())
        for p in self.patches:
            r = rest.pop(p['ref'], None)
            if r is None:
                err('%s: no [DISABLE] restore for %s' % (self.fname, p['ref']))
                continue
            p['stock'] = r
            if len(r) != p['len']:
                err('%s: %s patch is %d bytes, its restore %d' % (self.fname, p['ref'], p['len'], len(r)))
            if exe.read(p['va'], len(r)) != r:
                err('%s: %s (%08X) restore %s differs from the exe %s' % (self.fname, p['ref'], p['va'], r.hex(),
                                                                          exe.read(p['va'], len(r)).hex()))
        if rest:
            err('%s: restores without a patch: %s' % (self.fname, ', '.join(rest)))

    # ---------------------------------------------------------- translation
    def dsym(self, name):
        if name in self.dlabel:
            return 'g_%sMem+0x%X' % (self.tag, self.dlabel[name])
        if name in self.allocs and name != self.tag + '_mem':
            return 'g_%sBuf' % self.tag if name == self.tag + '_buf' else None
        if name in EXTERN:
            return EXTERN[name]
        return None

    def mem(self, inner, size, other):
        terms = re.findall(r'[+-]?[^+-]+', inner.replace(' ', ''))
        regs, syms, out = False, False, []
        for t in terms:
            sign = ''
            if t[0] in '+-':
                sign, t = t[0], t[1:]
            if '*' in t:
                r, s = t.split('*')
                if r.lower() not in R32:
                    err('%s: bad scaled term %r' % (self.fname, t))
                out.append(sign + r + '*' + str(int(s, 16)))
                regs = True
            elif t.lower() in REGS:
                out.append(sign + t)
                regs = True
            elif HEXRE.match(t):
                out.append(sign + '0x%X' % num(t))
            else:
                s = self.dsym(t)
                if s is None:
                    err('%s: unknown memory symbol %r' % (self.fname, t))
                    s = t
                out.append(sign + s)
                syms = True
        body = ''
        for k, o in enumerate(out):
            if o[0] in '+-':
                body += o if k else o.lstrip('+')
            else:
                body += ('+' if k else '') + o
        if not size:
            o = (other or '').lower()
            size = 'dword' if o in R32 else 'word' if o in R16 else 'byte' if o in R8 else ''
        pre = (size + ' ptr ') if size else ''
        if not regs and not syms:
            if not pre:
                err('%s: absolute operand [%s] without a size' % (self.fname, inner))
            return pre + 'ds:[' + body + ']'
        return pre + '[' + body + ']'

    def operand(self, s, other):
        m = re.match(r'^(?:(byte|word|dword|qword)\s+ptr\s+)?\[(.*)\]$', s, re.I)
        if m:
            return self.mem(m.group(2), m.group(1), other)
        if s.lower() in REGS:
            return s
        if HEXRE.match(s):
            return '0x%X' % num(s)
        return None

    def absref(self, tok):
        if HEXRE.match(tok):
            return num(tok)
        va = self.site(tok)
        return va

    def translate(self):
        out, absn = [], set()
        for raw in self.code:
            l, c = strip_comment(raw)
            l = l.strip()
            cm = ('  // ' + c) if c else ''
            if not l:
                if c:
                    out.append('        // ' + c)
                continue
            m = re.match(r'^(\w+):$', l)
            if m:
                out.append('    %s:%s' % (m.group(1), cm))
                continue
            parts = l.split(None, 1)
            mn = parts[0].lower()
            ops = split_ops(parts[1]) if len(parts) > 1 else []
            if mn in ('jmp', 'call') or (mn.startswith('j') and len(ops) == 1):
                t = ops[0]
                if t in self.clabels:
                    out.append('        %s %s%s' % (mn, t, cm))
                    continue
                if t.lower() in R32:
                    out.append('        %s %s%s' % (mn, t, cm))
                    continue
                va = self.absref(t)
                if va is None or mn not in ('jmp', 'call'):
                    err('%s: branch %r' % (self.fname, l))
                    continue
                absn.add(va)
                out.append('        %s dword ptr [kInvA_%08X]%s' % (mn, va, cm))
                continue
            if mn == 'mov' and len(ops) == 2 and ops[0].lower() in R32:
                m2 = re.match(r'^(\w+)(?:\+([0-9A-Fa-f]+))?$', ops[1])
                if m2 and not HEXRE.match(ops[1]) and self.dsym(m2.group(1)):
                    s = self.dsym(m2.group(1)) + ('+0x%X' % num(m2.group(2)) if m2.group(2) else '')
                    out.append('        lea %s, [%s]%s' % (ops[0], s, cm))
                    continue
            new = []
            for k, o in enumerate(ops):
                other = ops[1 - k] if len(ops) == 2 else None
                t = self.operand(o, other)
                if t is None:
                    err('%s: operand %r in %r' % (self.fname, o, l))
                    t = o
                new.append(t)
            out.append('        %s%s%s' % (mn, (' ' + ', '.join(new)) if new else '', cm))
        self.absn = absn
        return out


# ------------------------------------------------------------------ CT refusal check
def ct_refusal(exe, scripts):
    img = bytearray(exe.text)
    for s in scripts:
        for p in s.patches:
            if 'stock' not in p:
                continue
            o = p['va'] - exe.tva
            if p['kind'] == 'IMM32':
                img[o:o + 4] = struct.pack('<I', p['imm'])
            else:
                b = bytes([0xE9 if p['kind'] == 'JMP' else 0xE8]) + struct.pack('<I', 0x7F000000 - p['va'] - 5)
                img[o:o + p['len']] = b + b'\x90' * (p['len'] - 5)
    rep = []
    for s in scripts:
        gone = [n for n, (va, pat) in s.aob.items() if len(scan(img, exe.tva, pat)) == 0]
        if not gone:
            err('%s: with Refined\'s bytes in, every aobscan of the cheat table\'s script still matches' % s.fname)
        rep.append((s, gone))
    return rep


def emit(scripts, exe, rep):
    L = []
    w = L.append
    w('// ================================================================ v3.37 INV337: GENERATED by tools/inv337_gen.py')
    w('// Do not edit: change tools/inv337/*.cea and run `python -I tools/inv337_gen.py` (--check verifies). The sources are')
    w('// Refined\'s copies of the cheat table\'s Extend Toolbelt v6 (CT 7310) and Stackable Items v3 (CT 7360); diff them')
    w('// with tools/inv337/ct7310_pockets_v6_marker.cea / ct7360_stackable_v3.cea for every change. Exe md5 %s.' % exe.md5)
    w('// Included by inventory.inc (g_invTbLive / g_invStOn come from there). CE numbers are hex (0x... here).')
    w('// With Refined\'s bytes in, these cheat-table aobscans no longer match (Cheat Engine refuses the script):')
    for s, gone in rep:
        w('//   %s: %s' % (s.fname, ', '.join(sorted(gone))))
    w('')
    w('typedef struct { DWORD va; BYTE len, kind; WORD label; DWORD imm; BYTE stock[12]; const char *ref; } InvSite;')
    w('enum { INV_JMP, INV_CALL, INV_IMM32 };')
    bodies = {s.tag: s.translate() for s in scripts}
    absall = set()
    for s in scripts:
        absall |= s.absn
    for va in sorted(absall):
        w('static const DWORD kInvA_%08X = 0x%08X;' % (va, va))
    for s in scripts:
        T = s.tag.upper()
        body = bodies[s.tag]
        w('')
        w('// ---------------------------------------------------------------- %s: %s' % (s.tag, s.title))
        w('// data block %s_mem (%d bytes; the labels are offsets into it)' % (s.tag, len(s.blob)))
        w('static __declspec(align(16)) BYTE g_%sMem[%d] = {' % (s.tag, len(s.blob)))
        for k in range(0, len(s.blob), 16):
            w('    ' + ', '.join('0x%02X' % b for b in s.blob[k:k + 16]) + ',')
        w('};')
        if s.tag + '_buf' in s.allocs:
            w('static __declspec(align(16)) BYTE g_%sBuf[%d];' % (s.tag, s.allocs[s.tag + '_buf']))
        for name, off in sorted(s.dlabel.items(), key=lambda x: x[1]):
            w('#define INV_%s (*(volatile LONG *)(g_%sMem + 0x%X))  // %s' % (name.upper(), s.tag, off, name))
        w('enum {')
        for k, lb in enumerate(s.clabels):
            w('    %sL_%s,' % (T, lb))
        w('    %sL_N };' % T)
        w('static DWORD g_%sLbl[%sL_N];  // the code labels\' addresses (%sCave fills them)' % (s.tag, T, T.capitalize()))
        w('static __declspec(naked) void Inv%sCave(void)' % T.capitalize())
        w('{')
        w('    __asm {')
        w('        // called once (InvInstall): every label\'s address into g_%sLbl, then ret; the cave code follows' % s.tag)
        for k, lb in enumerate(s.clabels):
            w('        mov eax, offset %s' % lb)
            w('        mov dword ptr [g_%sLbl + %d], eax' % (s.tag, 4 * k))
        w('        ret')
        L.extend(body)
        w('    }')
        w('}')
        w('static const InvSite k%sSites[] = {' % T.capitalize())
        for p in s.patches:
            st = ', '.join('0x%02X' % b for b in p.get('stock', b''))
            lab = ('%sL_%s' % (T, p['target'])) if p['target'] else '0'
            w('    { 0x%08X, %d, INV_%s, %s, 0x%X, { %s }, "%s" },' % (p['va'], p['len'], p['kind'], lab, p['imm'], st, p['ref']))
        w('};')
        w('#define INV_%s_SITES %d' % (T, len(s.patches)))
    return '\n'.join(L) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=EXE)
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    exe = Exe(a.exe)
    if not exe.md5.startswith('15a85581'):
        print('not the stock GOG v7 exe: md5 %s' % exe.md5)
        return 1
    scripts = [Script(t, f, ti, exe) for t, f, ti in SCRIPTS]
    rep = ct_refusal(exe, scripts)
    text = emit(scripts, exe, rep)
    if ERR:
        for e in ERR:
            print('ERROR', e)
        return 1
    for s in scripts:
        print('%s: %d aobs, %d code labels, %d patch sites, data %d bytes' % (s.fname, len(s.aob), len(s.clabels),
                                                                          len(s.patches), len(s.blob)))
        for p in s.patches:
            print('  %08X %-14s %-5s %2d  stock %s%s' % (p['va'], p['ref'], p['kind'], p['len'], p['stock'].hex(' ').upper(),
                                                   ('  -> ' + p['target']) if p['target'] else '  = 0x%X' % p['imm']))
    for s, gone in rep:
        print('%s: CT aobs that fail with Refined\'s bytes: %s' % (s.fname, ', '.join(sorted(gone))))
    rd = exe.read(0x5BF100, 0x200)
    rtext = ('// v3.37 INV337: GENERATED by tools/inv337_gen.py: the exe\'s item tables 0x5BF100..0x5BF2FF (md5 %s)\n'
             '// (5BF158 units, 5BF188 breakables, 5BF198 new condition, 5BF1D0 wear, 5BF220 sort rank, 5BF298 broken successor)\n'
             'static const unsigned char kInvRdata5BF100[0x200] = {\n' % exe.md5)
    for k in range(0, len(rd), 16):
        rtext += '    ' + ', '.join('0x%02X' % b for b in rd[k:k + 16]) + ',\n'
    rtext += '};\n'
    outs = [(OUT, text), (OUT_RDATA, rtext)]
    if a.check:
        bad = 0
        for path, body in outs:
            cur = open(path, encoding='utf-8').read() if os.path.exists(path) else ''
            if cur != body:
                print('CHECK FAILED: %s is not the generator\'s output (run without --check)' % path)
                bad = 1
        if not bad:
            print('check: src/inventory_cave.inc and test/inventory_rdata.h are up to date')
        return bad
    for path, body in outs:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(body)
        print('wrote', path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
