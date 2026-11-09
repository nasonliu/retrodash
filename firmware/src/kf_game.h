/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* VaveHero's rules, as the reference has them (reference/vavehero.html, "the game", "input"): the judgement windows,
 * the combo and its multiplier, holds, the OCT shifts, the OP phrases and OP BOOST, the result and its grade, and
 * the autoplay. No drawing and no hardware here: vavehero.c feeds it key presses stamped with the audio clock (frames,
 * as heard), and tests/kf can play whole songs with it.
 *
 * Time: the song's position is kept in T96 x 256 (1/96 beat, Q8); a beat is 24576. Windows are in frames. */
#pragma once
#include <stdint.h>
#include "kf_song.h"

#define KG_MAXN 400u                             /* chart notes (ENGINE TOUR MED: 188) */
#define KG_MAXO 48u                              /* octave shifts */
#define KG_BEAT 24576                            /* a beat in position units */
#define KG_WIN_PERFECT 1544                      /* frames: 35, 70, 110 ms, and 220 for an OCT shift */
#define KG_WIN_GREAT 3087
#define KG_WIN_GOOD 4851
#define KG_WIN_SHIFT 9702
#define KG_BOOST_BEATS 16

enum { KN_PENDING, KN_HIT, KN_HELD, KN_MISSED, KN_DONE, KN_BROKEN };
enum { KO_PENDING, KO_HIT, KO_AUTO, KO_MISSED };
enum { KJ_NONE, KJ_PERFECT, KJ_GREAT, KJ_GOOD, KJ_MISS, KJ_WRONG, KJ_SHIFT };
static const char *const KJ_TEXT[7] = {"", "PERFECT", "GREAT", "GOOD", "MISS", "WRONG", "SHIFT"};
static const uint8_t KG_WHITE_SEMI[8] = {0, 2, 4, 5, 7, 9, 11, 12};   /* lane -> semitones above the window's C */
static const uint8_t KG_LAYOUT[2][8] = {{7, 9, 11, 12, 14, 16, 18, 19},   /* C4 D4 E4 F4 G4 A4 B4 C5 */
                                        {0, 4, 7, 11, 14, 18, 21, 24}};   /* D2 SPREAD: F3 A3 C4 E4 G4 B4 D5 F5 */

typedef struct {
    const kf_song_t *song;
    uint8_t idx, level, speed, spread;
    const kf_cnote_t *notes;
    uint32_t n, first;                           /* the notes; the first not yet behind the window */
    uint8_t nst[KG_MAXN], adn[KG_MAXN];          /* each note's state; autoplay: 1 pressed, 2 let go */
    uint8_t ost[KG_MAXO];
    uint8_t ph_total[16], ph_hit[16], ph_failed[16];
    uint32_t rr;                                 /* position a frame: 2^32 / (frames a T96 x 256) */
    int32_t score, combo, max_combo, ops;
    int32_t boost_from, boost_to;                /* positions; boost_to < 0: no boost */
    int32_t cnt[5];                              /* PERFECT GREAT GOOD MISS SHIFT */
    int32_t dt_sum, dt_n;                        /* ms, for the mean (the latency) */
    uint8_t judge;                               /* the last judgement, and when (fm1_ms) */
    uint32_t judge_ms, plus_ms;
    int32_t oct;                                 /* the player's octave (OCT- / OCT+) */
    int16_t held[27];                            /* a key's held note, -1 none */
    uint8_t key_pitch[27];                       /* the pitch a key sounds, for its release */
    int32_t pos;                                 /* where the song is (update): what is heard, what is judged */
    int32_t vpos;                                /* where it is drawn: pos, or a little ahead of it (vavehero.c) */
    uint8_t done, auto_on, midi_used;
    struct { uint8_t lane, big; uint32_t ms; } fx[8];   /* bursts at the line: big a hit, else a hold done (green) */
    uint32_t nfx;
    /* the result */
    int32_t acc10, grade, new_best, full, total;
} kg_play_t;
static kg_play_t kg;

