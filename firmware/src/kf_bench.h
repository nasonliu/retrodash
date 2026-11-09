/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* The engine's worst case, to measure it (tests/kf/kf_render.c stress; the firmware's DEMO, which fm1-emu times):
 * ENGINE TOUR from bar 41, its heaviest stretch (every part on its gritty patch, the pads' long tails, 24 voices
 * busy), the band and the melody as the reference's renderSong plays them. */
#pragma once
#include "kf_engine.h"
#include "kf_song.h"

#ifdef RD_SPC
#define KF_BENCH_SONG 0u
#else
#define KF_BENCH_SONG 2u                         /* ENGINE TOUR */
#endif
static volatile uint32_t kf_bench_beat = 160u;  /* bar 41 (volatile: folded, 160 beats of frames is -3528000, which
                                                  * looks like a ROM address to the build's check, docs/RULES.md) */
static uint32_t kf_bench_mi;                     /* the melody's next note */
static void kf_bench_start(uint32_t t0)          /* bar 41 sounds at frame t0 */
{
    const kf_song_t *s = &KF_SONG[KF_BENCH_SONG];
    uint32_t from = kf_bench_beat * 96u;
    kf_seq_start(s, 0, 100);
    kf_seq.t0 = t0 - kf_seq_len(from);
    while (kf_seq.bi < s->nband && s->band[kf_seq.bi].t < from) {   /* the keyswitches up to bar 41, nothing sounds */
        const kf_bev_t *e = &s->band[kf_seq.bi++];
        if (e->ks) kf_seq.patch[e->part] = (uint8_t)(e->ks - 1u);
    }
    for (kf_bench_mi = 0; kf_bench_mi < s->nchart && s->chart[kf_bench_mi].t < from; kf_bench_mi++) {}
}
static void kf_bench_post(uint32_t upto)
{
    const kf_song_t *s = &KF_SONG[KF_BENCH_SONG];
    const kf_cnote_t *m = s->chart;
    while (kf_bench_mi < s->nchart && (int32_t)(kf_seq_frame(m[kf_bench_mi].t) - upto) < 0) {
        uint32_t t = kf_seq_frame(m[kf_bench_mi].t);
        kf_seq_post(t + 1u);
        kf_sched_note(t, KF_P_PLAY, kf_seq_play_patch(), (uint32_t)(m[kf_bench_mi].pitch + s->transpose), 104,
                      kf_seq_len(m[kf_bench_mi].len) * 19u / 20u);
        kf_bench_mi++;
    }
    kf_seq_post(upto);
    if (kf_seq.bi >= s->nband && kf_bench_mi >= s->nchart) kf_bench_start(upto + 22050u);   /* the end: again */
}
