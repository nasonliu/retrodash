#!/bin/sh
# Test the FF6 song with its generated audio and scene, then restore selected song.
set -eu
cd "$(dirname "$0")/.."
PYTHON="${PYTHON:-python3}"
song_backup=$(mktemp -d)
for file in rd_spc_data.h cq_spc_demo.h kf_spc_song.h; do cp "firmware/gen/$file" "$song_backup/$file"; done
restore_song() {
 for file in rd_spc_data.h cq_spc_demo.h kf_spc_song.h; do cp "$song_backup/$file" "firmware/gen/$file"; done
 rm -rf "$song_backup"
}
trap restore_song EXIT HUP INT TERM
"$PYTHON" tools/gen_spc_firmware.py projects/ff6-terra-snow
mkdir -p build/ff6-test
CXXFLAGS="-std=c++11 -O2 -Ifirmware/src -Ifirmware/gen -Ifirmware/vendor/snes_spc"
c++ $CXXFLAGS -DNDEBUG -c firmware/vendor/snes_spc/fast/SPC_DSP.cpp -o build/ff6-test/dsp.o
c++ $CXXFLAGS -fsanitize=address,undefined -DDINGHY_HOST -DRD_SPC_FAST -c firmware/src/rd_spc.cpp -o build/ff6-test/spc.o
for test in spc_game ff6_scene; do
 cc -O1 -g -fsanitize=address,undefined -DRD_SPC -DEXPECTED_TARGETS=78 -DEXPECTED_FILLS=7 -DTEST_SECONDS=119 -DAPP_VER='"ff6-test"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -c "tests/chipquest/test_$test.c" -o "build/ff6-test/$test.o"
 c++ -fsanitize=address,undefined "build/ff6-test/$test.o" build/ff6-test/spc.o build/ff6-test/dsp.o -o "build/ff6-test/$test"
 "build/ff6-test/$test"
done
