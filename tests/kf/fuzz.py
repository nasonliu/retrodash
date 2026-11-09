#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""A random host script (tests/host.c's lines) for VaveHero: someone mashing everything on an FM-1 while a DAW sends
it whatever it likes. Seeded, so a failure can be run again.

  - the buttons (every FM-1 button, PLAY and HOME most), the seven encoders a few detents either way
  - the 27 keys: one, chords, all of them, quick taps, holds
  - MIDI on every channel: notes (the keyswitch notes 0-11 too, the OP note), velocity 0, any status byte with any
    data (CC, program change, pitch bend, SysEx bytes, real time), clocks at a random tempo, START, STOP
  - long stretches in a song (PLAY from the songs), and now and then MIDI MODE (SELECT to its row)

Run it under the sanitizers (tests/run_vavehero.sh): any out-of-bounds access or undefined behaviour stops the run.

    python3 tests/kf/fuzz.py SEED [SECONDS] > script.txt"""
import random
import sys

BTN = {"OCT-": 0, "OCT+": 1, "FX": 2, "SEL": 3, "ENV": 4, "LFO": 5, "EDIT": 6, "GLO": 7, "HOME": 8, "SAVE": 9,
       "ARP": 10, "SEQ": 11, "PLAY": 12, "REC": 13}
N_ENC = 7


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    secs = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
    r = random.Random(seed)
    out, t, end = [f"# tests/kf/fuzz.py {seed} {secs:g}"], 300, int(secs * 1000)
    keys = 0
    while t < end:
        mode = r.random()
        if mode < .10:                          # into a song, or MIDI MODE: PLAY, HOME, SELECT
            out.append(f"{t} btn {BTN['PLAY'] if r.random() < .8 else BTN['HOME']}")
            if r.random() < .25:
                out.append(f"{t + 40} enc 0 {r.choice([-9, -1, 1, 4, 9])}")
            t += r.randint(60, 400)
        elif mode < .22:                        # any button, any encoder
            if r.random() < .6:
                out.append(f"{t} btn {r.randrange(14)}")
            else:
                out.append(f"{t} enc {r.randrange(N_ENC)} {r.choice([-5, -2, -1, 1, 2, 5])}")
            t += r.randint(5, 200)
        elif mode < .62:                        # the keys
            kind = r.random()
            if kind < .5:
                keys ^= 1 << r.randrange(27)
            elif kind < .7:
                keys = sum(1 << k for k in r.sample(range(27), r.randint(2, 6)))
            elif kind < .75:
                keys = (1 << 27) - 1
            else:
                keys = 0
            out.append(f"{t} keys {keys:#x}")
            t += r.choice([1, 2, 3, 8, 15, 40, 120, 400])
        elif mode < .95:                        # MIDI notes
            ch = r.choice([0, 0, 1, 2, 3, 4, 5, 9, 6, 15])
            note = r.choice([r.randrange(128), r.randrange(12), 11, r.randrange(48, 84)])
            on = r.random() < .6
            vel = r.choice([0, 1, 64, 100, 127]) if on else r.randrange(128)
            out.append(f"{t} midi {(0x90 if on else 0x80) | ch:#x} {note} {vel}")
            t += r.choice([0, 0, 1, 5, 20, 60])
        else:                                   # anything else on the wire
            kind = r.random()
            if kind < .3:
                out.append(f"{t} clock {r.choice([40, 90, 120, 180, 300])} {r.randint(1, 96)}")
            elif kind < .45:
                out.append(f"{t} midi {r.choice([0xFA, 0xFB, 0xFC, 0xFE, 0xFF]):#x} 0 0")
            else:
                out.append(f"{t} midi {r.randrange(0x80, 0x100):#x} {r.randrange(256)} {r.randrange(256)}")
            t += r.randint(0, 80)
    out.append(f"{end} keys 0")
    out.append(f"{end + 200} end")
    print("\n".join(out))


if __name__ == "__main__":
    main()
