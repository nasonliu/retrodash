/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void rd_spc_init(void);
void rd_spc_start(uint32_t frame,uint32_t speed);
void rd_spc_stop(void);
void rd_spc_pause(uint32_t paused);
void rd_spc_hit(uint32_t index);
void rd_spc_mix(uint32_t backing);
void rd_spc_render(int32_t *out,uint32_t frames,uint32_t now);
uint32_t rd_spc_status(uint32_t field);
#ifdef __cplusplus
}
#endif
