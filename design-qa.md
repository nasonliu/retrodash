# RetroDash native design QA

final result: passed

Source visual truth: outputs/retrodash-design/retrodash-universal-ui.png (selected third direction, revised to generic UI).
Implementation evidence: outputs/retrodash-release/retrodash-all-pages.png, design-comparison.png, design-focused-comparison.png, help.png, pause.png, good.png, miss.png, countdown.png, error.png. End-to-end result: playback-results.png.

Viewport: native 240x240 RGB565 LCD. The 1536x1024 six-screen concept is normalized to a 1440x960 board; each actual 240x240 firmware render is enlarged to480x480 using nearest-neighbor. No device/browser chrome. No CSS dimensions or web runtime: this is the requested native C firmware, captured by its LCD HAL and exercised through the same controller/audio engine. This intentionally replaces web-specific prototype setup and browser checks.

Full-view comparison: design-comparison.png contains source and implementation in one image. Focused comparison: design-focused-comparison.png compares song selection and results at the same display density. Gameplay comparison uses PERFECT and20COMBO state; exact next pitch/score differ because implementation uses actual chart state. Result totals differ because the mock's sample98.6 is replaced by the engine's calculated value. Playback evidence is32PERFECT,100.0%,maxcombo32.

Comparison history:
- Initial comparison found song jacket largely empty and results grade/accuracy too small (P2), plus background arcs crossing result statistics (P2).
- Generated a proper per-song SLOW STREET jacket, enlarged grade and accuracy, removed decorative background from result statistics.
- Re-captured actual firmware panels and reviewed focused comparison; these issues are resolved.

Five fidelity surfaces:
- Typography: Barlow Condensed semibold and bold italic replace the old all-pixel UI typography. Display judgment28px, combo44px; grade uses a doubled44px glyph. Native binary glyph masks are an intentional LCD adaptation. No cut-off values observed, including100.0% and3-digit combo bounds.
- Layout: title/menu/settings/song/results follow the selected circle/primary-action hierarchy. Main settings/help choices stack for physical rotary selection. Real key labels and a fourth GREAT row are deliberate adaptations to actual hardware and scoring. Keyboard remains y195..234; cue area and hit text do not overlap.
- Tokens: deep plum/acid yellow/hot pink/mint are globally fixed and not inherited from song assets. High contrast input hints remain invariant across song palettes.
- Assets: original generated logo/background/shockring/burst are packed as RGB565 images; original per-song cover/characters/scenery/enemies remain in song project. Global title/menu/settings/results contain no song actors/world art. No screenshot used as a static interactive screen.
- Copy: product is RETRODASH. Actual SELECT/PLAY/HOME/SEQ controls replace concept keyboard labels. AUTO is identified as non-recorded. No invented song catalog or USB import controls.

Interaction validation: title -> main -> settings/help/song -> play -> pause/resume/retry/quit -> results -> retry/song. MUSIC/SPEED/OFFSET bounds and save/load tested; whole melody manual script achieves32PERFECT. AUTO/paused key lights dark; manual bright NEXT and separate dim THEN traces checked. Unit C UI test is run with ASan/UBSan. No browser console exists; compiler/sanitizer logs are clean.

Residual P3: native font edges are pixel-quantized; generic ring motif is less dense than the large concept board. The screen uses the existing six-frame per-song animation. Physical LCD appearance and LED perceived brightness require user observation after flashing.

Implementation checklist: all requested pages implemented, controls exercised, native captures compared, firmware built inside Flash/RAM/pool limits. Hardware flashing tracked separately in release report/log.
