# ROADMAP

The current phase is the first one with an item that is not checked off. Same recipe as RoboSeq and Star Distroyer:
first a playable browser reference that is the ground truth for feel, timing and sound, then a Dinghy 0.5 app that is
tested on the host and in fm1-emu.

**Two players are built in from the start.** The game engine is deterministic and has one protocol for LINK
([docs/link.md](docs/link.md)), so every phase below must keep VS mode alive.

## Phase 0: idea and mockup

- [x] Concept ([docs/concept.md](docs/concept.md))
- [x] Mockup of the device and four screens at 240 × 240 with Dinghy's Terminus fonts ([mockup/](mockup/))

## Phase 1: browser reference 0.1

- [x] `reference/vavehero.html`: one file, playable with the computer keyboard, the panel (mouse and touch) and WebMIDI
- [x] The screen drawn pixel by pixel at 240 × 240 with `FONT_S` and `FONT_L`
- [x] Two songs (FIRST LIGHT, NEON CARRIER) with EASY and MED. The levels are made from the same note chart.
      (Changed by D12: a song has one fixed level, from a LEVEL marker in its file, and one chart.)
- [x] Note highway in perspective with one lane per white key, chords, hold notes and octave shifts (OCT+/OCT−)
- [x] D1: only the white keys are played. The black ones are shown but have no function (REF 0.1.2,
      [docs/DECISIONS.md](docs/DECISIONS.md)).
- [x] D2: SPREAD, every other white key for big fingers, with GLO
- [x] Score: PERFECT ±35 ms, GREAT ±70 ms, GOOD ±110 ms, combo and multiplier, WRONG for a wrong key
- [x] OP phrases and OP BOOST with FX
- [x] Key LEDs (lit and glow) and button LEDs, the way the FM-1 would show them
- [x] FM sound: three voices for the player (TONE) and a band with pad, bass and drums. The sound gets brighter with
      combo and dull on mistakes.
- [x] Latency setting, with calibration from the last round's average offset
- [x] LINK L0 and L1: protocol version 1, two windows, and a bot as opponent
- [x] D3: the songs are MIDI files, one instrument per channel, the play track on channel 1 ([docs/songs.md](docs/songs.md)).
      The import transposes, and adds octave shifts and OP phrases. Your own files are loaded and remembered, and the
      templates can be downloaded. The song's identity goes over LINK.
- [x] D4: synthwave audio engine with pad, bass, arpeggio, lead, FM piano, gated drums, syndrum, delay and hall reverb
- [x] Live MIDI per channel: the DAW plays the song through the audio engine while it is being made
- [x] D6: several patches per instrument (3 each, 4 voices on PLAY, 3 drum kits), selected with keyswitch notes 0–11,
      in files and live. TONE defaults to SONG. Notes only: OP phrases are note 11 on channel 1, not CC.
- [x] D7: dirtier sound. GRIT (pumping, saturation, drum crunch), a dirty patch per instrument, the DIRT drum kit,
      and the demo songs rewritten.
- [x] D5: MIDI MODE. LIVE is a sound module with channels, clock and bar. PREVIEW delays by one bar, shows channel 1
      on the highway and lets you play along. The keys are sent out on channel 1 for recording.
- [x] Test: `node tests/reference.cjs` plays both songs on autoplay, tries wrong and late presses, a bot match and two windows

## Phase 2: reference 0.2, game feel

- [ ] Playtest with real hands: lane width, note speed and timing windows
- [ ] Sweep sections with KNOB 1
- [ ] Playtest of SPREAD with real hands: is F3–F5 right, or G3–G5?
- [ ] Transposition per song, so the white keys follow the song's key
- [ ] Practice mode: looping a section, “wait for the right key”, and a guide melody
- [ ] A separate calibration screen: press in time with clicks
- [x] D12: one fixed level per song (EASY, MED, HARD, EXP); ENGINE TOUR is HARD
- [x] D13: the level counted from the notes over time (notes a second, the whole song and its busiest four bars); a LEVEL marker overrides it
- [x] The reference's panel has every control the firmware uses: MASTER, SELECT, ALGORITHM, KNOB 1–4 (drag, wheel or tap) and SEQ
- [ ] The level: count octave shifts, chord shapes and lane jumps too, if playtests ask for it
- [ ] An EXP song and 2–3 new songs
- [ ] MIDI import: tempo changes, and a keyswitch note to override the automatic octave shifts
- [x] Sound check: renders of the reference as the ground truth for the port, as in RoboSeq (`tools/ref_tour.cjs`, `tests/kf/parity.py`)

