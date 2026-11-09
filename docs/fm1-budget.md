# Does it all fit on the FM-1? Memory, CPU and graphics

**Short answer: yes.** The memory has plenty of room for the engine, the graphics, 30 built-in songs, 8 of your own songs and all the scores.
The graphics cost little. The audio is the tight part. As it is in the reference, it sits on the edge, but with fixed
limits it ends up around 30 % of the chip.

The numbers below are **estimates** from the Dinghy documentation and from the measurements Star Distroyer and SkurkyMello have made in
fm1-emu. None of the FM-1 apps have been measured on a real FM-1 yet. That is why the first step of the port is to measure the audio engine
(see [ROADMAP.md](../ROADMAP.md), phase 3).

## What the FM-1 has (Dinghy 0.5)

| | |
|---|---|
| Code and constants (flash, executed directly) | 568 KB. `const` tables cost no RAM. |
| RAM (`.data` + `.bss`) | 96 KB |
| Pool (large buffers) | 336 KB |
| Stack | 24 KB |
| Storage | settings 4 KB, 8 blobs of 4 KB, 8 slots of 16 KB |
| Audio | 256 frames every 5.8 ms. The rule is to stay under half (2.9 ms). |
| Cores | One for the app. Dinghy does not give the app the second one, and some units have reported problems with it. |
| Compute | In fm1-emu the chip does about 240 million instructions per second: 8.7 M/s was 3.6 % in Star Distroyer. That gives about 5400 instructions per audio frame. |

## Memory

| What | Where | Size |
|---|---|---|
| The code: game, graphics, audio, MIDI, LINK | flash | about 150 KB (Star Distroyer uses around 111 KB) |
| The Terminus fonts (16 and 32 px) | flash | 33 KB |
| The title screen's image above the horizon (240 × 148, RGB565) | flash | 69 KB. The floor below the horizon is drawn live. |
| The sky above the highway (240 × 20, RGB565, D10) | flash | 9.6 KB |
| Wave tables and curves (tanh, bitcrush) | flash | about 10 KB |
| Built-in songs, 5 bytes per note | flash | FIRST LIGHT 2 KB, NEON CARRIER 5 KB, ENGINE TOUR 10 KB, CHROME HORIZON 7 KB. **30 songs are about 180 KB.** |
| **Total flash** | | **about 385 KB of 568** |
| Game state, voices, MIDI MODE buffer (one bar), LINK | RAM | about 15 KB of 96 |
| Screen canvas | pool | 59 KB (240 × 124, two strips) or 113 KB (the whole screen) |
| Tempo delay, dotted eighth down to 80 BPM, mono | pool | about 50 KB |
| Reverb (Schroeder/FDN, not convolution) | pool | about 32 KB |
| Gated reverb and chorus | pool | about 24 KB |
| **Total pool** | | **about 165–220 KB of 336** |
| Scores and medals: 4 bytes per song (its hash and level, and the accuracy) | settings or one blob | under 1 KB for 40 songs (one level each, D12) |
| Your own songs | slots | 8 × 16 KB. That is about 3000 notes per slot, enough for ENGINE TOUR (2023 notes). |

The play track of one song (about 200 notes) takes about 2.5 KB of RAM while it is played. The band is read straight from flash.

## Audio (CPU)

The heaviest spot is the last chorus of CHROME HORIZON or the finale of ENGINE TOUR: all the parts at once, on the dirty
patches. An oscillator costs 37–79 instructions per frame in Star Distroyer's compiled code. Here we count 40 for a
saw with PolyBLEP and 20 for a filter.

| Part | At the heaviest | Instructions per frame |
|---|---|---|
| PAD | 3 notes × 4 saws, filter | about 500 |
| BASS | 2 overlapping notes, 3 oscillators, filter, distortion | about 240 |
| ARP | 2 notes × 2 oscillators, filter | about 180 |
| LEAD | 3 oscillators, filter, distortion | about 140 |
| KEYS | 3 notes × 3 FM operators | about 350 |
| The player | 3 notes × 3 oscillators, filter | about 380 |
| Drums | about 6 layers at once (noise, tone, filter) | about 270 |
| Effects | chorus, delay, reverb, gated reverb, glue, compressor, pumping | about 300 |
| **Total** | | **about 2360, that is 43 % of the chip** |

That is under the half rule, but too close to the edge when the graphics also need their share. With these limits it comes down
towards 30 %, without much audible difference:

