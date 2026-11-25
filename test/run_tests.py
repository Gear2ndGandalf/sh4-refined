"""run_tests.py: offline tests of the SH4 Window Mod (winmm.dll) and of its split from the Playable Eileen plugin.

    python run_tests.py [--plugin <new dsound.dll>] [--old-plugin <v0.81 dsound.dll>] [--ini <user's SH4PlayableEileen.ini>]

Builds two 32-bit harnesses (MSVC 2022) into test\\work, then runs scenarios, each in its own folder holding a harness
copied as "SILENT HILL 4.exe" (so the mod treats it as the game) and the dlls of that scenario. The real game is never
started and the game folder is never touched. Writes test\\results.txt; exit 0 when everything passed.
"""
import argparse, hashlib, os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PE = os.environ.get('SH4_PLAYABLE_EILEEN', 'SH4PlayableEileen')  # legacy v1.x split test: needs Playable Eileen's builds
WORK = os.path.join(HERE, 'work')
VCVARS = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat'
PY = sys.executable
SPLIT = os.path.join(PE, 'staging', 'winsplit', 'split_settings.py')
OUT = []
FAILS = []


def say(*a):
    s = ' '.join(str(x) for x in a)
    OUT.append(s)
    print(s)


def check(cond, msg):
    say(('ok    ' if cond else 'FAIL  ') + msg)
    if not cond:
        FAILS.append(msg)


def md5(p):
    return hashlib.md5(open(p, 'rb').read()).hexdigest()


def build(name, src, defs='', libs='', extra=''):
    exe = os.path.join(WORK, name + '.exe')
    cmd = os.path.join(WORK, 'build_%s.cmd' % name)
    open(cmd, 'w').write('@echo off\r\ncall "%s" >nul\r\ncd /d "%s"\r\ncl /nologo /W3 /RTC1 /MTd /Zi %s "%s" /Fo"%s\\\\" /Fd"%s\\\\%s.pdb" '
                         '/Fe"%s" /link /NOLOGO %s %s\r\n' % (VCVARS, WORK, defs, src, WORK, WORK, name, exe, extra, libs))
    r = subprocess.run(['cmd.exe', '/c', cmd], capture_output=True, text=True)
    if r.returncode or not os.path.exists(exe):
        print(r.stdout, r.stderr)
        raise SystemExit('build of %s failed' % name)
    return exe


def run(folder, args, timeout=60):
    r = subprocess.run([os.path.join(folder, 'SILENT HILL 4.exe')] + args, cwd=folder, capture_output=True, text=True, timeout=timeout)
    for l in r.stdout.splitlines():
        say('      | ' + l)
    return r.returncode


