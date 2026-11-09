/* SPDX-License-Identifier: GPL-3.0-only
 * Existing game/scheduler API for the embedded SPC validation song.
 * Music is driven entirely by the SPC timeline; MIDI band events are unused.
 */
#pragma once
#include "kf_tables.h"
#include "rd_spc.h"
#define KF_FS 44100u
#define KF_NV 1u
static struct {unsigned state;} kf_v[KF_NV];
static int16_t kf_sine[4096];
static volatile uint32_t kf_now;
static int32_t kf_gp=5734,kf_gb=1228;
enum{KF_EV_ON=1,KF_EV_OFF,KF_EV_DRUM,KF_EV_ALLOFF,KF_EV_KILL};
typedef struct{uint32_t t;uint8_t type,part,pitch,vel,patch;int8_t detune;uint16_t bright;}kf_ev_t;
static int kf_post(const kf_ev_t *e){(void)e;return 0;}
static void kf_note_on(uint32_t t,uint32_t p,uint32_t patch,uint32_t pitch,uint32_t v,int32_t d,uint32_t b){(void)t;(void)p;(void)patch;(void)pitch;(void)v;(void)d;(void)b;}
static void kf_note_off(uint32_t t,uint32_t p,uint32_t pitch){(void)t;(void)p;(void)pitch;}
static void kf_drum(uint32_t t,uint32_t k,uint32_t n,uint32_t v){(void)t;(void)k;(void)n;(void)v;}
static void kf_sched_clear(void){}
static void kf_sched_flush(uint32_t t){(void)t;}
static void kf_sched_note(uint32_t t,uint32_t p,uint32_t patch,uint32_t pitch,uint32_t v,uint32_t len){(void)t;(void)p;(void)patch;(void)pitch;(void)v;(void)len;}
static void kf_set_tempo(uint32_t t){(void)t;}
static void kf_set_grit(uint32_t g){(void)g;}
static void kf_set_mix(int32_t m){(void)m;}
static void kf_set_game_balance(uint32_t b){kf_gb=(int32_t)(b*4096/100);rd_spc_mix(b);}
static int32_t kf_sat16(int32_t x){return x>32767?32767:x< -32768?-32768:x;}
static void kf_render(int32_t *out,uint32_t n){rd_spc_render(out,n,kf_now);kf_now+=n;}
static void kf_init(void){for(unsigned i=0;i<4096;i++)kf_sine[i]=KF_SINE[i];rd_spc_init();}