## Phase 3: Dinghy port

The budget for memory, CPU and graphics is in [docs/fm1-budget.md](docs/fm1-budget.md). In short, everything fits,
but the audio is the tight part, so it is measured first.

- [x] **First: measure the audio engine in fm1-emu.** Bars 41–44 of ENGINE TOUR (all the parts, dirty patches, the pads'
      long release tails) use 31.6 % of the chip alone, and 34.0 % with the game around it (36.9 % with the platform). CI fails above 45 %. What it took:
      24 voices and 10 drum layers, an oscillator budget of 48 (D8), a control rate of 32 frames and FDN reverb.

- [x] `vavehero.c` on Dinghy 0.5: `APP_NAME "VAVEHERO"`, its own identity (`FM-1_95N`), `PLATFORM.md` with deviations
      from upstream, CI via zvenson/fm1-build, `make test` on the host
- [x] Highway, top strip and knob row in one full-screen canvas, drawn like the reference. Estimated at about 9 % of
      the chip at 30 fps (callgrind × 0.8). Must be measured on a real FM-1.
- [x] Key LEDs with `fm1_led_key` and `fm1_led_key_glow`, and SPREAD (GLO) saved in the settings
- [x] Precise timing: key edges are also read in `LCD_IDLE` and stamped with the audio clock, the way it is heard
      (the latency on a real FM-1 must be calibrated)
- [x] The synthwave engine in the audio interrupt (fixed point: sawtooth with PolyBLEP, filters, chorus, gated reverb,
      hall reverb), measured in fm1-emu and checked against the reference's renders (all patches within 1.5 dB, the
      songs within 0.8 dB)
- [x] `tools/mid2vavehero.mjs`: MIDI file to note chart and band tracks for flash, with the reference's own import code
- [x] The patches (D6) and GRIT (D7) in fixed point, with keyswitch notes in firmware. GRIT is on ALGORITHM.
- [x] MIDI MODE LIVE in firmware: USB-MIDI and TRS in (`midi_get`), clock and STOP, keyswitch notes, the keys out on
      channel 1 with `midi_send`
- [ ] MIDI MODE PREVIEW in firmware, and the LEDs for channel 1. To be tested with a DAW against fm1-emu and then on a
      real FM-1.
- [x] Note maps in firmware, and the best result in flash (AUTO is not saved)
- [ ] The game engine is ready for LINK from the first version: START, clock and score go through one module
- [x] Host tests (`make test`) and `.fwsc` with attestation in CI
- [ ] fm1-emu in strict mode, and a calibration screen for the latency
- [ ] Tried on a real FM-1

## Phase 4: LINK on hardware, two FM-1s connected with USB-C

- [ ] **Feasibility study for direct USB-C (L3).** Can the AC791N be a USB host (OTG)? Can the FM-1 supply 5 V on
      VBUS, or is a powered OTG adapter needed? How big will a USB-MIDI host driver be? This can start now, in
      parallel with phases 2–3, because the answer decides the hardware track.
- [ ] L2: relay page in the reference (WebMIDI) that connects FM-1 A and FM-1 B through a computer, and firmware that
      speaks the protocol
- [ ] L3: host mode in firmware (P1 is the host) and one USB-C cable between two FM-1s, if the feasibility study says
      yes. Otherwise L2 becomes the default.
- [ ] Tempo adjustment on P2 to follow the clock (deviation over 10 ms)
- [ ] Modes: VS on hardware, then alternating phrases and band (co-op)
- [ ] Test with two real FM-1s: start, sync over a whole song, disconnect in the middle of a match

## Phase 5: release

- [ ] Card for Firmware Hub (`fmhub.json`), logo and user manual with lessons
- [ ] Installation and return to M-VAVE's firmware documented, as in the other projects
