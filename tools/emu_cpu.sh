#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk), after Star Distroyer's tools/emu_cpu.sh
# The engine's load on the chip, in fm1-emu (ROADMAP.md, phase 3: "first, measure the engine"): the audio
# interrupt's share of the chip while VaveHero's DEMO plays (FX on the splash: kf_bench.h, the worst case), in instructions (the
# emulator counts instructions, not cycles). Two runs: 1.6 s of silence, then 9.6 s with FX pressed at 1.6 s;
# the difference is 8 s of the DEMO: ENGINE TOUR's bars 41-44. The main loop spins in the emulator, so its count says nothing about the chip.
#   tools/emu_cpu.sh build/retrodash.fwsc [label]
#   FM1_EMU   the fm1-emu binary (default: ~/fm1-emu/target/release/fm1-emu; tools/emu_ci.sh builds it)
#   EMU_MAX   fail if the share is above this many percent (default: report only)
set -e
PKG="$1"
[ -f "$PKG" ] || { echo "usage: tools/emu_cpu.sh PACKAGE.fwsc [label]"; exit 2; }
LABEL="${2:-$(basename "$PKG" .fwsc)}"
EMU="${FM1_EMU:-$HOME/fm1-emu/target/release/fm1-emu}"
SILENT_MS=1600
PLAY_MS=8000
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cp "$PKG" "$WORK/pkg.fwsc"
emu() {    # ms, keys, log
    echo "emu_cpu: $1 ms${2:+, $2}"
    s0=$(date +%s)
    (cd "$WORK" && mkdir -p work && timeout "${EMU_TIMEOUT:-500}" "$EMU" --fwsc pkg.fwsc --ms "$1" ${2:+--keys "$2"} >"$3" 2>&1) || true
    echo "emu_cpu: took $(( $(date +%s) - s0 )) s"
    grep -q "in an ISR" "$3" || { cat "$3"; echo "emu_cpu: no report"; exit 1; }
}
emu $((SILENT_MS + PLAY_MS)) "$SILENT_MS:FX" "$WORK/play.txt"
emu $SILENT_MS "" "$WORK/silent.txt"
isr_total() {    # M ISR instructions in the whole run
    awk -v ms="$2" '/in an ISR/ { for (i = 1; i <= NF; i++) if ($i == "M" && $(i+1) == "ISR") { m = $(i-1); sub(/\(/, "", m); printf "%.3f", m * ms / 1000.0 } }' "$1"
}
PLAY_TOTAL=$(isr_total "$WORK/play.txt" $((SILENT_MS + PLAY_MS)))
SILENT_TOTAL=$(isr_total "$WORK/silent.txt" $SILENT_MS)
grep -E "in an ISR|^time " "$WORK/play.txt" | sed 's/^/  emu: /'
PCT=$(awk -v play="$PLAY_TOTAL" -v silent="$SILENT_TOTAL" -v pms=$PLAY_MS -v sms=$SILENT_MS 'BEGIN {
    per_s = (play - silent * (pms + sms) / sms) / (pms / 1000.0); printf "%.1f", 100.0 * per_s / 240.0 }')
awk -v label="$LABEL" -v play="$PLAY_TOTAL" -v silent="$SILENT_TOTAL" -v pms=$PLAY_MS -v sms=$SILENT_MS -v pct="$PCT" 'BEGIN {
    base = silent / (sms / 1000.0)
    per_s = (play - silent * (pms + sms) / sms) / (pms / 1000.0)
    printf "%s: the DEMO (the worst case) costs the engine %.1f M instructions/s = %s%% of the chip, %.0f a frame\n", label, per_s, pct, per_s * 1e6 / 44100.0
    printf "%s: all interrupts while it plays %.1f%% (the platform alone, silent: %.1f M/s = %.1f%%)\n", label, 100.0 * (per_s + base) / 240.0, base, 100.0 * base / 240.0
}'
[ -n "$GITHUB_ACTIONS" ] && echo "::notice title=fm1-emu::the DEMO (ENGINE TOUR bars 41-44) costs the engine $PCT % of the chip"
if [ -n "$EMU_MAX" ]; then
    awk -v p="$PCT" -v m="$EMU_MAX" 'BEGIN { exit !(p <= m) }' || { echo "emu_cpu: FAIL, above $EMU_MAX %"; exit 1; }
fi
