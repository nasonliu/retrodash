#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""VaveHero's timing on the emulated chip (fm1-emu): what the averages of tools/emu_cpu.sh do not show.

  - every audio block (the ALNK0 interrupt, 256 frames): its cost against the 5.8 ms it has. One block over its time
    and the sound clicks, whatever the average;
  - every frame: what draw() costs, and the gaps between frames. A gap is the main loop away: the screen skips and the
    keys wait.

Breakpoints on the call into the audio interrupt and the instruction after it, on the call to draw() and the one after
it, and on song_start (a song's start may take a frame or two: those gaps are left out). The addresses come from the
build's own disassembly (build/retrodash.dis). fm1-emu counts instructions: 240 M of them are a second.

Two runs:
  demo    the splash, FX at 1.6 s: the DEMO (ENGINE TOUR from bar 41, the worst of the songs), measured 2.0-7.0 s
  flood   MIDI MODE LIVE and a DAW gone wild: every 100 ms each of channels 1-6 lets go of a 6-note chord and plays
          another, channel 10 hits four drums; measured 2.0-5.5 s

    python3 tools/emu_timing.py build/retrodash.fwsc build/retrodash.dis [--ci] [--only demo|flood]
    FM1_EMU   the fm1-emu binary (default: ~/fm1-emu/target/release/fm1-emu)

--ci fails (exit 1) when an audio block takes more than 75 % of its time, when more than 5 % of the frames come later
than LATE_MS after the one before, or one later than HARD_MS (outside a song's start).

CI runs it with FM1_LCD_TIMED=1: the screen's transfers take their time at the SPI clock (12 MHz), as on an FM-1,
not at once. The frame rate is then what an FM-1 would show."""
import os
import re
import statistics
import subprocess
import sys
import tempfile

RATE = 240e6                        # fm1-emu: instructions a second (tools/emu_cpu.sh)
BLOCK = 256 / 44100 * RATE          # an audio block's time, in instructions
AUDIO_MAX = 75.0                    # % of a block's time
# between frames: the play draws at up to 60 fps (16.7 ms), MIDI MODE at 30 (33.3 ms). A frame now and then may come
# late; most must not (LATE_MS, at most LATE_SHARE of them), and none very late (HARD_MS)
LATE_MS = {"demo": 25.0, "flood": 45.0}
LATE_SHARE = 0.05
HARD_MS = {"demo": 50.0, "flood": 70.0}
SKIP_MS = 300.0                     # after a song_start: its loading, left out


def sites(dis):
    """the addresses from the disassembly: (call, after) for the audio interrupt and draw, and song_start"""
    lines = open(dis).read().splitlines()
    addr = re.compile(r"^\s*([0-9a-f]+):\s")
    out, label = {}, None
    for i, ln in enumerate(lines):
        if re.match(r"^\w+:$", ln):
            label = ln[:-1]
            if label == "song_start":
                m = addr.match(lines[i + 1])
                out["song_start"] = m.group(1)
            continue
        for name in ("fm1_alnk0_irq", "draw"):
            if re.search(r"\tcall .*<%s : " % name, ln) and (name != "fm1_alnk0_irq" or label == "isr_alnk0"):
                a, b = addr.match(ln), addr.match(lines[i + 1])
                if a and b and name not in out:
                    out[name] = (a.group(1), b.group(1))
    missing = {"fm1_alnk0_irq", "draw", "song_start"} - set(out)
    if missing:
        sys.exit(f"emu_timing: not in {dis}: {', '.join(sorted(missing))}")
    return out


def flood_keys():
    ev = ["500:PLAY", "900:enc0:+4", "1300:PLAY"]   # the splash, SONGS, the last row: MIDI MODE
    prev, t, k = {}, 1800, 0
    while t < 5500:
        for ch in range(6):
            for n in prev.get(ch, []):
                ev.append(f"{t}:midi:{0x80 | ch:02x}{n:02x}00")
            notes = [36 + ch * 5 + ((k * 7 + i * 3) % 30) for i in range(6)]
            ev += [f"{t}:midi:{0x90 | ch:02x}{n:02x}64" for n in notes]
            prev[ch] = notes
        ev += [f"{t}:midi:99{d:02x}70" for d in (36, 38, 42, 49)]
        t += 100
        k += 1
    return ",".join(ev)


def run(emu, pkg, ms, keys, at):
    """the breakpoint hits: [(pc, step)]"""
    pcs = [at["fm1_alnk0_irq"][0], at["fm1_alnk0_irq"][1], at["draw"][0], at["draw"][1], at["song_start"]]
    env = dict(os.environ, FM1_BREAK_MAX="200000", FM1_BREAK_PCS=",".join(pcs[1:]))
    hit = re.compile(r"^break c0 pc=0x([0-9a-f]+) \[(\d+)\]")
    out = []
    with tempfile.TemporaryDirectory() as wd:
        os.makedirs(os.path.join(wd, "work"))
        p = subprocess.Popen([emu, "--fwsc", os.path.abspath(pkg), "--ms", str(ms), "--keys", keys, "--break-pc", pcs[0]],
                             cwd=wd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        for ln in p.stdout:
            m = hit.match(ln)
            if m:
                out.append((m.group(1).lstrip("0"), int(m.group(2))))
        p.wait()
    return out


def report(name, hits, at, start_ms, ci):
    norm = lambda a: a.lstrip("0")
    a_in, a_out = map(norm, at["fm1_alnk0_irq"])
    d_in, d_out = map(norm, at["draw"])
    s_in = norm(at["song_start"])
    blocks, frames, starts, ent_a, ent_d = [], [], [], None, None
    for pc, st in hits:
        if pc == a_in:
            ent_a = st
        elif pc == a_out and ent_a is not None:
            blocks.append((ent_a, st - ent_a))
            ent_a = None
        elif pc == d_in:
            ent_d = st
        elif pc == d_out and ent_d is not None:
            frames.append((ent_d, st - ent_d))
            ent_d = None
        elif pc == s_in:
            starts.append(st)
    t0 = start_ms / 1000 * RATE
    b = [c for s, c in blocks if s >= t0]
    f = [(s, c) for s, c in frames if s >= t0]
    if not b or len(f) < 2:
        print(f"emu_timing {name}: no measurements ({len(blocks)} blocks, {len(frames)} frames)")
        return False
    ms = lambda steps: steps / RATE * 1000
    gaps = []
    for (s0, _), (s1, _) in zip(f, f[1:]):
        if any(s0 - SKIP_MS / 1000 * RATE <= s <= s1 for s in starts):
            continue                            # (a song's start: its loading)
        gaps.append((s0, ms(s1 - s0)))
    bmax, bmed = max(b), statistics.median(b)
    dr = [c for _, c in f]
    print(f"emu_timing {name}: audio blocks {len(b)}: median {100 * bmed / BLOCK:.0f} %, max {100 * bmax / BLOCK:.0f} % "
          f"of their {ms(BLOCK):.2f} ms")
    print(f"emu_timing {name}: frames {len(f)}: draw median {ms(statistics.median(dr)):.1f} ms, max {ms(max(dr)):.1f} ms "
          f"(with the interrupts in it); gaps median {statistics.median(g for _, g in gaps):.1f} ms, "
          f"max {max(g for _, g in gaps):.1f} ms")
    long = [(s, g) for s, g in gaps if g > LATE_MS[name]]
    worst = max(g for _, g in gaps)
    for s, g in long[:8]:
        print(f"emu_timing {name}:   a frame {g:.1f} ms after the one before, at {ms(s):.0f} ms")
    print(f"emu_timing {name}: {len(long)} of {len(gaps)} frames later than {LATE_MS[name]:.0f} ms")
    ok = 100 * bmax / BLOCK <= AUDIO_MAX and len(long) <= LATE_SHARE * len(gaps) and worst <= HARD_MS[name]
    if ci and os.environ.get("GITHUB_ACTIONS"):
        print(f"::notice title=fm1-emu {name}::audio blocks max {100 * bmax / BLOCK:.0f} % of their time; "
              f"frame gaps max {max(g for _, g in gaps):.1f} ms")
    if not ok:
        print(f"emu_timing {name}: FAIL (audio blocks at most {AUDIO_MAX:.0f} %; at most {100 * LATE_SHARE:.0f} % of the frames "
              f"later than {LATE_MS[name]:.0f} ms, none later than {HARD_MS[name]:.0f} ms)")
    return ok


def main():
    argv = sys.argv[1:]
    only = None
    if "--only" in argv:
        i = argv.index("--only")
        only = argv[i + 1] if i + 1 < len(argv) else None
        del argv[i:i + 2]
    args = [a for a in argv if not a.startswith("--")]
    if len(args) != 2 or only not in (None, "demo", "flood"):
        sys.exit(__doc__)
    pkg, dis = args
    ci = "--ci" in argv
    emu = os.environ.get("FM1_EMU", os.path.expanduser("~/fm1-emu/target/release/fm1-emu"))
    at = sites(dis)
    ok = True
    if only in (None, "demo"):
        ok &= report("demo", run(emu, pkg, 7000, "1600:FX", at), at, 2000, ci)
    if only in (None, "flood"):
        ok &= report("flood", run(emu, pkg, 5500, flood_keys(), at), at, 2000, ci)
    sys.exit(0 if ok or not ci else 1)


if __name__ == "__main__":
    main()
