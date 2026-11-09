# FM-1 hardware validation — 2026-11-09

**0.2.0-beta status:** dual-song compressed build passed host game/render tests and firmware capacity checks. It has not been flashed; the user disconnected the device. The measurements below belong to the previous single-song build and do not establish dual-song FPS or audio deadlines.

Validated on one connected M-VAVE FM-1, using development identity FM-1_950. The release has its own identity FM-1_91720101; it retains the same tested audio/game logic.

- A real-device USB MIDI input run, with AUTO disabled, sent 200 note-ons and 200 note-offs: **200 PERFECT, 0 GREAT/GOOD/MISS, maximum combo 200**. This does not mechanically exercise the physical keys.
- All 200 automatic target triggers were suppressed; the two merged melody fills stayed automatic.
- 21 native one-second PCM fingerprints from the real S-DSP backend matched the offline fast-DSP reference. The fast backend is not cycle-exact.
- Full-song play telemetry: **40–60 FPS, median 49**; maximum audio render callback 2.1 ms against a 5.805 ms block, zero callback deadline misses and zero SPC errors. Stable 60 FPS is not yet achieved.
- Captured 25 seconds of USB output with CoreAudio/PortAudio, with no PortAudio input-overflow status. Device USB ring-buffer underflows were observed during recording (81 repeated-last-frame events in that run), so this is not a zero-glitch USB-streaming claim. No analog output recording was made.

## Freeze regression

The free-running 24 MHz TIMER4 counter wraps every approximately 179 seconds. Dividing its absolute value by 24 and using that as an ordinary uint32 microsecond deadline stranded the next-frame timestamp after rollover. The main loop and audio continued, but the play screen stopped updating. A 151-second song test did not cover that boundary.

The frame scheduler now uses the platform's accumulated millisecond timebase; short timing measurements subtract raw hardware ticks first and convert the difference afterward. The native-branch regression test failed before the fix and passed after it.

A **210-second device run** captured an actual TIMER4 rollover at host elapsed 176.481 s. Display telemetry remained at 60 FPS immediately before/after that boundary and continued past 209 s. This validates display scheduling across the wrap, not sustained 60 FPS for every battle scene. The user also confirmed restored continuous movement. The temporary auto/soak diagnostics are excluded from the ordinary manual release.

Machine-readable summary: [hardware-validation.json](hardware-validation.json). Physical switch feel, stock-V15 install/reversion and SMK-37 hardware remain unverified by this release.
