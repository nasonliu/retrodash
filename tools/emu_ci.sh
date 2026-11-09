#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# CI: fm1-emu (joakimeriksson/fm1-emu, pinned) built from source, then the engine's load on the package just built
# (tools/emu_cpu.sh) and its timing, block by block and frame by frame (tools/emu_timing.py: the DEMO and a MIDI flood).
# The numbers land in the job's log.
set -e
REV=92a21df72e9eae4c78b44c3199c974bc5937c620
DIR="$HOME/fm1-emu"
if [ ! -x "$DIR/target/release/fm1-emu" ]; then
    rm -rf "$DIR"
    git init -q "$DIR"
    git -C "$DIR" fetch -q --depth 1 https://github.com/joakimeriksson/fm1-emu "$REV"
    git -C "$DIR" checkout -q FETCH_HEAD
    echo "emu_ci: building fm1-emu ($REV)"
    (cd "$DIR" && timeout 900 cargo build -q --release -p fm1-emu)
fi
echo "emu_ci: fm1-emu ready"
EMU_MAX=${EMU_MAX:-45} timeout 1200 sh tools/emu_cpu.sh build/retrodash.fwsc   # (45 %: ROADMAP's ceiling)
FM1_EMU="$DIR/target/release/fm1-emu" FM1_LCD_TIMED=1 timeout 1500 python3 tools/emu_timing.py build/retrodash.fwsc build/retrodash.dis --ci   # (each audio block, each frame, the screen at its own speed)
