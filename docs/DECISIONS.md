# Decisions

A decision is recorded here with its date and reason, so that the code and the reference can be checked against it.
It overrides the concept where they disagree.

## D1 (2026-11-07): only the white keys are played

**Decision.** The note charts only have notes on white keys, and the lanes on the screen are one per white key. The
black keys are still shown on the screen where they sit, in the upper row of the key strip, but they have no
function. During a song they give no note, no score, no penalty and no sound.

*Adjusted the same day (REF 0.1.2).* For a short while the strip showed only the white keys. The black row is back
because it makes it easier to see where you are on the keyboard. The decision was about the function, not the display.

**Why.**

- The screen is built for it. Eight lanes, one per white key, are easy to read on 240 pixels. Black notes on the
  dividing lines were the messiest part of the highway.
- On the FM-1 the black keys sit in a separate upper row, not between the white ones as on a piano. Hitting them at
  speed is not the same as playing piano.
- With one function per key, playing, there is nothing to remember. The strip still shows the black keys, so that
  the screen looks like the keyboard.

**What it costs.** Notes outside the scale disappear. This is made up for with transposition per song, so that the
white keys follow the song's key and mode. Three notes in NEON CARRIER were rewritten: D#4 became D4, G#4 became A4
and F#5 became D5.

**Checked by.** `buildChart` stops with an error if a note chart has a black key. `tests/reference.cjs` checks that
no note chart has black keys, and that a black key in the middle of a song does not change the combo or the judgement.

## D2 (2026-11-07): SPREAD, every other white key

**Decision.** A setting, SPREAD, puts the eight lanes on every other white key: F3 A3 C4 E4 G4 B4 D5 F5, with one
unused white key between each. The lanes and the music are the same. Only which physical key plays each lane
changes. The keys in between have no function, like the black ones (D1). SPREAD is turned on and off with GLO (`Q`
in the reference), is saved, and cannot be changed in the middle of a song.

**Why.** The silicone keys on the FM-1 are small. With large fingers it is easy to hit the neighbouring key. With one
key of space between them, the distance between the lanes becomes about twice as large. It fills almost the whole
keyboard: 15 of 16 white keys.

**What it looks like.** The key strip on the screen shows the used keys under the lanes and a dark, unused key
between each. The LEDs and the letter hints in the panel follow the used keys. SPREAD applies only to the person
playing, so in a match one player can have SPREAD on and the other not.

**Checked by.** `tests/reference.cjs` plays NEON CARRIER with SPREAD on autoplay (100 %). It also checks that the
LEDs light only on the SPREAD keys, that a key between two used ones does nothing, and that F3 plays the first lane.

## D3 (2026-11-07): the songs are MIDI files, one instrument per channel

**Decision.** A song is a standard MIDI file (format 0 or 1). Channel 1 is the play track. Channels 2 BASS, 3 PAD,
4 ARP, 5 LEAD, 6 KEYS and 10 DRUMS (General MIDI) are the band. Channel 16 is reserved for LINK. Note 11 (B-1) on
channel 1, held over the phrase, marks OP phrases (changed from CC 85 the same day, see D6). The import transposes the
play track to white keys (D1) and adds octave shifts; the file's LEVEL marker gives the song its level (D12). The built-in songs are made the same way
(`tools/songs.py` writes `songs/*.mid`) and go through the same import. The full rule is in [songs.md](songs.md).

**Why.** It should be possible to make songs entirely in a DAW, with tools that already exist. One channel per
instrument is common MIDI practice, and the same channel layout works both for files and for live MIDI. When the DAW
plays into the reference, the reference becomes a sound module where you hear the song as in the game.

**Checked by.** `tests/reference.cjs`: the built-in files give the same note charts and octave shifts as before. A
test song in E minor (format 0, running status) is transposed, gets octave shifts, and is played 100 % on autoplay.
The report mentions the chromatic note, the fourth chord note, channel 7 and channel 16. Live MIDI goes to the right
part per channel.

## D4 (2026-11-07): the audio engine is synthwave

**Decision.** The audio engine is made for synthwave and retrowave: sawtooth pads with chorus, pulsing bass,
arpeggio with tempo delay (dotted eighth), lead with vibrato, FM piano, gated reverb on snare and clap, syndrum toms
and a dark hall. The player's voice can be SYN LEAD (default), BELL EP, BRASS or GLASS.

