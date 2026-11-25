"""Offline preview of SH4 Refined v3.22.0 Thermal (staging/feedback_v322/PLAN.md section 2). Pure stdlib.

Applies the exact ps.1.1 arithmetic of kPsThermal (the cold world) and kPsHeat (a body redrawn hot) with the
constants read from src/screenfx.inc to a picture and writes a PNG:
  top left     the picture through the cold world palette
  top right    the same picture as if every pixel were a living body (heat 1.0)
  bottom left  the world palette for luminance 0..1
  bottom right body ramps for texel luma 0..1 at heat 1.0 / 0.6 / 0.3 (alpha = heat over the cold world)
Usage: py -3 tools/thermal_sim.py <picture.dds (32-bit BGRX, as SH4 Refined writes it)> <out.png> [width]
"""
import re
import struct
import sys
import zlib
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "screenfx.inc"


def consts():
    txt = SRC.read_text(encoding="utf-8", errors="replace")
    return {m.group(1): float(m.group(2)) for m in re.finditer(r"#define\s+((?:THERM|TH)_\w+)\s+([-0-9.]+)f?", txt)}


def sat(x):
    return 0.0 if x < 0.0 else 1.0 if x > 1.0 else x


def load_dds(path, width):
    b = Path(path).read_bytes()
    h, w = struct.unpack_from("<II", b, 12)
    oh = max(1, h * width // w)
    img = []
    for y in range(oh):
        sy = y * h // oh
        row = []
        for x in range(width):
            sx = x * w // width
            o = 128 + (sy * w + sx) * 4
            row.append((b[o + 2] / 255.0, b[o + 1] / 255.0, b[o] / 255.0))
        img.append(row)
    return img


def world(p, c):
    L = sat((0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]) * c["THERM_GAIN"])
    st = (c["THERM_R0"], c["THERM_G0"], c["THERM_B0"])
    sl = (sat(c["THERM_RS"] * 0.25), sat(c["THERM_GS"] * 0.25), sat(c["THERM_BS"] * 0.25))
    cold = sat(4.0 * (c["THERM_COLD_END"] - L) * sat(c["THERM_COLD_S"] * 0.25))
    cc = (c["THERM_COLD_R"], c["THERM_COLD_G"], c["THERM_COLD_B"])
    return tuple(sat(sat(4.0 * (L - st[i]) * sl[i]) + cold * cc[i]) for i in range(3))


def body(p, c, heat):
    V = sat((0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]) * c["TH_DETAIL"]) + c["TH_BODY"] * heat
    st = (c["TH_R0"], c["TH_G0"], c["TH_B0"])
    sl = (c["TH_RS"] * 0.25, c["TH_GS"] * 0.25, c["TH_BS"] * 0.25)
    return tuple(sat(4.0 * (V - st[i]) * sl[i]) for i in range(3))


def write_png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = bytearray()
    for r in rows:
        raw.append(0)
        for p in r:
            raw += bytes(int(sat(v) * 255.0 + 0.5) for v in p)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def main():
    c = consts()
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 480
    img = load_dds(sys.argv[1], width)
    top = [[world(p, c) for p in r] + [body(p, c, 1.0) for p in r] for r in img]
    grad = [(x / (width - 1),) * 3 for x in range(width)]
    bottom = []
    for heat in (1.0, 0.6, 0.3):
        hot = [body(g, c, heat) for g in grad]
        under = [world((g[0] * 0.6,) * 3, c) for g in grad]
        al = min(1.0, heat * c['TH_ALPHA_K'])
        ramp = [tuple(hot[x][i] * al + under[x][i] * (1.0 - al) for i in range(3)) for x in range(width)]
        wl = [world(g, c) for g in grad]
        for _ in range(30):
            bottom.append(wl + ramp)
    write_png(sys.argv[2], top + bottom)
    print("wrote", sys.argv[2], f"{2 * width}x{len(top) + len(bottom)}")


if __name__ == "__main__":
    main()