- **Voice cap:** 24 oscillator voices in total, where the oldest is stolen when it is full, and 6 drum layers on their own.
- **PAD with 3 saws instead of 4.** The chorus gives the width.
- **KEYS as 2-operator FM.**
- **Control rate:** envelopes, LFOs and filter coefficients are computed every 32nd frame, as Dinghy's rules say.
- **The reverb as a small Schroeder or FDN reverb.** The convolution in the reference (2.8 s) does not run on the FM-1.
- **The hottest loops in the 8 KB RAM window,** if cache misses from flash show up in the measurements.

Grit (D7) is cheap: distortion and bitcrush are a table lookup, and pumping is one multiplication per frame.

### Measured (firmware 0.1 in progress, 2026-11-07)

The audio engine now exists in fixed point (`firmware/src/kf_engine.h`) and is measured in fm1-emu in CI (`tools/emu_cpu.sh`):

| What | Measured |
|---|---|
| The platform alone, silent (USB, keys, the audio interrupt without sound) | 2.8 % |
| A synthetic worst case, all the parts on dirty patches (the first DEMO) | 43.9 %, 2390 instructions per frame |
| x86 versus the chip, same code | the chip counts about 0.8 times x86 (callgrind) |

The songs are heavier than the budget above assumed, because the pads have long release tails (SUPERSAW has τ = 1 s). In
ENGINE TOUR's heaviest bars (41–52), 24 voices were in use, with about 76 oscillators per frame. That is why the
engine has an **oscillator budget** of 48 (D8): when it is exceeded, the quietest release tails fade out in 25 ms. A
release tail on its own, for example at the end of a song, is allowed to ring out fully.

| What | Measured in fm1-emu |
|---|---|
| ENGINE TOUR bars 41–44, the audio engine alone (the band and the melody) | **31.6 %**, 1717 instructions per frame |
| The same as DEMO in the game (the player's voice with the combo's brightness) | **34.0 %**, 1853 instructions per frame; **30.8 %**, 1673, after the optimisation below |
| The same, all interrupts | 36.9 %; 33.6 % after |

The x86 estimate (callgrind × 0.8) said 31.5 %, so the estimate can be used between CI runs.

**Block by block and frame by frame** (`tools/emu_timing.py`, in CI after `emu_cpu.sh`). An average does not show the one
audio block that takes too long and causes a click, or the one frame that comes too late. The tool sets
breakpoints on the call into the audio interrupt, on `draw()` and on `song_start`, and measures each audio block and each frame:

| What (fm1-emu, 2026-11-07) | The DEMO, 60 fps | MIDI MODE, flood from a DAW |
|---|---|---|
| The audio blocks (256 frames, 5.8 ms each), median / worst | 32 % / 42 % of the time (before: 36 / 46) | 44 % / 48 % (before: 48 / 54) |
| `draw()` including the interrupts, median / worst | 3.1 ms / 5.5 ms (before: 6.6 / 9.3) | 2.3 ms / 4.9 ms (30 fps; before: 6.3 / 6.6) |
| Longest gap between two frames | 19.2 ms | 35.0 ms |

The flood: every 100 ms, channels 1–6 release a chord of 6 notes and play a new one, and channel 10 hits four drums.
The oscillator budget keeps the heaviest block at 54 %. CI fails if a block uses more than 75 % of the time, or if
two frames are more than 25 ms apart in the game or 45 ms in MIDI MODE (the start of a song is exempt).

The first measurement found a gap of 30–40 ms every second. `usb_retry` waited 25 ms in a loop when no USB host answered,
for example with a power bank or a charger. Every second the screen skipped three frames, and keys pressed
in the meantime got a timestamp up to 30 ms too late. Now `usb_retry` waits without blocking (PLATFORM.md), and
the keys get their timestamp from the scan (`FM1_NOTE_EDGE`), not from when the main loop reads them.

**Where the time goes, in the chip's own instructions.** fm1-emu counts every instruction address. A local copy that prints
the whole histogram, together with a `-g` build (`DINGHY_DEFS=-g ./build.sh` and `objdump -d -l`), gives a profile per
function and per source line. The run is the same as in `emu_cpu.sh`: 9.6 s, with FX after 1.6 s. The numbers are shares
of the whole chip:

| What | Before | After |
|---|---|---|
| The audio interrupt with the engine | 28.7 % | 25.9 % |
| Text (`ktext_n`) | 10.1 % | 4.8 % |
| Clearing the canvas every frame (`cv_begin`, now `kfill_rows` for the key rows) | 2.6 % | 0.5 % |
| Blended rectangles, beams and flashes (`krect_a`) | 3.0 % | 2.5 % |

