# The reference

`vavehero.html` is VaveHero in the browser: one file, with no network and no build step. Until the Dinghy port exists,
it is the ground truth for VaveHero's feel, timing and sound. When the C code and the HTML disagree, the HTML wins,
as in RoboSeq and SkurkyMello.

| File | Version | SHA-256 |
|---|---|---|
| `vavehero.html` | REF 0.1.14 (D13: the level counted from the notes over time, the import report's graph; the panel's knobs, MASTER, SELECT, ALGORITHM, KNOB 1–4, and SEQ) | `6643b5714b6272e542a4d8c19b08ff5c991f44f3846a211012cb8e815b25315b` |

A new HTML is a new reference and gets its own commit with a new SHA-256 here.

## Run

Open `vavehero.html` in a browser. Chrome and Edge have WebMIDI; the others play with the computer keyboard and the
panel. Press Enter (PLAY) on the title screen, or Space (FX) for the DEMO, choose a song with the up and down arrows
(each song has its own level, counted from its notes: D12, D13). The panel under the screen has the FM-1's knobs as
well as its keys and buttons: MASTER, SELECT, ALGORITHM and KNOB 1–4. Drag a knob, turn it with the mouse wheel, or tap
its left or right half; the page's help lists the computer keys for each.

Two players on one machine: open the file in two windows, choose **Two windows** in both, and give each window its own
MIDI keyboard. P1 chooses the song and starts.

## Test

```sh
node tests/reference.cjs          # requires Playwright with Chromium; screenshots in build/test/
```

The test checks that the title screen shows its image, and that FX starts the DEMO (ENGINE TOUR from bar 41) and PLAY
ends it. It plays both songs on autoplay and requires 100 % with no misses, both octave shifts and OP BOOST. It checks
that no note chart has black keys, and that a black key during play is ignored (D1). With SPREAD (D2) it checks 100 % on
autoplay, that the LEDs light only on the SPREAD keys, and that a key between two used ones does nothing. It also
checks that a press 80 ms late gives GOOD, that a wrong key breaks the combo, the match against the bot, and two
windows over LINK (roles, start, clock, score and stop). The two windows start with the same ID on purpose, and must
draw again to connect.

The songs (D3): the built-in MIDI files give the same note charts and octave shifts as before. A test song in E minor
(format 0, running status) is loaded through the file picker, transposed, gets octave shifts and is played 100 %. The
report mentions what has been removed or ignored. Live MIDI goes to the right part per channel, and the template
downloads identical to `songs/neon-carrier.mid`. The sound (D4): both songs are rendered offline for 20 seconds without
clipping, and the WAV files end up in `build/test/`.

MIDI MODE (D5): it opens from the last row of the song list, and a MIDI clock at 120 BPM gives 120 BPM and the right
pulse count. In LIVE, channels 1, 2 and 10 light up, and E4 from the DAW lights the E4 LED. The key C4 is sent out on
channel 1. In PREVIEW, channel 1 ends up one bar ahead, with an octave shift and a hold note. C#4 is shown as a
warning, and autoplay hits both notes and the shift. HOME exits, and MIDI MODE is kept closed while LINK is on.

The sound demos: ENGINE TOUR and CHROME HORIZON import without warnings. ENGINE TOUR uses all 29 patches and drum kits,
and CHROME HORIZON (D minor) is moved 5 semitones down to white keys. Both are played 100 % on autoplay, in two windows
at the same time, and rendered without clipping.

GRIT (D7): at GRIT 70 the band ducks to about half the level right after the kick, compared with GRIT 0.
CHROME HORIZON clips neither at GRIT 0 nor at 100, and the setting reaches the engine and the side panel.

Patches (D6): the built-in songs change patch where the keyswitch notes say so. All 22 patches and drum kits are heard
alone, and none of them clips. The test song's keyswitch notes end up in the report, and the one without a patch is
flagged. Live, keyswitch notes change the channel's patch, while Program Change, the OP note and a keyswitch note
without a patch change nothing. OP phrases come from note 11 on channel 1, not CC.

## The songs

`window.VAVEHERO_SONGS` in the HTML holds the built-in MIDI files in base64. `python3 tools/songs.py` makes
`songs/*.mid` and updates the block. The HTML then gets a new SHA-256.