**Levels.** Measured in the chorus of NEON CARRIER, each part alone: the player's voice −20 to −22 dB RMS, drums −21,
bass −24.5, pad and arpeggio −30. The whole mix has a peak value of 0.75. The player's voice should sit on top or
level with the drums. That check is repeated when the sound changes.

**What it costs.** Many oscillators per note (four per pad note). On the FM-1 the engine must be built in fixed point
and measured in fm1-emu. If it is too heavy, the pad voices and the hall are simplified first.

## D5 (2026-11-07): MIDI MODE, the DAW straight into the FM-1 while the song is made

**Decision.** The FM-1 has its own mode for making songs: MIDI MODE, at the bottom of the song list. The DAW is
connected to the FM-1 over USB-C, and each track is sent on its own channel as in D3. The mode has two variants, and
PLAY switches between them:

- **LIVE:** the FM-1 is a sound module. Each channel is played at once by its instrument. The screen shows activity
  per channel, the tempo from the MIDI clock, and bar and beat. Notes on channel 1 light up the key LEDs, and notes
  on black keys are flagged in red (D1).
- **PREVIEW:** everything is delayed by one bar, as far as the highway looks ahead. Channel 1 falls down the highway
  as it will in the game, with octave shifts. The band plays in time with the highway, and you can play along and be
  judged. Black notes are shown as red marks and are not counted.

The FM-1's keys are sent out on channel 1 in both variants, so the DAW can record the play track. Without a MIDI
clock, the tempo from KNOB 3 (BPM) is used. MIDI MODE and LINK are mutually exclusive, because both use the MIDI
clock and START/STOP.

**Why.** The fastest way to make a playable song is to hear it on the FM-1 and see the play track on the highway
while writing it. PREVIEW answers the question "can this be played?" without exporting the file.

**What it costs.** Dinghy does not pass Song Position Pointer through. If the DAW starts in the middle of the song,
the FM-1 counts bars from where it started, not from the song's bar. PREVIEW lags one bar behind the DAW, so the
DAW's own sound must be off, or it must play MIDI only.

**Checked by.** `tests/reference.cjs`, with the tests described in [reference/README.md](../reference/README.md).

## D6 (2026-11-07): several patches per instrument, chosen with keyswitch notes

**Decision.** Each instrument has several patches: PLAY 4, BASS, PAD, ARP, LEAD, KEYS 3 each, and the drums 3 kits.
Notes 0–11 (C-1 to B-1) on the instrument's channel choose the patch, with note 0 as patch 1. The note is never
heard, and applies until the next keyswitch note. On channel 1 it chooses the player's voice when TONE is set to
SONG, which is the default. It works the same in MIDI files, in live MIDI and in MIDI MODE. The table is in
[songs.md](songs.md).

**Why.** One sound per instrument quickly gets monotonous over a whole song. Keyswitch notes are how sample
libraries switch articulation, so they are familiar from the DAW. Notes 0–11 are not used for music, so they do not
collide with anything.
**Notes only (adjusted the same day).** Program Change and CC are not used, neither for patches nor for OP phrases
(which are now note 11 on channel 1). Everything in a song can then be drawn in the piano roll, and that is the least
demanding way to make a song.

**Levels.** Each patch is measured alone (four notes, or a drum pattern). Within the same instrument the patches are
within ±2 dB of each other, so a switch does not become a jump in volume. The chorus of NEON CARRIER, with FM BASS
and the BELL arpeggio, has the same balance as in D4.

**Checked by.** `tests/reference.cjs`, with the tests described in [reference/README.md](../reference/README.md).

## D7 (2026-11-07): dirtier sound, with GRIT

**Decision.** The audio engine should be able to sound dirty and French (Kavinsky, Justice), not just polished:

- **GRIT** (0–100, default 70) controls three things on the whole mix: pumping, where the band ducks on every kick
  (sidechain), saturation and glue on the whole mix, and crunch on the drum bus. At 0 the sound is clean as before.
- **One dirty patch per instrument,** with distortion after the filter, and some with a crushed copy (bitcrush):
  FUZZ (player voice), DIRT BASS, GRIT PAD, CRUSH (arpeggio), FUZZ LEAD and GRIT EP. The drum kit is called DIRT. They
  sit on the next free keyswitch note, which is note 4 on channel 1 and note 3 on the others.
- **Drier mix:** the hall is lowered from 0.55 to 0.4.
- The player's voice does not pump, so it always sits clear above the band.