def scenario(name, exe, dlls, files=None):
    d = os.path.join(WORK, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    shutil.copy(exe, os.path.join(d, 'SILENT HILL 4.exe'))
    for target, src in dlls.items():
        shutil.copy(src, os.path.join(d, target))
    for target, data in (files or {}).items():
        p = os.path.join(d, target)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(data)
    say('')
    say('== %s (%s)' % (name, ', '.join(sorted(list(dlls) + list(files or {})))))
    return d


def read(p):
    return open(p, 'rb').read() if os.path.exists(p) else None


def log_has(d, logname, pattern):
    """the log of the last start or of the one before (.prev.log)"""
    for name in (logname, logname.replace('.log', '.prev.log')):
        t = read(os.path.join(d, name))
        if t is not None and re.search(pattern, t.decode('latin-1')) is not None:
            return True
    return False


def split_reference(ini_bytes, tag):
    """split_settings.py's two outputs for this input (the reference the dlls must match byte for byte)"""
    d = os.path.join(WORK, 'ref_' + tag)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    src = os.path.join(d, 'in.ini')
    open(src, 'wb').write(ini_bytes)
    r = subprocess.run([PY, SPLIT, src, os.path.join(d, 'out')], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    return read(os.path.join(d, 'out', 'SH4PlayableEileen.ini')), read(os.path.join(d, 'out', 'SH4WindowMod.ini'))


def import_order(exe):
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    from check_exports import import_dlls
    return [x.upper() for x in import_dlls(exe)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--proxy', default=os.path.join(ROOT, 'out', 'winmm.dll'))
    ap.add_argument('--plugin', default=os.path.join(PE, 'staging', 'winsplit', 'out', 'dsound.dll'))
    ap.add_argument('--old-plugin', default=os.path.join(PE, 'release', '2026-10-26_v0.81', 'install', 'dsound.dll'))
    ap.add_argument('--ini', default=os.path.join(PE, 'release', '2026-10-26_v0.81', 'install', 'SH4PlayableEileen.ini.my_settings'))
    a = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    say('proxy %s md5 %s; plugin %s md5 %s; old plugin %s md5 %s' % (a.proxy, md5(a.proxy), a.plugin, md5(a.plugin), a.old_plugin,
                                                                   md5(a.old_plugin)))
    fwd = build('harness_forward', os.path.join(HERE, 'harness_forward.c'))
    sdk = 'winmm.lib'
    base = '/BASE:0x01000000 /DYNAMICBASE:NO /FIXED'
    game = build('harness_game', os.path.join(HERE, 'harness_game.c'), libs=sdk, extra=base)
    game_wm1 = build('harness_game_wm_first', os.path.join(HERE, 'harness_game.c'), defs='/DWITH_DSOUND', libs='winmm.lib dsound.lib', extra=base)
    game_ds1 = build('harness_game_ds_first', os.path.join(HERE, 'harness_game.c'), defs='/DWITH_DSOUND', libs='dsound.lib winmm.lib', extra=base)
    for e in (game, game_wm1, game_ds1):
        say('%s imports %s' % (os.path.basename(e), ', '.join(import_order(e))))

    # 1. forwarding in a non-game process
    say('')
    say('== forward (LoadLibrary of the proxy in a 32-bit test process)')
    d = os.path.join(WORK, 'forward')
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    shutil.copy(a.proxy, os.path.join(d, 'winmm.dll'))
    r = subprocess.run([fwd, os.path.join(d, 'winmm.dll'), os.path.join(ROOT, 'src', 'winmm.def')], capture_output=True, text=True, timeout=60)
    for l in r.stdout.splitlines():
        say('      | ' + l)
    check(r.returncode == 0, 'forwarding harness passed')

    user_ini = read(a.ini)
    # the user's file with other window values and an extra key, so the copy is visible
    test_ini = user_ini.replace(b'Width=784', b'Width=800').replace(b'Height=588', b'Height=600').replace(
        b'SnapToStandard=1', b'SnapToStandard=1\r\nMyNote=keep me')
    assert test_ini != user_ini
    ref_pe, ref_wm = split_reference(test_ini, 'test')

    # 2. window mod alone, first start: imports [Window] from SH4PlayableEileen.ini, leaves that file alone
    d = scenario('wm_alone_first_start', game, {'winmm.dll': a.proxy}, {'SH4PlayableEileen.ini': test_ini})
    check(run(d, ['expect-active', '800', '600']) == 0, 'window behaviour as before (harness checks)')
    check(read(os.path.join(d, 'SH4WindowMod.ini')) == ref_wm, 'SH4WindowMod.ini written with the user\'s values, byte-identical to split_settings.py')
    check(read(os.path.join(d, 'SH4PlayableEileen.ini')) == test_ini, 'SH4PlayableEileen.ini untouched by the window mod')
    check(log_has(d, 'SH4WindowMod.log', r'written with the values of SH4PlayableEileen\.ini \[Window\]'), 'log: values imported')
    check(log_has(d, 'SH4WindowMod.log', r'exe fixes not applied'), 'log: exe fix refused off the game image base (byte check path)')

    # 3./4. window mod + new plugin, both import orders, two starts each
    for tag, exe in (('wm_first', game_wm1), ('ds_first', game_ds1)):
        d = scenario('both_' + tag, exe, {'winmm.dll': a.proxy, 'dsound.dll': a.plugin}, {'SH4PlayableEileen.ini': test_ini})
        check(run(d, ['expect-active', '800', '600']) == 0, 'start 1: window behaviour as before with both dlls')
        after1 = read(os.path.join(d, 'SH4PlayableEileen.ini'))
        removed1 = after1 == ref_pe
        say('      start 1: [Window] %s from SH4PlayableEileen.ini (depends on which DllMain ran first)' % ('removed' if removed1 else 'kept'))
        check(after1 in (ref_pe, test_ini), 'start 1: SH4PlayableEileen.ini is either untouched or exactly the split')
        check(run(d, ['expect-active', '800', '600']) == 0, 'start 2: window behaviour as before')
        check(read(os.path.join(d, 'SH4PlayableEileen.ini')) == ref_pe, 'start 2: [Window] removed, file byte-identical to split_settings.py')
        check(read(os.path.join(d, 'SH4PlayableEileen.ini.before_winsplit')) == test_ini, 'copy before the removal kept (.before_winsplit)')
        check(read(os.path.join(d, 'SH4WindowMod.ini')) == ref_wm, 'SH4WindowMod.ini holds the user\'s values')
        check(log_has(d, 'SH4EileenWeapons.log', r'v0\.82 loading') and log_has(d, 'SH4EileenWeapons.log', r'\[Window\] \(8 keys, 1 section'),
              'plugin log: v0.82, [Window] (8 keys) removed')
        check(log_has(d, 'SH4WindowMod.log', r'hooks installed'), 'window mod active next to plugin v0.82')

    # 5. new plugin alone: leaves [Window] where it is (no window mod ini), no window changes
    d = scenario('plugin_alone', game_ds1, {'dsound.dll': a.plugin}, {'SH4PlayableEileen.ini': test_ini})
    check(run(d, ['expect-idle']) == 0, 'the game keeps its own (full-screen popup) window')
    check(read(os.path.join(d, 'SH4PlayableEileen.ini')) == test_ini, 'SH4PlayableEileen.ini untouched ([Window] kept: no SH4WindowMod.ini)')
    check(log_has(d, 'SH4EileenWeapons.log', r'\[Window\] left in SH4PlayableEileen\.ini.*does not exist yet'), 'plugin log says why')

    # 6. old plugin v0.81 (window.c inside) + window mod: the window mod stays a forwarder
    d = scenario('old_plugin_and_wm', game_ds1, {'winmm.dll': a.proxy, 'dsound.dll': a.old_plugin}, {'SH4PlayableEileen.ini': test_ini})
    check(run(d, ['smoke']) == 0, 'runs')
    check(log_has(d, 'SH4WindowMod.log', r'not active: the Playable Eileen plugin up to v0\.81'), 'window mod log: not active (old plugin has its own window code)')
    check(not os.path.exists(os.path.join(d, 'SH4WindowMod.ini')), 'no SH4WindowMod.ini written while idle')

    # 6b. reference: the same harness against the old plugin v0.81 alone (its window.c), to compare behaviour
    d = scenario('reference_old_plugin_window_c', game_ds1, {'dsound.dll': a.old_plugin}, {'SH4PlayableEileen.ini': test_ini})
    check(run(d, ['expect-window', '800', '600']) == 0, 'v0.81 window.c passes the same window checks (reference)')

    # 7. Enable=0: the window mod leaves the window alone
    off = ref_wm.replace(b'\r\nEnable=1\r\n', b'\r\nEnable=0\r\n')
    d = scenario('wm_enable_0', game, {'winmm.dll': a.proxy}, {'SH4WindowMod.ini': off})
    check(run(d, ['expect-idle']) == 0, 'Enable=0: the game keeps its own window')
    check(read(os.path.join(d, 'SH4WindowMod.ini')) == off, 'SH4WindowMod.ini untouched')

    # 8. laptop rig: no plugin ini, scripts\SH4Windowed.ini -> imported (ConfineMouse -> CaptureMouse)
    asi = b'[Window]\r\nWidth=1024\r\nHeight=768\r\nResizable=1\r\nKeepActiveOnAltTab=1\r\nFreeMouse=1\r\nSnapToStandard=1\r\nConfineMouse=0\r\n'
    d = scenario('wm_import_sh4windowed', game, {'winmm.dll': a.proxy}, {os.path.join('scripts', 'SH4Windowed.ini'): asi})
    check(run(d, ['expect-active', '1024', '768']) == 0, 'window behaviour with the imported 1024x768')
    t = (read(os.path.join(d, 'SH4WindowMod.ini')) or b'').decode('latin-1')
    check('Width=1024' in t and 'Height=768' in t and 'CaptureMouse=0' in t, 'SH4WindowMod.ini took Width/Height and ConfineMouse=0 as CaptureMouse=0')

    say('')
    say('%s: %d failure(s)' % ('PASSED' if not FAILS else 'FAILED', len(FAILS)))
    open(os.path.join(HERE, 'results.txt'), 'w').write('\n'.join(OUT) + '\n')
    return 1 if FAILS else 0


if __name__ == '__main__':
    sys.exit(main())
