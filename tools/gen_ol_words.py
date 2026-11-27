r"""v3.53 OL1 (SH4 Refined, Options layout round R1): writes src\ol_words.h, every Options text of the new layout.

The texts come from the plan's data, staging\options_layout\tools\layout_data.py (PLAN.md section 5), byte for byte;
EXTRA below adds the R1 texts the plan leaves to the build (page titles before the moves, the combo rows' Enter part,
the Back lines of today's parents, the shortened state lines). The header holds:
  - one #define per text (OLW_<PAGE>_<ROW>_L label, _D row line, _V_<VALUE> value, _VD_<VALUE> value line);
  - OL_WORDS_LIST(X): every text with its kind, for test\ol_words_test.c (the plan's limits);
  - kOlMap / OlShow / OlIni: the screen words that differ from the ini words (PLAN section 7), for the round trip.
Usage: py -3 tools\gen_ol_words.py   (from the sandbox or the real tree; layout_data.py is found from the tree root)
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # the sandbox or the real tree
OUT = os.path.join(ROOT, 'src', 'ol_words.h')


def find_layout():
    d = ROOT
    for _ in range(6):
        p = os.path.join(d, 'staging', 'options_layout', 'tools')
        if os.path.isfile(os.path.join(p, 'layout_data.py')):
            return p
        d = os.path.dirname(d)
    raise SystemExit('layout_data.py not found above ' + ROOT)


sys.path.insert(0, find_layout())
import layout_data as L  # noqa: E402

MAX = {'L': 18, 'V': 13, 'D': 52, 'T': 28, 'S': 52}  # label, value, description, title, an added sentence (PLAN section 3)
# PLAN section 3: delayed settings end with "Next room.". The plan's Randomizer value lines drop it "to stay one line";
# every one of them still fits 52 with it, so R1 keeps the delay on screen: the _VDT_ / _DT variants (lint-checked)
TAG = ' Next room.'
TAGGED = set()  # v3.56 RI356: was {'rando'}; the user (v3.55 QA, improvement 11): no "Next room." on the Randomizer page

# (name, kind, text): the R1 texts (the rows where they are today; PLAN section 9, R1)
EXTRA = [
    # page titles (w3 OL2 draws the breadcrumbs and the strip: these are the plain names)
    ('OLW_T_DISPLAY', 'T', 'Display'), ('OLW_T_CONTROLS', 'T', 'Controls'), ('OLW_T_INTERFACE', 'T', 'Interface'),
    ('OLW_T_PRESENTATION', 'T', 'Presentation'), ('OLW_T_GAMEPLAY', 'T', 'Gameplay'), ('OLW_T_PAUSE', 'T', 'Pause Menu'),
    ('OLW_T_SUBTITLES', 'T', 'Subtitles'), ('OLW_T_EFFECTS', 'T', 'Effects'), ('OLW_T_SHADOWS', 'T', 'Shadows'),
    ('OLW_T_ROOM302', 'T', 'Room 302'), ('OLW_T_PLAYER', 'T', 'Player'), ('OLW_T_ENEMIES', 'T', 'Enemies'),
    ('OLW_T_POWERS', 'T', 'Powers'), ('OLW_T_RANDO', 'T', 'Randomizer'), ('OLW_T_SOUND', 'T', 'Sound'),
    # Back lines of today's parents
    ('OLW_BACK', 'L', 'Back'),
    ('OLW_BACK_DISPLAY', 'D', 'Back to Display.'), ('OLW_BACK_INTERFACE', 'D', 'Back to Interface.'),
    ('OLW_BACK_PRESENTATION', 'D', 'Back to Presentation.'), ('OLW_BACK_GAMEPLAY', 'D', 'Back to Gameplay.'),
    # rows whose plan name needs its folder: kept or prefixed while they sit on today's pages
    ('OLW_L_FOV_3RD', 'L', '3rd Person FOV'), ('OLW_L_FOV_1ST', 'L', '1st Person FOV'),
    ('OLW_L_GRAIN_STRENGTH', 'L', 'Grain Strength'),
    ('OLW_L_FILM_GRAIN', 'L', 'Film Grain'), ('OLW_L_SHADOWS', 'L', 'Shadows'), ('OLW_L_SUBTITLES', 'L', 'Subtitles'),
    # the combo rows (a value AND Enter opens a page; R5 makes them pure folders)
    ('OLW_COMBO_SUB_OFF', 'D', 'Off: no subtitles. Enter: their look.'),
    ('OLW_COMBO_SUB_ON', 'D', 'On: subtitles in scenes and talk. Enter: their look.'),
    ('OLW_COMBO_GRAIN', 'D', 'Grain on or off. Enter: the Effects page.'),
    ('OLW_COMBO_SHADOWS', 'D', 'Real-time shadows on or off. Enter: edge, darkness.'),
    ('OLW_SND_ENTER', 'S', ' Enter: the Sound page.'),
    ('OLW_CAM_SETTINGS', 'S', ' Settings: pause > Extra Menu > Camera.'),
    # state lines (one line each)
    ('OLW_NA_GAME', 'D', 'Not available in this game. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_NA_SESSION', 'D', 'Not available this session. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_OFF_SESSION', 'D', 'Off this session. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_SND_PAN_STOCK', 'D', "The game's own steep pan."),  # v3.57 OLP1
    ('OLW_SND_PAN_NA', 'D', "The game's own steep pan. See SH4Refined.log."),  # v3.57 OLP1
    ('OLW_SND_WIDTH_STOCK', 'D', "No width with the game's own pan."),  # v3.57 OLP1
    ('OLW_SND_REV_NOPROBE', 'D', 'No reverb on this sound device. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_SND_DEVICE_NEW', 'D', 'New sound device: restart the game to use it.'),  # v3.56 SD356 (Sound Mode's line)
    ('OLW_FONT_MISSING', 'D', 'Font not loaded: stock shown. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_FONT_FAILED', 'D', 'HD font failed: next start. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_REND_16BIT', 'D', '16-bit Direct3D 11: more lag, AMD only. Next start.'),
    ('OLW_REND_OTHER', 'D', 'An SH4 Graphics Plus renderer. Next start.'),
    ('OLW_REND_TEST', 'D', 'A Graphics Plus test renderer. Next start.'),  # v3.57 OLP1
    ('OLW_FPS_60_FM1', 'D', '60: a smoother picture; the game stays at 30.'),
    ('OLW_R302_FOLDER_PE', 'D', 'Playable Eileen sets these for now.'),
    ('OLW_R302_NA', 'D', 'Not available. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_CAM_PE', 'D', "Playable Eileen's camera menu has it."),
    ('OLW_CAM_V_PE', 'V', 'Eileen'), ('OLW_CAM_V_NA', 'V', 'Unavailable'),
    ('OLW_REGEN_PE_LEGACY', 'D', 'Playable Eileen heals after %d seconds unhurt.'),  # v3.57 OLP1 (gameplay.inc GpSnapshot uses it now)
    ('OLW_FADEIN_NA', 'D', 'Not available. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_FADEIN_V_VORTEX', 'D', 'Portal: fade in; ghosts rise from a blood portal.'),
    ('OLW_TELEPORT_ON_CD', 'D', 'On: behind the enemy you face, %g seconds apart.'),  # v3.57 OLP1
    ('OLW_RANDO_CT', 'D', "Off: the cheat table's randomizer is on."),
    ('OLW_PP_NOT_FILTER', 'D', 'Not with this Filter.'),
    ('OLW_PP_NOT_SPIRIT', 'D', 'Not with Spirit Vision.'),
    ('OLW_PP_NORESET', 'D', "Not available: the game's reset code is not stock."),
    ('OLW_SHD_EDGE_NOPREVIEW', 'D', "The shadows' edge: Stock or Soft."),
    ('OLW_SHD_EDGE_FAILED', 'D', 'Off this session: Stock. See SH4Refined.log.'),  # v3.57 OLP1
    ('OLW_SHD_DARK_NOPREVIEW', 'D', 'How dark shadows are.'),
    ('OLW_FILTER_F7', 'S', ' Press F%d in play for a new preview.'),  # v3.57 OLP1 (was ' F%d in play: new preview.'): the Filter
    # lines + it are two lines now (descfit wraps; the longest, Old Film's, 79 characters with F12); g_txtFxD 64 -> 80
    ('OLW_OMNI_PE', 'D', 'On: any angle (Eileen keeps her own speed).'),
    ('OLW_OMNI_BIND', 'D', 'Movement is not on a stick: no effect.'),
    ('OLW_OMNI_NOSRC', 'D', 'The stick could not be read: no effect.'),
    # Extra Enemies gets its "Next room." tag (its note)  # v3.57 OLP1: OLW_PLAYER_FOLDER_R1_D retired (the Player folder
    # line is OLW_GAMEPLAY_PLAYER_D now: Codex OLP1-002)
    ('OLW_EXTRA_ENEMIES_R1_D', 'D', 'More of every monster and ghost. Next room.'),
    ('OLW_RESERVED', 'V', ''),
]
# v3.54 OL34 (rounds R3 + R4): the texts of the moved layout that the plan leaves to the build: the new pages' titles (the
# leaf; look.inc draws "Parent > Page"), the Back lines of the new parents, and the interim rows of PLAN section 4's
# fallbacks (stock rows stay on their stock pages until R5: Interface 13 -> 11 puts Menu Font and Start-Up into the Pause
# Menu folder, renamed Menus; the Film Grain / Shadows switches stay on Interface and point at their Effects folders)
EXTRA34 = [
    ('OLW_T_CAMERA', 'T', 'Camera'), ('OLW_T_STEERING', 'T', 'Steering'), ('OLW_T_COMBAT', 'T', 'Combat'),
    ('OLW_T_ITEMS', 'T', 'Items'), ('OLW_T_GRAIN', 'T', 'Film Grain'), ('OLW_T_MENUS', 'T', 'Menus'),
    ('OLW_BACK_CONTROLS', 'D', 'Back to Controls.'), ('OLW_BACK_EFFECTS', 'D', 'Back to Effects.'),
    ('OLW_BACK_CAMERA', 'D', 'Back to Camera.'),
    ('OLW_MENUS_L', 'L', 'Menus'), ('OLW_MENUS_D', 'D', 'The pause list, the menu font and the start-up.'),
    ('OLW_GRAIN_SWITCH_D', 'D', 'Grain on or off. Settings: Effects > Film Grain.'),
    ('OLW_SHADOWS_SWITCH_D', 'D', 'Real-time shadows. Settings: Effects > Shadows.'),
]
EXTRA += EXTRA34
KIND_CHAR = {'value': 'v', 'bar': 'b', 'folder': 'f', 'combo': 'c', 'screen': 's', 'action': 'a', 'info': 'i'}


def ident(s):
    s = re.sub(r'[^A-Za-z0-9]+', '_', s).strip('_').upper()
    return s


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def fmt_len(s):
    # the longest a format can print: %d -> 4 digits (F12, a delay up to 120), %g -> 4 characters
    return len(s.replace('%d', '0000').replace('%g', '0000').replace('%%', '%'))


def main():
    defs, words, maps, seen = [], [], [], {}

    def add(name, kind, text, src):
        if name in seen:
            if seen[name] != text:
                raise SystemExit('two texts for %s: %r / %r' % (name, seen[name], text))
            return
        seen[name] = text
        defs.append((name, text, src))
        words.append((kind, name))

    for p in L.PAGES:
        pg = ident(p['id'])
        for r in p['rows']:
            if r['kind'] in ('back', 'exit'):
                continue
            rn = pg + '_' + ident(r['name'])
            tag = p['id'] in TAGGED and r['kind'] == 'value'
            add('OLW_%s_L' % rn, 'L', r['name'], '%s / %s' % (p['title'], r['name']))
            if r['desc']:
                add('OLW_%s_D' % rn, 'D', r['desc'], '%s / %s' % (p['title'], r['name']))
                if tag and TAG not in r['desc']:
                    add('OLW_%s_DT' % rn, 'D', r['desc'] + TAG, 'R1: %s / %s + the tag' % (p['title'], r['name']))
            mapped = []
            for v in r['values']:
                vi = ident(v['label'])
                if not vi:
                    continue
                if r['kind'] == 'value':
                    add('OLW_%s_V_%s' % (rn, vi), 'V', v['label'], '%s / %s' % (p['title'], r['name']))
                if v['desc']:
                    add('OLW_%s_VD_%s' % (rn, vi), 'D', v['desc'], '%s / %s / %s' % (p['title'], r['name'], v['label']))
                    if tag and TAG not in v['desc']:
                        add('OLW_%s_VDT_%s' % (rn, vi), 'D', v['desc'] + TAG, 'R1: %s / %s / %s + the tag' % (p['title'], r['name'], v['label']))
                if v['ini'] and v['ini'] != v['label']:
                    mapped.append((v['ini'], v['label']))
            if mapped:
                maps.append(('OLM_' + rn, '%s > %s' % (p['title'], r['name']), r['ini'], mapped))
    r4 = set(n for n, _, _ in EXTRA34)
    for name, kind, text in EXTRA:
        add(name, kind, text, 'R4' if name in r4 else 'R1')  # v3.54 OL34: its own texts say R4

    bad = []
    for kind, name in words:
        n = fmt_len(seen[name])
        if n > MAX[kind]:
            bad.append('%s (%s) %d > %d: %s' % (name, kind, n, MAX[kind], seen[name]))
    for b in bad:
        print('LINT:', b)

    out = []
    out.append('// v3.53 OL1: GENERATED by tools\\gen_ol_words.py from staging\\options_layout\\tools\\layout_data.py (PLAN.md')
    out.append('// section 5, byte for byte) plus its EXTRA list (the R1 texts). Do not edit by hand: change the data, run it again.')
    out.append('// Limits (PLAN section 3): labels 18, values 13, descriptions 52 (one line), titles 28; test\\ol_words_test.c checks them.')
    out.append('#ifndef OL_WORDS_H')
    out.append('#define OL_WORDS_H')
    out.append('')
    for name, text, src in defs:
        out.append('#define %-44s %s  /* %s */' % (name, cstr(text), src))
    out.append('')
    out.append('// every text with its kind (L label, V value, D description, T title, S a sentence added to a description)')
    out.append('#define OL_WORDS_LIST(X) \\')
    for kind, name in words:
        out.append("    X('%s', %s, %s) \\" % (kind, name, cstr(name)))
    out.append('')
    out.append('// the screen words that differ from the ini words (PLAN section 7): the row shows the screen word, the ini keeps its')
    out.append('// own; a value outside a map shows as it is written')
    out.append('enum {')
    for m in maps:
        out.append('    %s,  /* %s: %s */' % (m[0], m[1], m[2]))
    out.append('    OLM_N };')
    out.append('typedef struct { int map; const char *ini, *show; } OlMapEntry;')
    # v3.54 OL34: the plan's pages and rows (layout_data.py PAGES / TOP), for test\layout_sim_test.c (the row placement)
    out.append('// v3.54 OL34: the plan\'s pages (id, title, parent id; "" = a top page), its page cycle and every row but Back / Exit')
    out.append('// (page id, row name, kind: v value, b bar, f folder, c combo, s screen, a action, i info; v3.55 OL56: + the page id a')
    out.append('// folder opens, "" for the others), in the plan\'s order')
    out.append('#define OL_PLAN_PAGES(X) \\')
    for p in L.PAGES:
        out.append('    X(%s, %s, %s) \\' % (cstr(p['id']), cstr(p['title']), cstr(p['parent'] or '')))
    out.append('')
    out.append('#define OL_PLAN_TOP(X) \\')
    for t in L.TOP:
        out.append('    X(%s) \\' % cstr(t))
    out.append('')
    out.append('#define OL_PLAN_ROWS(X) \\')
    for p in L.PAGES:
        for r in p['rows']:
            if r['kind'] in ('back', 'exit'):
                continue
            out.append("    X(%s, %s, '%s', %s) \\" % (cstr(p['id']), cstr(r['name']), KIND_CHAR[r['kind']],
                                                    cstr(r['opens'] or '')))  # v3.55 OL56: + the page a folder opens
    out.append('')
    # v3.56 MN356: every value line of the plan by its row (USER_FEEDBACK_v355 issue 3: a value must never show the row's
    # line); src\vlines.inc answers these for a row whose own code still returns the row's line for that value
    out.append('// v3.56 MN356: the value lines by row (page id, row name, the row\'s line, the value\'s screen word, the value\'s line), for')
    out.append('// src\\vlines.inc (a value whose row code still answers the row\'s line gets its own) and test\\vlines_test.c')
    out.append('#define OL_PLAN_VLINES(X) \\')
    for p in L.PAGES:
        pg = ident(p['id'])
        for r in p['rows']:
            if r['kind'] != 'value' or not r['desc']:
                continue
            rn = pg + '_' + ident(r['name'])
            for v in r['values']:
                vi = ident(v['label'])
                if not vi or not v['desc']:
                    continue
                out.append('    X(%s, %s, OLW_%s_D, OLW_%s_V_%s, OLW_%s_VD_%s) \\' % (cstr(p['id']), cstr(r['name']), rn, rn, vi, rn, vi))
    out.append('')
    out.append('static const OlMapEntry kOlMap[] = {')
    for m in maps:
        for ini, show in m[3]:
            out.append('    { %s, %s, %s },' % (m[0], cstr(ini), cstr(show)))
    out.append('};')
    out.append('#define OL_MAP_N ((int)(sizeof kOlMap / sizeof kOlMap[0]))')
    out.append('static int OlSame(const char *a, const char *b)  // case-blind, as the ini reads (GetPrivateProfileString + _stricmp)')
    out.append('{')
    out.append('    for (; *a && *b; ++a, ++b) {')
    out.append("        const int x = *a >= 'a' && *a <= 'z' ? *a - 32 : *a, y = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;")
    out.append('        if (x != y) return 0;')
    out.append('    }')
    out.append('    return *a == *b;')
    out.append('}')
    out.append('static const char *OlShow(int map, const char *ini)  // the word the row shows for an ini word')
    out.append('{')
    out.append('    int i;')
    out.append('    if (!ini) return ini;')
    out.append('    for (i = 0; i < OL_MAP_N; ++i)')
    out.append('        if (kOlMap[i].map == map && OlSame(kOlMap[i].ini, ini)) return kOlMap[i].show;')
    out.append('    return ini;')
    out.append('}')
    out.append('static const char *OlIni(int map, const char *show)  // the ini word a shown word stands for')
    out.append('{')
    out.append('    int i;')
    out.append('    if (!show) return show;')
    out.append('    for (i = 0; i < OL_MAP_N; ++i)')
    out.append('        if (kOlMap[i].map == map && OlSame(kOlMap[i].show, show)) return kOlMap[i].ini;')
    out.append('    return show;')
    out.append('}')
    out.append('')
    out.append('#endif')
    text = '\n'.join(out) + '\n'  # LF, as the other src files
    with open(OUT, 'w', encoding='ascii', newline='') as f:
        f.write(text)
    print('wrote %s: %d texts, %d maps (%d entries), lint %d' % (OUT, len(words), len(maps), sum(len(m[3]) for m in maps), len(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
