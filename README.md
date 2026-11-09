# RetroDash for M-VAVE FM-1

Native pixel-art rhythm-game firmware: play the melody on the FM-1 keys while the other voices accompany you. Built on VaveHero, Dinghy, Felucca and SLOOP.

**0.1.0-beta — tested on one real FM-1.** The current demo is *Peaceful Days* from a user-supplied **Chrono Trigger Pre-Release** SPC snapshot (151 seconds). This is a game firmware with one embedded song, not a general synthesizer or an arbitrary-SPC player. SMK-37 builds are not validated or released.

![RetroDash battle screen](docs/screens/retrodash-spc.png)

Host-rendered capture using the same firmware renderer and song assets; hardware measurements are documented separately.

## Play

- SELECT selects menu items; PLAY confirms. From the title, enter PLAY, then press PLAY on the song card.
- Play only the melody. Each pitch always maps to the same physical key; accompaniment plays automatically.
- **KNOB1 adjusts visual/enemy scroll speed, 50–200%, in 10% steps.** This changes approach time and saves the setting; music tempo and judgment timing stay independent.
- The minimum merge division is **1/8 note**. The demo has **200 manual targets and 2 automatic melody fills**; original music events remain intact.
- NEXT is bright and steady. THEN is dim and pulses on the beat. Automatic accompaniment/fills do not create key hints. AUTO and pause turn the hint lights off.
- PLAY pauses; HOME returns to songs. The pause menu offers resume/retry/songs. SEQ on the song screen toggles AUTO. Normal startup defaults to manual play.
- Settings: MUSIC output volume, SPEED music playback rate (50–100%), OFFSET calibration, BACKING accompaniment level (default 30%). KNOB1 is separate from SPEED.

All game pages use a shared UI; song projects supply the protagonist, enemies, background layers, animation and cover. The battle uses native-size sprites/fonts and three scrolling layers, with large judgments and combo feedback.

## Audio and song projects

`projects/peaceful-days-spc/` contains the original SPC, BRR samples, initial audio RAM/registers, timestamped S-DSP events, melody chart, art, scene and credits. The SPC700 driver runs offline; firmware runs the integer **snes_spc fast S-DSP** backend, including original BRR/envelopes/pitch changes/echo, then converts 32 kHz to the platform's 44.1 kHz output.

The fast DSP is not cycle-exact. The accurate offline replay is PCM-identical to full SPC emulation after its four-frame interface delay; the fast version's four checked passages correlated at 0.997–0.998 after alignment. Native device PCM fingerprints matched the same fast backend at all 21 captured checkpoints.

This beta embeds its SPC resources at build time. There is no USB song-pack import or on-device multi-song browser. Each selected SPC project supplies its own artwork and SPC-derived chart directly. Do not treat a visual `.cqp` alone as a complete SPC audio package.

See the [song-project creation guide / 歌曲工程创建指南](docs/creating-song-projects.md) for MIDI projects, SPC limitations, art/animation structure and build commands.

## Build and test

Prepare the JieLi pi32v2 toolchain and AC79 SDK as described in [the platform build guide](docs/DINGHY_README.md). Native cross-compilation runs on Linux x86-64; the existing Docker route is also retained.

```sh
python3 -m pip install mido Pillow
# Rebuild this SPC project, including its own artwork:
python3 tools/gen_spc_firmware.py projects/peaceful-days-spc
# build.sh defaults to the SPC/fast-DSP backend:
JIELI_TOOLCHAIN=/path/to/toolchain AC79_SDK=/path/to/sdk ./build.sh --release 0.1.0-beta
```

`build/retrodash-0.1.0-beta.fwsc` includes the update loader. Install over direct USB:

```sh
python3 -m pip install python-rtmidi
python3 tools/fm1_install.py build/retrodash-0.1.0-beta.fwsc --port Felucca --yes
python3 tools/fm1_install.py --info --port Felucca
```

The USB port retains the platform name **Felucca**. RetroDash's independent release identity is **FM-1_91720101**; older private development builds used VaveHero's FM-1_950 identity. Identify an exact build by the release SHA-256, not the identity alone. Hold OCT− while powering on for the inherited USB rescue loader.

Focused host checks (Linux, C/C++ compiler):

```sh
sh tests/run_retrodash.sh
```

25 Python checks, native renderer comparisons, parallax, manual audio triggers/fills, lights, pause/restart and KNOB1 timing independence are covered. Hardware test tools are optional; `numpy`/`sounddevice` are needed only for USB recordings.

## Hardware evidence and limitations

See [hardware validation](docs/hardware-validation.md) and [the SPC research notes](docs/spc-audio.md). The 200-target test used real USB MIDI input with AUTO disabled: 200 PERFECT, no misses. It does not claim to mechanically press the physical keys. The tested display is approximately 40–60 FPS (median 49 in that full-song run), not a stable 60 FPS.

The display-freeze fix covers the hardware TIMER4 rollover at approximately 179 seconds. A 210-second device run captured the rollover and continued drawing afterward; ordinary host mode had hidden the bug by using a different clock. Audio callback timings now subtract raw ticks before converting to microseconds.

The USB recording path reported some ring-buffer underflows while recording; the audio render callback itself had no deadline misses. USB capture quality and analog speaker/output quality are distinct checks. Stock-V15 migration/reversion and SMK-37 hardware are not validated by this release.

## Credits and licences

Firmware: **GPL-3.0-only**. Preserve upstream copyright notices; see [LICENSING.md](LICENSING.md). VaveHero by Joakim/joakimskurk; Dinghy by zvenson; Felucca by Leo Kuroshita/Hügelton Instruments; SLOOP by isod89. The S-DSP emulator is Shay Green's **snes_spc**, LGPL-2.1-or-later; its source and licence are included. Fonts retain their OFL licences.

Chrono Trigger characters, music, the supplied SPC/MIDI and derived game-media assets are third-party demo material and are **not relicensed by the firmware GPL**. Their respective rights remain with their owners. RetroDash is an unofficial fan project, unaffiliated with Square Enix, Nintendo or M-VAVE.