static uint32_t kg_ms;                           /* the clock for the judgements' fades (vavehero.c sets it) */
static void kg_sound_on(uint32_t key, uint32_t pitch, int32_t detune, uint32_t bright);   /* vavehero.c */
static void kg_sound_off(uint32_t key, uint32_t pitch);
static uint32_t kg_rand(void) { static uint32_t s = 0x2545F491u; s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

static int32_t kg_pos_at(uint32_t frame)         /* the song's position at a frame */
{
    return (int32_t)(((int64_t)(int32_t)(frame - kf_seq.t0) * kg.rr) >> 16);
}
static uint32_t kg_frame_at_pos(int32_t pos)     /* the frame of a position */
{
    return kf_seq.t0 + (uint32_t)(int32_t)(((int64_t)pos * (int32_t)(kf_seq.fpt >> 8)) >> 16);
}
static uint32_t kg_note_frame(uint32_t i) { return kf_seq_frame(kg.notes[i].t); }
static int32_t kg_oct_at(int32_t pos)            /* octAt: the window at a position */
{
    const kf_song_t *s = kg.song;
    int32_t o = s->oct0;
    uint32_t i;
    for (i = 0; i < s->noct; i++) if (pos >= (int32_t)s->octs[i].t * 256) o += s->octs[i].dir;
    return o;
}
static int32_t kg_mult(void) { return kg.combo >= 30 ? 4 : 1 + kg.combo / 10; }
static int32_t kg_boosting(void) { return kg.boost_to >= 0; }
static int32_t kg_ops_shown(void)
{
    if (kg_boosting()) {
        int32_t v = (6 * (kg.boost_to - kg.pos) + KG_BOOST_BEATS * KG_BEAT - 1) / (KG_BOOST_BEATS * KG_BEAT);
        return v < 0 ? 0 : v > 6 ? 6 : v;
    }
    return kg.ops;
}
static uint32_t kg_bright(void)                  /* the player's voice opens with the combo and in a boost, Q12 */
{
    int32_t c = kg.combo > 40 ? 40 : kg.combo, b = 4096 + c * 2048 / 40;
    return (uint32_t)(kg_boosting() ? b * 17 / 10 : b);
}
static int32_t kg_ph(uint32_t i) { return (kg.notes[i].flags >> 4) - 1; }   /* a note's OP phrase, -1 none */
static void kg_set_judge(uint32_t j) { kg.judge = (uint8_t)j; kg.judge_ms = kg_ms | 1u; }
static void kg_fail_phrase(uint32_t i) { int32_t ph = kg_ph(i); if (ph >= 0) kg.ph_failed[ph] = 1; }
static int32_t kg_round(int32_t num, int32_t den) { return (num + den / 2) / den; }

static void kg_start(uint32_t idx, uint32_t speed, uint32_t spread, uint32_t t0)   /* the song's own level (D12) */
{
    const kf_song_t *s = &KF_SONG[idx];
    uint32_t i;
    kg.song = s; kg.idx = (uint8_t)idx; kg.level = s->level; kg.speed = (uint8_t)speed; kg.spread = (uint8_t)spread;
    kg.notes = s->chart;
    kg.n = s->nchart;
    if (kg.n > KG_MAXN) kg.n = KG_MAXN;
    kg.first = 0;
    for (i = 0; i < kg.n; i++) { kg.nst[i] = KN_PENDING; kg.adn[i] = 0; }
    for (i = 0; i < KG_MAXO; i++) kg.ost[i] = KO_PENDING;
    for (i = 0; i < 16u; i++) { kg.ph_total[i] = 0; kg.ph_hit[i] = 0; kg.ph_failed[i] = 0; }
    for (i = 0; i < kg.n; i++) { int32_t ph = kg_ph(i); if (ph >= 0) kg.ph_total[ph]++; }
    kg.score = kg.combo = kg.max_combo = kg.ops = 0;
    kg.boost_from = kg.boost_to = -1;
    for (i = 0; i < 5u; i++) kg.cnt[i] = 0;
    kg.dt_sum = kg.dt_n = 0;
    kg.judge = KJ_NONE; kg.plus_ms = 0;
    kg.oct = s->oct0;
    for (i = 0; i < 27u; i++) kg.held[i] = -1;
    kg.done = 0; kg.midi_used = 0; kg.nfx = 0;
    kf_seq_start(s, t0, speed);
    kg.rr = 0xFFFFFFFFu / (kf_seq.fpt >> 8);
    kg.pos = kg.vpos = kg_pos_at(t0);
}

#ifndef KG_HIT_FEEDBACK
#define KG_HIT_FEEDBACK(index, judge) ((void)0)
#endif
static void kg_hit(uint32_t i, int32_t dt, uint32_t key)
{
    int32_t a = dt < 0 ? -dt : dt, j = a <= KG_WIN_PERFECT ? KJ_PERFECT : a <= KG_WIN_GREAT ? KJ_GREAT : KJ_GOOD, ph;
    static const int32_t BASE[4] = {0, 100, 70, 40};
    kg.cnt[j - 1]++;
    kg.dt_sum += dt * 10 / 441; kg.dt_n++;                     /* ms */
    kg.combo++;
    if (kg.combo > kg.max_combo) kg.max_combo = kg.combo;
    kg.score += BASE[j] * kg_mult() * (kg_boosting() ? 2 : 1);
    kg_set_judge((uint32_t)j);
    KG_HIT_FEEDBACK(i,j);
    kg.fx[kg.nfx % 8u].lane = kg.notes[i].lane; kg.fx[kg.nfx % 8u].big = 1;
    kg.fx[kg.nfx % 8u].ms = kg_ms; kg.nfx++;
    if (kg.notes[i].flags & 1u) { kg.nst[i] = KN_HELD; if (key < 27u) kg.held[key] = (int16_t)i; }
    else kg.nst[i] = KN_HIT;
    ph = kg_ph(i);
    if (ph >= 0 && ++kg.ph_hit[ph] == kg.ph_total[ph] && !kg.ph_failed[ph] && !kg_boosting()) {
        kg.ops = kg.ops + 2 > 6 ? 6 : kg.ops + 2;
        kg.plus_ms = kg_ms | 1u;
    }
}
static void kg_miss(uint32_t i)
{
    kg.nst[i] = KN_MISSED;
    kg.cnt[3]++;
    kg.combo = 0;
    kg_fail_phrase(i);
    kg_set_judge(KJ_MISS);
}
static void kg_hold_end(uint32_t i, int32_t frac)   /* frac Q12 */
{
    uint32_t k;
    kg.nst[i] = frac >= 3687 ? KN_DONE : KN_BROKEN;     /* .9 */
    kg.score += kg_round(50 * frac * kg_mult() * (kg_boosting() ? 2 : 1), 4096);
    if (kg.nst[i] == KN_DONE) {
        kg.fx[kg.nfx % 8u].lane = kg.notes[i].lane; kg.fx[kg.nfx % 8u].big = 0;
        kg.fx[kg.nfx % 8u].ms = kg_ms; kg.nfx++;
    }
    for (k = 0; k < 27u; k++) if (kg.held[k] == (int16_t)i) kg.held[k] = -1;
}

/* a key (0 = F3 .. 26 = G5) at a frame as heard: only the eight keys of the layout do anything (D1, D2) */
static void kg_press(uint32_t key, uint32_t frame)
{
    int32_t lane = -1, s, pitch, near = -1, best = -1, best_dt = 0, near_dt = 0;
    int fixed = kg.song->transpose == 127;
    uint32_t i;
    if (fixed && key < 27u) lane = (int32_t)key;
    else for (i = 0; i < 8u; i++) if (KG_LAYOUT[kg.spread ? 1 : 0][i] == key) lane = (int32_t)i;
    if (lane < 0 || kg.done) return;
    s = fixed ? 0 : KG_WHITE_SEMI[lane];
    pitch = fixed ? -1 : 60 + s + 12 * kg.oct;
    if (fixed) for (i = 0; i < kg.n; i++) if (kg.notes[i].lane == key) { pitch = kg.notes[i].pitch + 12 * kg.notes[i].oct; break; }
    if (pitch < 0) return;
    for (i = kg.first; i < kg.n; i++) {          /* judgePress */
        int32_t dt = (int32_t)(frame - kg_note_frame(i)), ok, ns;
        if (dt < -KG_WIN_GOOD) break;
        if (kg.nst[i] != KN_PENDING || dt > KG_WIN_GOOD) continue;
        if (near < 0 || (dt < 0 ? -dt : dt) < (near_dt < 0 ? -near_dt : near_dt)) { near = (int32_t)i; near_dt = dt; }
        ns = kg.notes[i].pitch - 60 - 12 * kg.notes[i].oct;
        ok = fixed ? kg.notes[i].lane == key : kg.level == 0 ? ((s % 12) == ((ns % 12) + 12) % 12) : kg.notes[i].pitch == pitch;
        if (ok && (best < 0 || (dt < 0 ? -dt : dt) < (best_dt < 0 ? -best_dt : best_dt))) { best = (int32_t)i; best_dt = dt; }
    }
    if (best >= 0) {
        kg_hit((uint32_t)best, best_dt, key);
        kg_sound_on(key, (uint32_t)(pitch + (fixed ? 0 : kg.song->transpose)), 0, kg_bright());
    } else if (near >= 0) {                      /* the wrong key: the combo goes, the voice sours */
        kg.combo = 0;
        kg_fail_phrase((uint32_t)near);
        kg_set_judge(KJ_WRONG);
        kg_sound_on(key, (uint32_t)(pitch + (fixed ? 0 : kg.song->transpose)), kg_rand() & 1u ? 28 : -28, 1434);   /* bright .35 */
    } else kg_sound_on(key, (uint32_t)(pitch + (fixed ? 0 : kg.song->transpose)), 0, kg_bright());
}
static void kg_release(uint32_t key, uint32_t frame)
{
    int32_t i;
    if (key >= 27u) return;
    kg_sound_off(key, 0);
    i = kg.held[key];
    if (i < 0) return;
    kg.held[key] = -1;
    if (kg.nst[i] == KN_HELD) {
        int32_t len = (int32_t)kf_seq_len(kg.notes[i].len), d = (int32_t)(frame - kg_note_frame((uint32_t)i));
        int32_t frac = len <= 0 || d >= len ? 4096 : d <= 0 ? 0 : (d << 12) / len;   /* how much of it was held (d < 2^19) */
        kg_hold_end((uint32_t)i, frac > 4096 ? 4096 : frac);
    }
}
static void kg_oct_press(int32_t dir, uint32_t frame)
{
    const kf_song_t *s = kg.song;
    uint32_t i;
    kg.oct += dir;
    if (kg.oct < -1) kg.oct = -1;
    if (kg.oct > 2) kg.oct = 2;
    if (kg.done) return;
    for (i = 0; i < s->noct && i < KG_MAXO; i++) {
        int32_t dt = (int32_t)(frame - kf_seq_frame(s->octs[i].t));
        if (kg.ost[i] != KO_PENDING || s->octs[i].dir != dir || dt > KG_WIN_SHIFT || dt < -KG_WIN_SHIFT) continue;
        kg.ost[i] = KO_HIT;
        kg.cnt[4]++;
        kg.combo++;
        if (kg.combo > kg.max_combo) kg.max_combo = kg.combo;
        kg.score += 50 * kg_mult() * (kg_boosting() ? 2 : 1);
        kg_set_judge(KJ_SHIFT);
        kg.oct = kg_oct_at((int32_t)s->octs[i].t * 256 + 26);   /* the window just after it (t + .001 beat) */
        return;
    }
}
static void kg_boost(void)                       /* FX: OP BOOST, when all six OPs are lit */
{
    if (kg.ops >= 6 && !kg_boosting() && !kg.done) { kg.boost_from = kg.pos; kg.boost_to = kg.pos + KG_BOOST_BEATS * KG_BEAT; }
}

/* the result: accuracy (PERFECT and SHIFT count whole, GREAT .7, GOOD .4), its grade, a full combo */
static void kg_finish(void)
{
    const kf_song_t *s = kg.song;
    int32_t *c = kg.cnt;
    kg.done = 1;
    kg.total = (int32_t)kg.n + s->noct;
    kg.acc10 = kg.total ? (1000 * (c[0] + c[4]) + 700 * c[1] + 400 * c[2]) / kg.total : 0;
    kg.grade = kg.acc10 >= 970 ? 'S' : kg.acc10 >= 900 ? 'A' : kg.acc10 >= 800 ? 'B' : kg.acc10 >= 700 ? 'C' : 'D';
    kg.full = kg.max_combo == kg.total;
}

/* the autoplay: the game plays itself, exactly on time, through the same paths (the DEMO, the tests) */
static void kg_autoplay(void)
{
    const kf_song_t *s = kg.song;
    uint32_t i;
    for (i = 0; i < s->noct && i < KG_MAXO; i++)
        if (kg.ost[i] == KO_PENDING && kg.pos >= (int32_t)s->octs[i].t * 256) kg_oct_press(s->octs[i].dir, kf_seq_frame(s->octs[i].t));
    for (i = kg.first; i < kg.n; i++) {
        const kf_cnote_t *n = &kg.notes[i];
        uint32_t key = kg.song->transpose == 127 ? n->lane : KG_LAYOUT[kg.spread ? 1 : 0][n->lane];
        if ((int32_t)n->t * 256 > kg.pos) break;
        if (kg.nst[i] == KN_PENDING && !kg.adn[i]) { kg.adn[i] = 1; kg_press(key, kg_note_frame(i)); }
    }
    /* A scored short note can leave kg.first before its sound releases. */
    for (i = 0; i < kg.n; i++) {
        const kf_cnote_t *n = &kg.notes[i];
        uint32_t up = n->t + ((n->flags & 1u) ? n->len : (n->len < 48u ? n->len : 48u)) * 95u / 100u;
        if ((int32_t)n->t * 256 > kg.pos) break;
        if (kg.adn[i] == 1u && kg.pos >= (int32_t)up * 256) { kg.adn[i] = 2; kg_release(kg.song->transpose == 127 ? n->lane : KG_LAYOUT[kg.spread ? 1 : 0][n->lane], kf_seq_frame(up)); }
    }
    if (kg.ops >= 6 && !kg_boosting()) kg_boost();
}

/* the song moves on to this frame (as heard): misses, OCT shifts gone by, holds done, the boost's end, the end */
static uint32_t kg_update(uint32_t frame)        /* 1: the song is over (4 beats after its end) */
{
    const kf_song_t *s = kg.song;
    uint32_t i;
    kg.pos = kg.vpos = kg_pos_at(frame);
    if (kg.auto_on) kg_autoplay();
    for (i = kg.first; i < kg.n; i++) {
        if ((int32_t)(frame - kg_note_frame(i)) < 0) break;
        if (kg.nst[i] == KN_PENDING && (int32_t)(frame - kg_note_frame(i)) > KG_WIN_GOOD) kg_miss(i);
    }
    while (kg.first < kg.n && kg.nst[kg.first] != KN_PENDING && kg.nst[kg.first] != KN_HELD &&
           (int32_t)(frame - kg_note_frame(kg.first)) > KG_WIN_GOOD) kg.first++;
    for (i = 0; i < s->noct && i < KG_MAXO; i++) {
        if (kg.ost[i] != KO_PENDING || (int32_t)(frame - kf_seq_frame(s->octs[i].t)) <= KG_WIN_SHIFT) continue;
        kg.oct = kg_oct_at((int32_t)s->octs[i].t * 256 + 26);
        if (kg.level == 0 || kg.midi_used) { kg.ost[i] = KO_AUTO; kg.cnt[4]++; }
        else { kg.ost[i] = KO_MISSED; kg.cnt[3]++; kg.combo = 0; kg_set_judge(KJ_MISS); }
    }
    for (i = kg.first; i < kg.n; i++)
        if (kg.nst[i] == KN_HELD && kg.pos >= (int32_t)(kg.notes[i].t + kg.notes[i].len) * 256) kg_hold_end(i, 4096);
    if (kg_boosting() && kg.pos >= kg.boost_to) { kg.boost_from = kg.boost_to = -1; kg.ops = 0; }
    if (!kg.done && kg.pos >= (int32_t)s->end * KG_BEAT) kg_finish();
    return kg.done && kg.pos >= ((int32_t)s->end + 4) * KG_BEAT;
}

/* start in the middle (the DEMO): everything before beat `beat` is gone by, unscored; the band's keyswitches, the
 * octave and the window as they are there */
static void kg_seek(uint32_t beat)
{
    const kf_song_t *s = kg.song;
    int32_t from = (int32_t)beat * KG_BEAT;
    uint32_t i;
    for (i = 0; i < kg.n && (int32_t)kg.notes[i].t * 256 < from; i++) { kg.nst[i] = KN_DONE; kg.adn[i] = 2; }
    kg.first = i;
    for (i = 0; i < s->noct && i < KG_MAXO; i++) if ((int32_t)s->octs[i].t * 256 < from) kg.ost[i] = KO_AUTO;
    kg.oct = kg_oct_at(from);
    kf_seq.t0 -= kf_seq_len(beat * 96u);
    while (kf_seq.bi < s->nband && (int32_t)s->band[kf_seq.bi].t * 256 < from) {
        const kf_bev_t *e = &s->band[kf_seq.bi++];
        if (e->ks) kf_seq.patch[e->part] = (uint8_t)(e->ks - 1u);
    }
}
