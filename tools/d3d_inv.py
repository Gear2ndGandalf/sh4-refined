import re, sys, collections
import os
sys.path.insert(0, os.environ.get("SH4_RE_KIT", "re_kit"))  # the folder holding sh4lib.py (the author's research kit; not in this repository)
import sh4lib as L
L.load()
NAMES = """QueryInterface AddRef Release TestCooperativeLevel GetAvailableTextureMem ResourceManagerDiscardBytes GetDirect3D GetDeviceCaps GetDisplayMode GetCreationParameters SetCursorProperties SetCursorPosition ShowCursor CreateAdditionalSwapChain Reset Present GetBackBuffer GetRasterStatus SetGammaRamp GetGammaRamp CreateTexture CreateVolumeTexture CreateCubeTexture CreateVertexBuffer CreateIndexBuffer CreateRenderTarget CreateDepthStencilSurface CreateImageSurface CopyRects UpdateTexture GetFrontBuffer SetRenderTarget GetRenderTarget GetDepthStencilSurface BeginScene EndScene Clear SetTransform GetTransform MultiplyTransform SetViewport GetViewport SetMaterial GetMaterial SetLight GetLight LightEnable GetLightEnable SetClipPlane GetClipPlane SetRenderState GetRenderState BeginStateBlock EndStateBlock ApplyStateBlock CaptureStateBlock DeleteStateBlock CreateStateBlock SetClipStatus GetClipStatus GetTexture SetTexture GetTextureStageState SetTextureStageState ValidateDevice GetInfo SetPaletteEntries GetPaletteEntries SetCurrentTexturePalette GetCurrentTexturePalette DrawPrimitive DrawIndexedPrimitive DrawPrimitiveUP DrawIndexedPrimitiveUP ProcessVertices CreateVertexShader SetVertexShader GetVertexShader DeleteVertexShader SetVertexShaderConstant GetVertexShaderConstant GetVertexShaderDeclaration GetVertexShaderFunction SetStreamSource GetStreamSource SetIndices GetIndices CreatePixelShader SetPixelShader GetPixelShader DeletePixelShader SetPixelShaderConstant GetPixelShaderConstant GetPixelShaderFunction DrawRectPatch DrawTriPatch DeletePatch""".split()
assert len(NAMES) == 97
REGS = ['eax','ebx','ecx','edx','esi','edi','ebp']
mdev = re.compile(r'^mov\s+(e[a-z]{2}),dword ptr ds:\[006E7D94h\]$')
mvt = re.compile(r'^mov\s+(e[a-z]{2}),dword ptr \[(e[a-z]{2})\]$')
mcall = re.compile(r'^call\s+dword ptr \[(e[a-z]{2})(?:\+([0-9A-F]+)h)?\]$')
mwrite = re.compile(r'^(?:mov|lea|pop|xor|add|sub|and|or|imul|movzx|movsx|inc|dec|shl|shr|sar|neg|not|xchg|cdq)\s+(e[a-z]{2})\b')
sites = collections.defaultdict(list)
st = {}
devrefs = 0; used = set()
lines = L.lines()
for i,(a,t) in enumerate(lines):
    t = ' '.join(t.split()) if False else re.sub(r'\s+', ' ', t).replace(' ,', ',')
    t2 = re.sub(r'^(\S+) ', r'\1 ', t)
    m = mdev.match(re.sub(r'\s+',' ',t2).replace('mov ','mov ',1)) if False else None
    tt = re.sub(r'\s+', ' ', t)
    tt = tt.replace('mov ', 'mov ', 1)
    if tt.startswith('ret') or tt.startswith('int 3'):
        st = {}; continue
    m = re.match(r'^mov (e[a-z]{2}),dword ptr ds:\[006E7D94h\]$', tt)
    if m:
        st[m.group(1)] = ('dev', a); devrefs += 1; continue
    m = re.match(r'^mov (e[a-z]{2}),dword ptr \[(e[a-z]{2})\]$', tt)
    if m and st.get(m.group(2), ('',))[0] == 'dev':
        st[m.group(1)] = ('vt', st[m.group(2)][1]); continue
    m = re.match(r'^call dword ptr \[(e[a-z]{2})(?:\+([0-9A-F]+)h)?\]$', tt)
    if m and st.get(m.group(1), ('',))[0] == 'vt':
        off = int(m.group(2) or '0', 16)
        idx = off // 4
        sites[idx].append(a); used.add(st[m.group(1)][1])
    if tt.startswith('call'):
        for r in ('eax','ecx','edx'): st.pop(r, None)
        continue
    m = re.match(r'^(?:mov|lea|pop|xor|add|sub|and|or|imul|movzx|movsx|inc|dec|shl|shr|sar|neg|not|cdq|sbb|adc) (e[a-z]{2})\b', tt)
    if m: st.pop(m.group(1), None)
    m = re.match(r'^xchg (e[a-z]{2}),(e[a-z]{2})', tt)
    if m: st.pop(m.group(1), None); st.pop(m.group(2), None)
print("device loads", devrefs, "that reach a vtable call", len(used))
for idx in sorted(sites):
    s = sites[idx]
    print("%3d %-28s %4d  %s" % (idx, NAMES[idx] if idx < 97 else "?", len(s), " ".join("%X" % x for x in s)))

print("---- device loads with no direct vtable call ----")
idx = {a:i for i,(a,_) in enumerate(lines)}
for i,(a,t) in enumerate(lines):
    tt = re.sub(r'\s+', ' ', t)
    if re.match(r'^mov (e[a-z]{2}),dword ptr ds:\[006E7D94h\]$', tt) and a not in used:
        ctx = ' | '.join(re.sub(r'\s+',' ',lines[j][1]) for j in range(i, min(i+5, len(lines))))
        print('%X: %s' % (a, ctx))
for i,(a,t) in enumerate(lines):
    tt = re.sub(r'\s+', ' ', t)
    if '6E7D94h' in tt and not re.match(r'^mov (e[a-z]{2}),dword ptr ds:\[006E7D94h\]$', tt):
        print('OTHER %X: %s' % (a, tt))
