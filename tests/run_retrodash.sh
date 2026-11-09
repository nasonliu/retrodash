#!/bin/sh
# Focused Linux host checks for the shipped SPC game and its native renderer.
set -eu
cd "$(dirname "$0")/.."
PYTHON="${PYTHON:-python3}"
"$PYTHON" -m unittest discover -s tests/chipquest -p 'test_*.py'
mkdir -p build/tests
CXXFLAGS="-std=c++11 -O2 -Ifirmware/src -Ifirmware/gen -Ifirmware/vendor/snes_spc"
c++ $CXXFLAGS -DNDEBUG -c firmware/vendor/snes_spc/fast/SPC_DSP.cpp -o build/tests/dsp.o
c++ $CXXFLAGS -fsanitize=address,undefined -DDINGHY_HOST -DRD_SPC_FAST -c firmware/src/rd_spc.cpp -o build/tests/spc.o
for test in scaled_render parallax spc_game; do
    cc -O1 -g -fsanitize=address,undefined -DRD_SPC -DAPP_VER='"test"' \
       -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -c "tests/chipquest/test_$test.c" -o "build/tests/$test.o"
    c++ -fsanitize=address,undefined "build/tests/$test.o" build/tests/spc.o build/tests/dsp.o -o "build/tests/$test"
    "build/tests/$test"
done
