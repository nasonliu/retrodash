/* Offline-only snes_spc instrumentation; no emulator code enters FM-1 firmware. */
#pragma once
void rd_spc_dsp_write(int clock, int reg, int value);
#define SPC_DSP_WRITE_HOOK(t, r, v) rd_spc_dsp_write(t, r, v)