The main loop spins when it has free time (`fm1_led_key` and `fm1_led_key_glow`, 39 %), and that time costs nothing.

What was done, with the same pixels and the same sound (bit for bit, checked against the previous commit):
- **Text:** a glyph that lies entirely on the canvas is drawn without a per-pixel check. The colour is split into channels once per
  glyph, not per pixel, and alpha comes from a table.
- **The canvas:** the play screen draws every row itself, so `draw()` does not clear it first. Only the key rows are filled,
  two pixels per write.
- **The oscillators:** the PolyBLEP test checked every sample whether the wave was near its jump. Now a tick (32
  frames) is checked once. If no oscillator is near the jump, a loop without the test runs. A square without a jump has the same level
  for the whole tick.
- **The filter and FM:** drive and crush are set by the patch, not by the sample. The loops are split into one per variant, and
  the drive table is looked up once.

The engine went from 34.1 % to 30.8 % of the chip, and a frame in the DEMO from 6.6 to 3.1 ms (median, with the interrupts).
The renders are identical byte for byte (the engine tour, the worst case, 60 s of each song), and all 23 screenshots from
the test scripts are identical pixel for pixel.

The sound is checked against the reference: all 25 patches and 4 drum kits are within 1.5 dB and 25 % brightness,
most within 0.3 dB. Every fourth bar of the first 30 seconds of all four songs is within 0.8 dB
(`tests/run_vavehero.sh`).

## Graphics

- A full screen is 240 × 240 × 2 bytes = 115 KB over SPI. **Dinghy drives the SPI at 12 MHz** (`lcd.c`, `LCD_BAUD 4`:
  60 MHz / 5), so a whole screen takes **77 ms**: at most 13 frames per second. (This page used to say 11 ms; that was
  wrong by a factor of 7. fm1-emu with `FM1_LCD_TIMED=1` showed it: 80 ms between frames, 12 fps.) The processor does
  other work meanwhile (`LCD_IDLE`). So VaveHero sends only what changed (D11): in the DEMO about 3 500 pixels a
  frame instead of 57 600, and the play holds 60 frames per second with the screen timed.
- The highway is rows of spans (about 150 rows × 17 spans), plus 20–40 capsules and text. That is about 150 000–300 000
  instructions per frame. **At 30 frames per second that is 2–4 % of the chip.**
- Only the highway (rows 40–222) has to be drawn every frame. The top bar and the knob row are drawn only when they change. That is
  about 85 KB per frame, so 30 frames per second uses about a third of the SPI time, but little CPU.
- Target: 30 frames per second, 60 if possible. **Measured with the screen timed (D11):** the DEMO 60 fps (16.0 ms
  between frames at the median, 2 of 305 frames over 25 ms, the longest 27 ms), MIDI MODE its 30 fps.
- **Calculated (firmware 0.1):** the play screen in ENGINE TOUR's heaviest bars costs about 0.69 million
  instructions per frame on the chip (callgrind × 0.8), that is about 9 % of the chip at 30 frames per second.
  The highway colours are blended once at startup, and text is blended in RGB565. The whole screen is one canvas of 113 KB.
  With D10 (the floor, the grid and the sky) the same bars cost about 7 % less, because text skips empty
  glyph bytes.

## Songs and scores

- **Built-in songs:** `tools/mid2vavehero` turns the MIDI files into compact tables when the firmware is built. They live in
  flash and cost no RAM. Transposition, octave shifts and levels are worked out there, not on the FM-1.
- **Your own songs on the FM-1:** SysEx belongs to the platform and is used for updates, so a file cannot be sent straight in.
  Two ways, both with notes only (D3):
  1. build the song into the firmware, or
  2. let **MIDI MODE record** the song from the DAW into a slot while the DAW plays it. The recording takes as long as the song.
- **Scores:** saved once after each song, when the sound has died away. A flash erase stops the audio for about 50 ms.
  A sector withstands about 100 000 erases, so with 20 songs a day it lasts over 13 years.

## What must be measured first

1. **The audio engine in fm1-emu,** with the finale of ENGINE TOUR as the worst case, before any of the graphics are built.
2. **Real hardware.** fm1-emu counts instructions, not cycles, and cache misses from flash can make it slower.
   None of the FM-1 apps have been measured on a real FM-1 yet.
3. **One core.** Everything is planned for one core. The second core is a bonus if it turns out to be stable.