**Levels.** Each dirty patch is within ±2 dB of its siblings. The exception is the DIRT kit, which is within the
range of the other drum kits. In the dirty last chorus of CHROME HORIZON the player sits at −18.5 dB and the drums at
−18.7 dB, the bass 3.3 dB below, and keys, arpeggio and pad further back. With GRIT 70 the mix gets about 2 dB
denser, with a lower peak level, and no song clips at 0, 70 or 100.

**For the FM-1.** Distortion, bitcrush and pumping are cheap in fixed point, much cheaper than a large reverb. The
dirt is the easiest part to port.

**Checked by.** `tests/reference.cjs`, with the tests described in [reference/README.md](../reference/README.md).

## D8 (2026-11-07): the firmware's sound, where it differs from the reference

**Decision.** The audio engine in the firmware (`firmware/src/kf_engine.h`) should sound like the reference, and it
is checked against the reference's own renders. It differs from the reference only on these points, because the FM-1
cannot afford the rest:

- **Oscillator budget of 48.** When the voices need more oscillators than that, the quietest release tails fade out
  over 25 ms. A release tail that stands alone, for example at the end of a song, is allowed to ring out fully.
  Otherwise release tails end 40 dB below their peak, while the reference lets them run for six time constants.
- **The reverb is an FDN** with four lines behind four diffusers, not a 2.8 s convolution. The energy is calibrated
  against the reference. The gated reverb is an FDN behind a gate that the snare and the clap open.
- **FM voices with integer ratios** (ORGAN, FM EP, TINE, FM BASS, BELL EP, GRIT EP, GLASS PAD) start the modulators
  at a fixed phase that gives the reference's average level. In the reference the phase follows the note's start
  time, and the level varies by up to 4 dB from note to note.
- **The compressor** follows Chromium's curve (measured with `tools/ref_tour.cjs --comp`), but without the 6 ms
  lookahead delay.

**Why.** The heaviest spot in the songs (bars 41–44 in ENGINE TOUR) cost 76 oscillators per frame without a budget,
because the pads have long release tails. With the budget the sound uses 31.6 % of the chip, and 34.0 % with the
game around it (measured in fm1-emu). A 2.8 s convolution is far beyond what the chip can do.

**Checked by.** `tests/run_vavehero.sh`: all 25 patches and 4 drum kits within 1.5 dB and 25 % brightness of the
reference, most within 0.3 dB, and every fourth bar of the first 30 seconds of the four songs within 0.8 dB.
`tools/emu_cpu.sh` in CI fails if the heaviest spot uses more than 45 % of the chip.

## D9 (2026-11-07): the game is called VaveHero

**Decision.** The game is called VaveHero, after M-VAVE and "hero" from the genre. The working title KEYFALL is gone
from file names, screens, firmware and documents. The screen shows VAVEHERO, with VAVE in white and HERO in cyan,
because the large font only has capital letters. The firmware is called `vavehero.fwsc`, the app
`firmware/src/vavehero.c`, the reference `reference/vavehero.html`, and the package identity is still `FM-1_95N`.

**What it costs.** Settings saved in flash by an earlier build are reset, because the storage now has its own marker
(`STORE_MAGIC` "VHRO"). The same goes for what the reference has saved in the browser, including your own songs,
because the storage keys are now named `vavehero.*`. Internal names in the code (`kf_`, `kg_`, `ks_` and
`window.KF`) are kept, so that the code is not changed just for the sake of the name.

**To check before a release.** "Hero" in Guitar Hero is Activision's trademark, as the concept already mentioned.
VaveHero is a separate name, but it should be reviewed before the game is published on Firmware Hub.

## D10 (2026-11-07): the title screen's synthwave across the whole game

**Decision.** The title screen's look is used throughout the game, not the other way around. That covers the chrome
logo, the sunset, the mountains with neon edges, the neon floor and violet night.

- **The colours:** the neutral colours have moved from navy blue to violet night (P in the reference, `K_*` in
  `kf_gfx.h`). Magenta neon (`#ff4fd8`) is the horizon line, and the grid is `#ff3df0`.
- **The highway:** the highway lies on the title screen's floor. The grid continues the highway's lines beyond its
  edges toward the same vanishing point, and the beat lines run across the whole floor, so it moves in time. The
  edges glow magenta. The far end fades into the floor's haze, not into black. Above the highway the sunset is
  squeezed into 20 rows: the glow, a few stars and the mountains. It is made by `tools/gen_splash.py` (`KF_SKY`) and
  has the neon horizon beneath it.
