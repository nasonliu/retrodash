#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""The site's link preview (Open Graph, web/og.png, 1200 x 630): the firmware's title screen, pixel for pixel at
twice its size, and what VaveHero is. web/make_site.py puts it in the site; web/index.html names it (og:image).

  python3 web/make_og.py [SPLASH.png] [OUT.png]

SPLASH is a 240 x 240 picture of the firmware's title screen from the host build (tests/run_host.sh), by default
docs/screens/fw-splash.png. Its top row (FX DEMO and the version) is left out. The text is set in Inter
(/usr/share/fonts/opentype/inter, or INTER_DIR)."""
import os
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
W, H = 1200, 630
TOP = 12                                          # the splash's top row: FX DEMO and the version, left out
BG0, BG1, BG2 = (7, 2, 26), (42, 12, 80), (12, 4, 30)
FG, DIM, CYAN, NEON, LINE = (244, 238, 250), (165, 140, 204), (95, 227, 255), (255, 79, 216), (71, 34, 111)


def font(name, size):
    d = Path(os.environ.get("INTER_DIR", "/usr/share/fonts/opentype/inter"))
    return ImageFont.truetype(str(d / name), size)


def wrap(draw, text, f, width):
    lines, line = [], ""
    for w in text.split():
        t = (line + " " + w).strip()
        if draw.textlength(t, font=f) <= width or not line:
            line = t
        else:
            lines.append(line)
            line = w
    return lines + [line]


def main(splash, out):
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):                            # the page's own night: dark, violet, dark
        t = y / (H - 1)
        a, b, u = (BG0, BG1, t / .6) if t < .6 else (BG1, BG2, (t - .6) / .4)
        c = tuple(round(a[i] + (b[i] - a[i]) * u) for i in range(3))
        for x in range(W):
            px[x, y] = c
    sp = Image.open(splash).convert("RGB")
    if sp.size != (240, 240):
        raise SystemExit(f"{splash}: {sp.size[0]} x {sp.size[1]}, not the screen's 240 x 240")
    sp = sp.crop((0, TOP, 240, 240)).resize((480, 2 * (240 - TOP)), Image.NEAREST)
    x0, y0 = 64, (H - sp.size[1]) // 2
    d = ImageDraw.Draw(img)
    d.rectangle((x0 - 3, y0 - 3, x0 + sp.size[0] + 2, y0 + sp.size[1] + 2), outline=LINE, width=3)
    img.paste(sp, (x0, y0))

    tx, tw = x0 + sp.size[0] + 56, W - (x0 + sp.size[0] + 56) - 56
    big, mid, small = font("Inter-Bold.otf", 46), font("Inter-Regular.otf", 27), font("Inter-SemiBold.otf", 25)
    blocks = [(wrap(d, "A rhythm game for the M-VAVE FM-1", big, tw), big, FG, 58),
              (wrap(d, "The notes come down a neon highway. Play them on the white keys, and a synthwave band "
                       "plays along.", mid, tw), mid, DIM, 38),
              (wrap(d, "Free firmware, installed from the browser", small, tw), small, CYAN, 34)]
    gaps = [30, 34]
    height = sum(len(ls) * lh for ls, _, _, lh in blocks) + sum(gaps)
    y = (H - height) // 2
    d.rectangle((tx, y - 30, tx + 90, y - 26), fill=NEON)   # a neon stroke over the words
    for k, (ls, f, col, lh) in enumerate(blocks):
        for ln in ls:
            d.text((tx, y), ln, font=f, fill=col)
            y += lh
        if k < len(gaps):
            y += gaps[k]
    img.save(out, optimize=True)
    print(f"og: {out} ({W} x {H}, {os.path.getsize(out)} B)")


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) > 2 or any(x.startswith("-") for x in a):
        sys.exit(__doc__)
    main(a[0] if a else ROOT / "docs/screens/fw-splash.png", a[1] if len(a) > 1 else ROOT / "web/og.png")
