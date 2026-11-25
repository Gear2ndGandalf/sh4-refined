r"""v3.40 LG340: end-to-end check of the session-log rotation with a built winmm.dll (standard library only).

    python test\lr_e2e.py <winmm.dll>

Builds test\lr_e2e_loader.c as "SILENT HILL 4.exe" in test\out\lr_e2e (a scratch game folder: never the real game), seeds
it with two v3.39-style logs (SH4Refined.prev.log and SH4Refined.log), starts the loader 10 times and checks logs\:
both old logs kept under their start, each start adds the session before it, the oldest go past Keep=8, a file that
is not Refined's stays, SH4Refined.prev.log is still the session before, the log has its "log: started" line. Then
[Log] Keep=0: nothing in logs\ changes. Exit 0 when every check passed.
"""
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, 'out', 'lr_e2e')
VCVARS = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
FAILS = []


def check(cond, msg):
    print(('ok    ' if cond else 'FAIL  ') + msg)
    if not cond:
        FAILS.append(msg)


def own(names):
    return sorted(n for n in names if re.match(r'^SH4Refined \d{4}-\d\d-\d\d \d{4} (v[\w.+-]+|unknown)( #[2-9])?\.log$', n))


def main(argv):
    if len(argv) != 1 or not os.path.isfile(argv[0]):
        print(__doc__)
        return 2
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    exe = os.path.join(WORK, 'SILENT HILL 4.exe')
    cmd = os.path.join(WORK, 'build.cmd')
    with open(cmd, 'w') as f:
        f.write('@echo off\r\ncall "%s" >nul\r\ncd /d "%s"\r\ncl /nologo /W3 /MT "%s" /Fo"%s\\\\" /Fe"%s" /link /NOLOGO '
                '/BASE:0x01000000 /DYNAMICBASE:NO /FIXED\r\n' % (VCVARS, WORK, os.path.join(HERE, 'lr_e2e_loader.c'), WORK, exe))
    r = subprocess.run(['cmd.exe', '/c', cmd], capture_output=True, text=True)
    if r.returncode or not os.path.exists(exe):
        print(r.stdout, r.stderr)
        return 1
    shutil.copy(argv[0], os.path.join(WORK, 'winmm.dll'))
    with open(os.path.join(WORK, 'SH4Refined.ini'), 'w') as f:
        f.write('[Window]\r\nEnable=0\r\n')
    # two v3.39 sessions, as the user's game folder had them on 2026-11-22
    for name, first, mt in (('SH4Refined.prev.log', '07:21:43.000', (2026, 11, 22, 7, 24, 18)),
                            ('SH4Refined.log', '07:24:21.162', (2026, 11, 22, 7, 26, 5))):
        p = os.path.join(WORK, name)
        with open(p, 'w', newline='\r\n') as f:
            f.write('%s SH4 Refined v3.39.0 loading (winmm.dll proxy, game base 00400000)\nbody of %s\n' % (first, name))
        t = time.mktime(mt + (0, 0, -1))
        os.utime(p, (t, t))
    logs = os.path.join(WORK, 'logs')
    os.makedirs(logs)
    with open(os.path.join(logs, 'my notes.txt'), 'w') as f:
        f.write('not Refined\'s')
    fake = 'SH4Refined 2000-01-01 0000 v1.0.log'  # v3.40 REV340 (LG340-002): named like the oldest archive, not a log
    with open(os.path.join(logs, fake), 'w') as f:
        f.write('my own notes under a name like Refined\'s')
    seen = []
    for i in range(10):
        r = subprocess.run([exe], cwd=WORK, capture_output=True, text=True, timeout=60)
        check(r.returncode == 0, 'start %d: the loader ran (%s)' % (i + 1, r.stdout.strip()))
        names = [n for n in own(os.listdir(logs)) if n != fake]
        seen.append(names)
        txt = open(os.path.join(WORK, 'SH4Refined.log'), encoding='utf-8', errors='replace').read()
        if i == 0:
            print(''.join(l + '\n' for l in txt.splitlines() if l[13:].startswith('log: ')))
            check('SH4Refined 2026-11-22 0721 v3.39.0.log' in names and 'SH4Refined 2026-11-22 0724 v3.39.0.log' in names,
                  'start 1: both v3.39 logs kept under their start (%s)' % names)
            check(re.search(r'^\S+ log: started \d{4}-\d\d-\d\d \d\d:\d\d:\d\d \(local time\)$', txt, re.M) is not None,
                  'start 1: the new log has its "log: started" line')
            check(re.search(r'log: session logs: the last 8 in logs\\ .*2 copied in, 0 older deleted', txt) is not None,
                  'start 1: the rotation line says 2 copied in')
            prev = open(os.path.join(WORK, 'SH4Refined.prev.log'), encoding='utf-8').read()
            check('body of SH4Refined.log' in prev, 'start 1: SH4Refined.prev.log is the session before (as up to v3.39)')
        time.sleep(61 if i in (2,) else 1.1)  # one gap of a minute: the rest share minutes (the " #2" names)
    final = seen[-1]
    check(len(final) == 8, 'after 10 starts: 8 of Refined\'s logs in logs\\ (%d: %s)' % (len(final), final))
    check('SH4Refined 2026-11-22 0721 v3.39.0.log' not in final, 'the oldest (07:21 v3.39) went past Keep=8')
    check(any(' #2.log' in n for n in final), 'two sessions in the same minute: a " #2" name')
    check(os.path.exists(os.path.join(logs, 'my notes.txt')), 'a file that is not Refined\'s stays')
    txt = open(os.path.join(WORK, 'SH4Refined.log'), encoding='utf-8', errors='replace').read()
    check(os.path.exists(os.path.join(logs, fake)) and 'not starting like one, left alone: ' + fake in txt,
          'v3.40 REV340: a file only named like the oldest archive stays past Keep and is named in the log')
    check(all(len(seen[k]) == min(8, 2 + k) for k in range(10)), 'each start adds the session before it (%s)' %
          [len(s) for s in seen])
    # Keep=0: as up to v3.39, logs\ left as it is
    with open(os.path.join(WORK, 'SH4Refined.ini'), 'a') as f:
        f.write('[Log]\r\nKeep=0\r\n')
    before = sorted(os.listdir(logs))
    r = subprocess.run([exe], cwd=WORK, capture_output=True, text=True, timeout=60)
    check(sorted(os.listdir(logs)) == before, '[Log] Keep=0: logs\\ unchanged')
    txt = open(os.path.join(WORK, 'SH4Refined.log'), encoding='utf-8', errors='replace').read()
    check('only SH4Refined.log and SH4Refined.prev.log' in txt and '(hidden key)' in txt, 'Keep=0: the log says so; the key is '
          'listed as a hidden key')
    print('%d failure(s)' % len(FAILS))
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