- **The title screen:** the floor is the game's own highway as the road into the sunset, and it plays itself. The
  lanes, the beat lines, the notes, the hit line with flashes and the keys look as they do in the game, in a loop at
  125 BPM. FX DEMO and the version are at the top.
- **The menus:** the song list, the results screen and MIDI MODE sit in the night sky with stars. The heading has
  the neon horizon beneath it, the progress bar is the sunset from magenta to gold, and the selected row has neon
  that fades toward the right.
- **Chrome:** the big words (the song title at the start, the countdown, FULL COMBO, CLEAR, PAUSE) and the grade on
  the results screen are in the logo's chrome (`ktext_chrome`, `textChrome`). OP BOOST is gold, as before.

**The logo** has less black now. The black band in the middle of the chrome was a bug: the gradient turned black
past the last stop. The edge and the outline are now violet instead of almost black.

**What it costs.** Drawing has not become more expensive. Text now skips empty glyph bytes, and an outline is used
only where the background requires it (the score over the sky and the hit words). Callgrind on the same script
(`-Os`) gives this: the game screen costs about 7 % less per frame in the DEMO (ENGINE TOUR from bar 41) and about
15 % less over all of `tests/kf/game.txt`. The title screen costs about 35 % less. The sky uses 9.6 KB of flash.

## D11 (2026-11-08): the screen gets only what changed; Dinghy stays at 0.5

**Decision.** The screen is sent in parts: only the blocks of a row that changed since the last frame, as rectangles.
The SPI clock stays at Dinghy's 12 MHz until a faster one has been tried on a real FM-1. Dinghy stays at 0.5, its
newest version. VaveHero's two fixes in its platform stay here (`PLATFORM.md`); whether Dinghy wants them is
for its author to say, and is talked over with him directly.

**Why.** Dinghy drives the screen at 12 MHz, and a whole 240 × 240 frame is 921 600 bits: 77 ms, at most 13 frames
per second. fm1-emu with the screen timed (`FM1_LCD_TIMED=1`) showed it: 80 ms between frames. Most of a frame stays as
it was, though: the lanes, the sky, the strips. In the DEMO about a third of the rows change a frame, and in them
about a third of the width.

- **How:** each row has 8 blocks of 30 px with a hash each (FNV-1a), in `vavehero.c`'s `send_changed`. A row's
  changed blocks make its span, and rows with the same span make one rectangle. A rectangle narrower than the screen is
  copied into one of two staging buffers before its DMA, so the next one is filled while the last one goes out.
- **The heal band:** 4 rows a frame go out whole as well, walking round the screen. Whatever else the screen shows
  (the update screen's text, a hash that matched by chance) is gone within a few seconds.
- **Checked:** on the host the panel shows exactly the canvas after every one of 7 200 frames (the game, the results
  and two random scripts). All screenshots are the same pixel for pixel.
- **Measured in fm1-emu with the screen timed:** the DEMO went from 12 to 60 frames per second (16.0 ms between frames
  at the median), and MIDI MODE keeps its 30. CI runs the timing guard (`tools/emu_timing.py`) with the screen timed.

**The clock.** `LCD_BAUD` can be lowered: 2 is 20 MHz, 1 is 30 MHz. The panel is an ST7789 class, which is usually
specified up to about 60 MHz. Neither Dinghy nor the FM-1's own firmware says what this board takes, and a wrong
clock gives a broken picture. So the speed is tried on hardware before it changes.

**Also from the timed and strict runs** (fm1-live with `FM1_STRICT=1`: measured cycles per instruction and the flash
cache's misses). The tables the audio interrupt reads every frame, the sine table and the drive curves, are copied
into RAM at the start, and so is the sky over the highway (11 KB of RAM, 10 KB of the pool). The flash cache's misses
in the DEMO fell from 145 305 to 25 293. The audio interrupt's share of the cycles fell from 38.5 % to 36.6 %. The
sound is the same, byte for byte.

**What you see against what you hear.** A press is judged by the sound: the scan stamps the key, and the frame being
heard then (`heard_now`: the audio clock, less the 512 frames between a block and the speaker, less the player's own
latency) is held against the note. The screen does not come into it. But a frame shows the song as it was when its
drawing started, and the hit line's rows go out among its last. Before the changes above that was 60–80 ms behind
the sound, and a player who played by eye was judged late. Now the play is drawn ahead (`kg.vpos`, `vis_lead_us` in
`vavehero.c`): by how long the last frames took from the start of the drawing until their last rows went to the DMA,
smoothed. fm1-emu with the screen timed measures it at about 13 ms in the DEMO. The panel's own refresh (an ST7789
redraws at about 60 Hz) is not in it: that, and the rest, is the calibration's (the latency setting, from the average
timing of a round).

