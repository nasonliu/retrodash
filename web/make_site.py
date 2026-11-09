#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""VaveHero's site (GitHub Pages): its page, its installer in the browser, the firmware, the game in the browser.

  index.html                    web/index.html with fm1pkg.js, fm1ota.js (SLOOP's, unchanged) and the metadata
                                inlined: the installer (Web MIDI, Chrome or Edge), the game, the controls, the links
  firmware/vavehero-VER.fwsc    the package the page installs (its SHA-256 in the page: a damaged download is
                                never offered)
  play.html                     the reference (reference/vavehero.html): the game in the browser, no FM-1 needed
  screens/                      the pictures (assets/vavehero-logo.png, docs/screens, the link preview web/og.png)
  .nojekyll                     (served as it is)

  python3 web/make_site.py build/vavehero.fwsc VERSION OUT_DIR

The package must be a VaveHero one: its identity FM-1_95N, its Felucca update loader in it (which the installer
talks to). Made after FM-1 Clock's web/make_site.py, itself after SLOOP's (Copyright (C) 2026 Leo Kuroshita
(@kurogedelic), Hügelton Instruments; isod89).
"""
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
PICTURES = {"assets/vavehero-logo.png": "logo.png", "docs/screens/splash.gif": "splash.gif",
            "docs/screens/fw-play.png": "play.png", "docs/screens/fw-songs.png": "songs.png",
            "docs/screens/fw-results.png": "results.png", "docs/screens/fw-midi.png": "midi.png",
            "web/og.png": "og.png"}                     # the link preview (web/make_og.py)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_95\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not VaveHero's (FM-1_95N)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no Felucca update loader in it: the installer could not write it")
    name = f"vavehero-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "firmware/" + name,
                       "sha256": hashlib.sha256(raw).hexdigest(), "size": len(raw)})
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    html = (HERE / "index.html").read_text(encoding="utf-8")
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"web/index.html must hold {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    if out.exists():
        shutil.rmtree(out)
    (out / "firmware").mkdir(parents=True)
    (out / "screens").mkdir()
    shutil.copy(pkg, out / "firmware" / name)
    for src, dst in PICTURES.items():
        shutil.copy(ROOT / src, out / "screens" / dst)
    shutil.copy(ROOT / "reference" / "vavehero.html", out / "play.html")
    (out / "index.html").write_text(html, encoding="utf-8")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html ({len(html)} B), firmware/{name} ({len(raw)} B, {product}), play.html, "
          f"{len(PICTURES)} pictures")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
