#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# VaveHero's own host tests (no FM-1, no JieLi toolchain): the app compiles cleanly for the host and plays, the
# engine against the reference (every patch's level and brightness, tests/kf/parity.py), the worst case renders
# without clipping, the tables are what the reference makes. Dinghy's own tests: tests/run_tests.sh (after
# ./build.sh). The engine's cost on the chip: tools/emu_cpu.sh (fm1-emu, CI).
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT" build/audio
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

if command -v node >/dev/null 2>&1; then
    run "firmware/gen/kf_tables.h is what tools/gen_kf_tables.mjs makes from the reference" node tools/gen_kf_tables.mjs --check
    run "firmware/gen/kf_songs.h is what the reference's import makes of songs/ (tools/mid2vavehero.mjs)" node tools/mid2vavehero.mjs --check
fi
if python3 -c 'import numpy' 2>/dev/null && [ -f "${INTER:-/usr/share/fonts/opentype/inter/InterDisplay-BlackItalic.otf}" ]; then
    run "firmware/gen/kf_splash.h is what tools/gen_splash.py draws" python3 tools/gen_splash.py --check
fi
run "vavehero.c compiles for the host without a warning (-Werror)" \
    cc -O2 -Wall -Werror -Wno-unused-function -DDINGHY_HOST -DAPP_VER='"test"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen \
       -o "$OUT/vavehero-werror" firmware/src/retrodash.c
if command -v clang >/dev/null 2>&1; then           # (the chip's compiler is a clang: its warnings, e.g. a constant that does not fit)
    run "vavehero.c with clang, as the chip's compiler sees it (-Werror)" \
        clang -O2 -Wall -Werror -Wno-unused-function -DDINGHY_HOST -DAPP_VER='"test"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen \
           -o "$OUT/vavehero-clang" firmware/src/retrodash.c
fi
run "the engine alone compiles without a warning (-Wextra -Werror)" \
    cc -O2 -Wall -Wextra -Werror -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/kf_render tests/kf/kf_render.c -lm
run "the engine tour: every patch and kit" sh -c "build/kf_render tour build/audio/kf-tour.wav build/audio/kf-tour.json >/dev/null"
run "the engine against the reference: levels within 1.5 dB, brightness within 25 %" python3 tests/kf/parity.py build/audio/kf-tour.json tests/kf/ref-tour.json
for i in 0 1 2 3; do
    run "song $i, its first 30 s against the reference's renderSong: every 4 bars within 1.5 dB" sh -c \
        "build/kf_render song $i 30 build/audio/kf-song$i.wav build/audio/kf-song$i.json >/dev/null && python3 tests/kf/parity.py build/audio/kf-song$i.json tests/kf/ref-song$i.json -q"
done
run "the worst case (kf_bench.h: ENGINE TOUR from bar 41) renders" build/kf_render stress build/audio/kf-stress.wav
run "the worst case does not clip" python3 - <<'PY'
import struct, sys
d = open("build/audio/kf-stress.wav", "rb").read()[44:]
x = struct.unpack("<%dh" % (len(d) // 2), d)
peak = max(abs(v) for v in x) / 32768
print("  peak %.3f" % peak)
sys.exit(0 if peak < 0.999 else 1)
PY
run "the rules compile without a warning (-Wextra -Werror)" \
    cc -O1 -Wall -Wextra -Werror -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/kf_game_test tests/kf/kf_game_test.c
run "the rules: every song on autoplay 100 %, late, wrong, black and outside keys, a broken hold" build/kf_game_test
run "the app on the host: the default script" sh tests/run_host.sh
run "the game on the host: splash, DEMO, songs, a song, MIDI MODE" sh tests/run_host.sh tests/kf/game.txt
run "the screens are there" sh -c 'for t in 600 2500 7400 8400 15000 15800; do test -s build/host/vavehero-$t.png || exit 1; done'
run "a song on AUTO to its results, again, back to the songs" sh tests/run_host.sh tests/kf/results.txt
run "the results' screen is there" test -s build/host/vavehero-42000.png
run "the sound file is there and not empty" test -s "$OUT/vavehero.wav"
if [ -f build/retrodash.fwsc ]; then                 # (CI builds the package before the tests)
    run "the site (web/make_site.py): the page with the installer, the package, the game in the browser, the link preview" sh -c \
        "python3 web/make_site.py build/retrodash.fwsc test build/site-test >/dev/null && test -s build/site-test/index.html && test -s build/site-test/play.html && test -s build/site-test/screens/og.png && grep -q 'property=.og:image. content=.https://.*/screens/og.png' build/site-test/index.html && grep -q 'id=.ack.' build/site-test/index.html && grep -q 'disabled = on || !image || !\$(.ack.).checked' build/site-test/index.html && ! grep -q '/\*META\*/\|/\*LIB\*/' build/site-test/index.html"
fi

# the sanitizers: the whole app, through every script and two random ones (tests/kf/fuzz.py: buttons, encoders, keys,
# MIDI of every kind), with AddressSanitizer and UndefinedBehaviorSanitizer; any out-of-bounds access, overflow or
# undefined shift stops the run
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g -O1"
mkdir -p build/san
if echo 'int main(void) { return 0; }' | gcc $SAN -x c - -o build/san/probe 2>/dev/null && build/san/probe; then
    run "the app built with the sanitizers" \
        gcc $SAN -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER='"san"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen \
            -o build/san/vavehero firmware/src/retrodash.c
    for s in default tests/kf/game.txt tests/kf/results.txt fuzz:1 fuzz:2; do
        case $s in
            default) arg= ;;
            fuzz:*) python3 tests/kf/fuzz.py "${s#fuzz:}" 45 > "build/san/fuzz${s#fuzz:}.txt"; arg="build/san/fuzz${s#fuzz:}.txt" ;;
            *) arg=$s ;;
        esac
        run "under the sanitizers: ${s}" sh -c "build/san/vavehero build/san/run ${arg} >/dev/null 2>build/san/err.txt || { tail -30 build/san/err.txt; exit 1; }"
    done
    rm -f build/san/run-*.ppm build/san/run.wav
    run "the engine and the rules with the sanitizers: the patch tour, the worst case, every song on autoplay" sh -c "
        gcc $SAN -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/san/kf_render tests/kf/kf_render.c -lm &&
        gcc $SAN -Wall -Wno-unused-function -Ifirmware/src -Ifirmware/gen -o build/san/kf_game_test tests/kf/kf_game_test.c &&
        build/san/kf_render tour build/san/tour.wav build/san/tour.json >/dev/null &&
        build/san/kf_render stress build/san/stress.wav >/dev/null &&
        build/san/kf_game_test >/dev/null"
    rm -f build/san/*.wav
else
    echo "== (no sanitizers here: gcc -fsanitize=address,undefined does not build or run)"
fi

[ $fail = 0 ] && echo "run_vavehero: all passed" || { echo "run_vavehero: FAILED"; exit 1; }
