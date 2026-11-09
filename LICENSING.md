# Licensing

VaveHero, like Dinghy under it, is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The TRS MIDI IN `firmware/src/midi_uart.c` | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| The wiring `platform.c`, the host mode `tests/`, the docs in `docs/` (`API.md`, `RULES.md`, `MAKE-IT-YOURS.md`, `SMK37.md`, `DINGHY_README.md`) | zvenson |
| The browser installer's protocol: `web/fm1pkg.js`, `web/fm1ota.js` (unchanged, from [SLOOP](https://github.com/isod89/sloop-fm1) by way of FM-1 Clock); the install flow of `web/index.html` and the shape of `web/make_site.py`, after SLOOP's `web/index_pkg.html` and `web/make_site.py` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca), with the changes of SLOOP (isod89) |
| VaveHero: the app `firmware/src/vavehero.c` (from Dinghy's `dinghy.c` by zvenson) and its `kf_*` files, `reference/`, `mockup/`, `songs/`, `tools/songs.py`, `tests/reference.cjs`, `tests/run_vavehero.sh`, the page `web/index.html`, `web/make_site.py` (but for what is SLOOP's in them, above) and the link preview `web/make_og.py`, `web/og.png`, the project docs | Copyright (C) 2026 Joakim (joakimskurk) |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| Inter Display Black Italic (Rasmus Andersson): the VAVEHERO logo's letters, drawn by `tools/gen_splash.py` into `firmware/gen/kf_splash.h`, `assets/vavehero-logo.png` and the reference (the font itself is not in this tree) | SIL OFL 1.1 | <https://rsms.me/inter> |
| Inter (Rasmus Andersson): the words of the link preview `web/og.png`, set by `web/make_og.py` (the font itself is not in this tree) | SIL OFL 1.1 | <https://rsms.me/inter> |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

VaveHero's own logo is `assets/vavehero-logo.png` (made by `tools/gen_splash.py`, GPL-3.0 as the rest of VaveHero). `assets/logo.svg` / `logo.png` carry the designburg signet: Copyright (C) designburg, all rights reserved, not under
the GPL. It marks Dinghy itself; a fork uses its own logo.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. Neither Dinghy nor VaveHero is made or endorsed by them.

## RetroDash additions

RetroDash game/UI integration, SPC project tools, tests and hardware fixes are GPL-3.0-only. Upstream attribution above is retained.

- `firmware/vendor/snes_spc/` and `projects/peaceful-days-spc/vendor/snes_spc/`: snes_spc by Shay Green, LGPL-2.1-or-later, with the original notices and licence included.
- Barlow Condensed: SIL OFL 1.1 (`assets/BarlowCondensed-OFL.txt`). Other bundled font licences remain alongside their assets.
- User-supplied Chrono Trigger SPC/MIDI, original BRR samples, character references and theme media are not covered by the code's GPL licence. Original music/character rights remain with their respective owners. No ownership of Square Enix/Nintendo content or endorsement is claimed.