**Dinghy.** Upstream's newest commit is `ea1b6bf`, the tag `v0.5`, and every Dinghy file here is that one,
except VaveHero's own changes (`PLATFORM.md`). Two of those changes would help every Dinghy app: `usb_retry` without
its 25 ms busy wait, and the `FM1_NOTE_EDGE` hook. Nothing is sent to Dinghy from here: the project talks with its
author directly.


## D12 (2026-11-08): a song has one level, and it is fixed

**Decision.** As on the Japanese arcade machines, a song's level belongs to the song: EASY, MED, HARD or EXP, one of
them, chosen by whoever made it. The player chooses the song, not the level. A song is one chart, its play track as
written; there is no thinned-out EASY copy any more. The level is a marker in the MIDI file, a marker or text meta
event `LEVEL EASY`, `LEVEL MED`, `LEVEL HARD` or `LEVEL EXP` ([songs.md](songs.md)). A file without one is MED, and
the import report says how to choose.

What the level does:

- **EASY:** the right note in any octave counts, and the octave shifts happen by themselves.
- **MED, HARD, EXP:** the right note in the right octave, and the player follows the shifts with OCT− and OCT+. HARD
  and EXP are ratings for songs with more and faster notes, more chords and more shifts; the rules are MED's.

The built-in songs: FIRST LIGHT is EASY, NEON CARRIER and CHROME HORIZON are MED, ENGINE TOUR is HARD.

(Changed the same day by D13: the level is counted from the notes over time, and a marker only overrides that.)

**Why.** A level made by a program out of someone's song was a thinned copy of it: the notes on the beat, the top
note of a chord. It played like another, weaker song. With the level fixed, the one who writes the song decides how
it should feel to play, and the list of songs becomes a ladder from easy to hard. The level is also a promise: a MED
song is MED wherever it is played, and two players over LINK always play the same chart.

**What changed.**

- KNOB 1 does nothing on SONGS, and its label there is empty. The songs screen lights the song's own level and dims
  the other three. The reference's left and right arrows still turn KNOB 1 (MIDI MODE's MODE).
- The best result is kept per song. Its key is the same as before, the song's hash and its level, so a result saved
  at the level the song now has is still its result.
- LINK no longer sends LEVEL (CC 103): both players play the song at its own level. A CC 103 from an older version is
  ignored ([link.md](link.md)).
- The firmware's songs carry one chart each (`firmware/gen/kf_songs.h`: `chart`, `nchart` and `level`), about 2 KB
  less flash. `kg_start` and `song_start` take no level; the rules read it from the song.

## D13 (2026-11-08): the level is counted from the notes over time

**Decision.** A song's level (D12) is counted from how many notes come how fast: notes a second at the song's own
tempo, every note of a chord counted. Two numbers: over the whole song, from its first note to the end of its last,
and over its busiest four bars (16 beats from any note on). The rating is 0.3 × the whole + 0.7 × the busiest, to one decimal.

| Rating | Level |
|---|---|
| under 1.6 | EASY |
| 1.6 to 2.7 | MED |
| 2.7 to 3.8 | HARD |
| 3.8 and up | EXP |

A `LEVEL` marker in the file (D12) still sets the level, for a song that is harder or easier than its notes a second
say. The import report gives the rating and both numbers, shows the notes a second bar by bar with the busiest four
bars marked, and says when a marker overrides them.

The built-in songs carry no marker any more. Their notes give them the levels they had: FIRST LIGHT 1.4 EASY, CHROME
HORIZON 1.8 MED, NEON CARRIER 2.5 MED, ENGINE TOUR 2.9 HARD.

**Why.** The level should say what the song asks of the hands, and the same way for every song, whoever made it.
Notes over time is what the hands feel. The busiest stretch counts most, because that is where a song is lost; the
whole song keeps one hard fill from making a calm song HARD. A song maker gets a level without having to choose one,
and sees why it came out that way.

**Not counted (yet):** the octave shifts, the chords' shapes, the jumps between lanes. Notes a second came first
because they are easy to understand and to check. If playtests show songs at the wrong level, the next things to
count are those.

**SPEED** (the practice tempo) does not change a song's level: the level is the song's at its own tempo.
