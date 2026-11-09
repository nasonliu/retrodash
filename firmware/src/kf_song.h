/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* A song's band, played: firmware/gen/kf_songs.h's events into the engine, a little ahead of the speaker, as the
 * reference's scheduler does (its setInterval with a 0.15 s horizon). The keyswitches move each part's patch (D6);
 * channel 1's pick the player's voice (TONE = SONG). The main loop calls kf_seq_post(kf_now + lookahead). */
#pragma once
#include "kf_engine.h"
#ifdef RD_SPC
#include "kf_spc_song.h"
#else
#include "kf_songs.h"
#endif

typedef struct {
    const kf_song_t *s;
    uint32_t t0;                                 /* the frame of beat 0 */
    uint32_t fpt;                                /* frames a T96, Q16 (the song's tempo times the speed) */
    uint32_t bi;                                 /* the next band event */
    uint8_t patch[KF_NPART];                     /* each part's patch, an index into its own (0 = the first) */
    uint8_t on;
} kf_seq_t;
static kf_seq_t kf_seq;

static uint32_t kf_seq_frame(uint32_t t96) { return kf_seq.t0 + (uint32_t)(((uint64_t)t96 * kf_seq.fpt) >> 16); }
static uint32_t kf_seq_len(uint32_t t96) { return (uint32_t)(((uint64_t)t96 * kf_seq.fpt) >> 16); }
static void kf_seq_start(const kf_song_t *s, uint32_t t0, uint32_t speed_pct)   /* speed 50..100 % */
{
    uint32_t i;
    kf_seq.s = s;
    kf_seq.t0 = t0;
    kf_seq.fpt = speed_pct >= 100u || !speed_pct ? s->fpt_q16 : s->fpt_q16 / speed_pct * 100u;
    kf_seq.bi = 0;
    for (i = 0; i < KF_NPART; i++) kf_seq.patch[i] = 0;
    kf_seq.on = 1;
    kf_sched_clear();
    kf_set_tempo(s->bpm_x100 * (speed_pct ? speed_pct : 100u) / 100u);
#ifdef RD_SPC
    rd_spc_start(t0,speed_pct);
#endif
}
static void kf_seq_stop(void)
{
    kf_ev_t e = {0, KF_EV_ALLOFF, 0, 0, 0, 0, 0, 0};
    kf_seq.on = 0;
    kf_sched_clear();
    kf_post(&e);
#ifdef RD_SPC
    rd_spc_stop();
#endif
}
static void kf_seq_post(uint32_t upto)           /* every band event before this frame, to the engine */
{
    const kf_song_t *s = kf_seq.s;
    if (!kf_seq.on) return;
    while (kf_seq.bi < s->nband) {
        const kf_bev_t *e = &s->band[kf_seq.bi];
        uint32_t t = kf_seq_frame(e->t), part = e->part;
        if ((int32_t)(t - upto) >= 0) break;
        if (e->ks) kf_seq.patch[part] = (uint8_t)(e->ks - 1u);
        else if (part == KF_P_DRUMS) { kf_sched_flush(t + 1u); kf_drum(t, kf_seq.patch[part] < KF_NKIT ? kf_seq.patch[part] : 0u, e->pitch, e->vel); }
        else { /* Includes automatic fill notes on the foreground melody part. */
            uint32_t len = kf_seq_len(e->len) * 49u / 50u;   /* .98 of its length, as playEvent */
            kf_sched_note(t, part, KF_PART_FIRST[part] + kf_seq.patch[part], e->pitch, e->vel, len ? len : 1u);
        }
        kf_seq.bi++;
    }
    kf_sched_flush(upto);
}
static uint32_t kf_seq_play_patch(void) { return KF_PART_FIRST[KF_P_PLAY] + kf_seq.patch[KF_P_PLAY]; }   /* the player's voice now */
