/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* VaveHero's synthwave engine in fixed point: the reference's WebAudio engine (reference/vavehero.html, "sound") as
 * the FM-1 can run it in its audio interrupt. The patches and the drum kits come from the reference through
 * tools/gen_kf_tables.mjs (firmware/gen/kf_tables.h); this file is the machinery.
 *
 *   voices    24, each one note of a patch: up to 4 oscillators (saw and square with PolyBLEP, triangle) into a
 *             TPT state-variable low-pass with its envelope, drive (a tanh curve) and a crushed copy; or an FM
 *             voice (a sine carrier, 2 sine modulators with their index envelopes). Vibrato, the wrong note's
 *             detune and the combo's brightness as the reference has them.
 *   drums     10 layers: tones with a pitch sweep (kick, syndrum toms, the snare's body) and filtered noise
 *             (the rest), exponential decays; the DIRT kit's drive and crush on the drum bus.
 *   buses     player, band (pumped by the kick: GRIT), chorus (2 modulated taps, into the band); the drum bus
 *             with its saturation (GRIT); a dotted-eighth delay with a low-pass in its feedback; a hall (a
 *             4-line FDN after 4 diffusers, for the reference's 2.8 s convolution); a gated reverb (an FDN
 *             behind a gate the snare and clap open); the mix's glue (tanh, GRIT), make-up gain, a compressor.
 *
 * The control rate is a tick of 32 frames: envelopes, filter coefficients, LFOs, the pump and the compressor's
 * gain move there and are ramped across the tick where a step would click.
 *
 * Threads: kf_render() runs in the audio interrupt. The main loop talks to it only through kf_post() (a queue,
 * one writer, one reader) and the kf_set_*() values, which are single 32-bit stores. */
#pragma once
#ifdef RD_SPC
#include "rd_spc_engine.h"
#else
#include <stdint.h>
#include "kf_tables.h"
/* the tables the audio interrupt reads every frame, in RAM (copied in kf_init): read from flash they miss the flash
 * cache (fm1-live with FM1_STRICT: KF_SINE and KF_DRIVE were a quarter of the misses). The same numbers */
static int16_t kf_sine[4096];
static int16_t kf_drive[KF_NDRIVE][257];

#ifndef KF_POOL
#define KF_POOL __attribute__((section(".pool")))   /* the big lines (zeroed at boot: platform.c's pool) */
#endif

#define KF_NV 24u                                /* voices */
#ifndef KF_OSC_BUDGET
#define KF_OSC_BUDGET 48u                        /* oscillators the voices may run at once; above it the quietest release
                                                  * tails fade out fast (25 ms): the pads' long tails under new chords */
#endif
#define KF_NL 10u                                /* drum layers */
#define KF_FS 44100u

/* ---- events: what the main loop asks for, and when (kf_now's frames; 0 = at once) */
enum { KF_EV_ON = 1, KF_EV_OFF, KF_EV_DRUM, KF_EV_ALLOFF, KF_EV_KILL };
typedef struct {
    uint32_t t;                                  /* the frame it sounds at (KF_EV_ON etc. on the timed queue) */
    uint8_t type, part, pitch, vel;              /* DRUM: pitch = the GM note */
    uint8_t patch;                               /* ON: the patch id (KF_PATCH), DRUM: the kit (KF_KIT) */
    int8_t detune;                               /* ON: cents at the start, gliding to 0 over 0.35 s (a wrong note) */
    uint16_t bright;                             /* ON: Q12, 4096 = 1 (the filter and the FM index open with it) */
} kf_ev_t;

#define KF_QN 256u                               /* a queue's events (a power of two) */
static kf_ev_t kf_q[2][KF_QN];                   /* 0: at once, 1: timed, in order */
static volatile uint32_t kf_qw[2], kf_qr[2];
static volatile uint32_t kf_now;                 /* frames rendered so far */

/* the settings (main loop writes, the interrupt reads) */
static volatile int32_t kf_gp = 4506, kf_gb = 3686;   /* Q12: player .5 + mix, band 1.5 - mix (MIX 60) */
static volatile int32_t kf_dly_len = 17719;      /* frames: a dotted eighth (112 BPM) */
static volatile int32_t kf_pump_depth;           /* Q15 */
static volatile int32_t kf_makeup = 4096;        /* Q12 */
static volatile uint32_t kf_glue_sel;            /* which of the two glue tables is live */
static int16_t kf_glue[2][2][257];               /* [sel][0 mix, 1 drums]: tanh(a x) / a, Q15 */

/* ---- integer helpers for the main loop and note-on (not per frame) */
static const uint16_t KF_EXP2_FRAC[17] = {      /* 2^(i/16), Q15 */
    32768, 34219, 35734, 37316, 38968, 40693, 42495, 44376, 46341, 48393, 50535, 52773, 55109, 57549, 60097, 62757, 65535};
static uint32_t kf_exp2_q16(int32_t x)           /* 2^(x / 65536), Q16, for x in -16..15 */
{
    int32_t n = x >> 16;
    uint32_t f = (uint32_t)x & 0xFFFFu, i = f >> 12, r = f & 0xFFFu;
    uint32_t m = KF_EXP2_FRAC[i] + (((KF_EXP2_FRAC[i + 1u] - KF_EXP2_FRAC[i]) * r) >> 12);   /* Q15, 1..2 */
    if (n >= 1) return m << (n + 1);
    if (n >= -16) return (m << 1) >> (-n);
    return 0;
}
static int32_t kf_exp2_small_q24(int32_t x)      /* 2^(x / 2^24) for |x| < 2^23 (a tick's step), Q24 */
{
    int32_t y = (int32_t)(((int64_t)x * 11629080) >> 24);   /* x ln 2 */
    int32_t y2 = (int32_t)(((int64_t)y * y) >> 24), y3 = (int32_t)(((int64_t)y2 * y) >> 24);
    return 16777216 + y + (y2 >> 1) + y3 / 6;
}
static int32_t kf_tanh_q15(int32_t z)            /* tanh(z / 4096), Q15 */
{
    int32_t a = z < 0 ? -z : z;
    uint32_t e;
    if (a > 8 * 4096) return z < 0 ? -32767 : 32767;
    e = kf_exp2_q16((int32_t)(((int64_t)a * 2 * 94548) >> 12));   /* e^(2a) = 2^(2a / ln 2), Q16 (94548 = 2^16 / ln 2) */
    a = 32768 - (int32_t)(0xFFFFFFFFu / (e + 65536u));             /* 1 - 2 / (e + 1) */
    return z < 0 ? -a : a;
}

/* ---- the voices */
typedef struct {
    uint8_t state;                               /* 0 free, 1 held, 2 released */
    uint8_t fast;                                /* released and fading fast: the oscillator budget wants it */
    uint8_t patch, part, pitch;
    uint32_t age;
    int32_t t, rt, rel_at;                       /* ticks since the start; since the release; the release's tick */
    uint32_t ph[4], inc[4], base[4], rinc[4];    /* rinc: 2^47 / inc (the BLEP's 1 / dt) */
    int32_t det;                                 /* cents Q8 at the start (a wrong note) */
    int32_t ic1, ic2, a1, a2, a3;                /* the filter */
    int32_t fcut, fpk, fval, fstep, fcoef;       /* Hz Q8: at rest, the peak, now, the swell's step a tick; the coefficients' */
    int32_t A, env, envd, att_step;              /* Q24: the peak level, now, a frame's step, the attack's step a tick */
    uint32_t mph[2], minc[2];                    /* FM: the modulators */
    int32_t mi[2], mtgt[2];                      /* FM: the index now and sustained, Q12 */
    uint32_t cph, cinc, cbase;                   /* FM: the carrier */
} kf_voice_t;
static kf_voice_t kf_v[KF_NV];
static uint32_t kf_age;

typedef struct {
    uint8_t on, def;                             /* def: KF_LAYER */
    int32_t t, at, end;
    uint32_t ph, inc;
    int32_t g, gd, m;                            /* Q24: the level, a frame's step, the fall a tick */
    int32_t ic1, ic2;
} kf_layer_rt_t;
static kf_layer_rt_t kf_l[KF_NL];
static uint8_t kf_kit;                           /* the drum bus's kit: the last hit's */

static uint32_t kf_noise_s = 0x9E3779B9u;
static inline int32_t kf_noise(void)
{
    uint32_t x = kf_noise_s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    kf_noise_s = x;
    return (int32_t)(int16_t)(x >> 16);
}

/* the pump, the compressor, the FX's activity */
static int32_t kf_duck = 32768, kf_duck_d, kf_duck_t = 1 << 20;   /* Q15, a frame's step; ticks since the kick */
static int32_t kf_cgain = 4096, kf_cgain_d;       /* the compressor's gain, Q12, and a frame's step (<< 8) */
static int32_t kf_cgain_x8 = 4096 << 8;
static int32_t kf_rev_live, kf_gate_live, kf_dly_live, kf_cho_live;   /* ticks left */
static int32_t kf_gate_open;                      /* ticks left of the gate (snare, clap) */
static int32_t kf_gate_g;                         /* Q15: the gate's level */

/* the lines (the pool) */
#define KF_DLY_N 32768u                           /* the delay: 0.74 s, a dotted eighth down to 61 BPM */
#define KF_CHO_N 2048u
static int16_t kf_dly[KF_DLY_N] KF_POOL;
static int16_t kf_cho[KF_CHO_N] KF_POOL;
static uint32_t kf_dly_w, kf_cho_w;
static int32_t kf_dly_ic1, kf_dly_ic2;           /* its low-pass */
static uint32_t kf_cho_ph[2];

/* the hall: 4 diffusers, then 4 lines with a Hadamard matrix */
static const uint16_t KF_AP_LEN[4] = {142, 107, 379, 277};
static const uint16_t KF_RV_LEN[4] = {1931, 2459, 2953, 3407};
static const uint16_t KF_GT_LEN[4] = {601, 733, 863, 1009};
static int16_t kf_ap[4][380] KF_POOL;
static int16_t kf_rv[4][3408] KF_POOL;
static int16_t kf_gt[4][1010] KF_POOL;
static uint32_t kf_ap_i[4], kf_rv_i[4], kf_gt_i[4];
static int32_t kf_rv_g[4], kf_gt_g[4];            /* Q15: each line's loop gain (the decay time) */

/* the mix's buffers for one tick */
static int32_t kf_pb[KF_NPATCH][KF_TICK];         /* each patch's voices, before its bus and sends */
static uint32_t kf_pb_used;                       /* bit: a patch has sound in kf_pb this tick */

/* ---- the main loop's side */
static int kf_post(const kf_ev_t *e)             /* 0: queued; -1: full (dropped) */
{
    uint32_t q = e->t ? 1u : 0u, w = kf_qw[q];
    if (w - kf_qr[q] >= KF_QN) return -1;
    kf_q[q][w % KF_QN] = *e;
    kf_qw[q] = w + 1u;                           /* (published after the event is written) */
    return 0;
}
static void kf_note_on(uint32_t t, uint32_t part, uint32_t patch, uint32_t pitch, uint32_t vel, int32_t detune, uint32_t bright)
{
    kf_ev_t e = {t, KF_EV_ON, (uint8_t)part, (uint8_t)pitch, (uint8_t)vel, (uint8_t)patch, (int8_t)detune, (uint16_t)bright};
    kf_post(&e);
}
static void kf_note_off(uint32_t t, uint32_t part, uint32_t pitch)
{
    kf_ev_t e = {t, KF_EV_OFF, (uint8_t)part, (uint8_t)pitch, 0, 0, 0, 0};
    kf_post(&e);
}
static void kf_drum(uint32_t t, uint32_t kit, uint32_t note, uint32_t vel)
{
    kf_ev_t e = {t, KF_EV_DRUM, KF_P_DRUMS, (uint8_t)note, (uint8_t)vel, (uint8_t)kit, 0, 0};
    kf_post(&e);
}
/* notes with a length: the note-offs wait here and go to the queue in time order (the timed queue must be in
 * order). Post the note-ons in order, kf_sched_flush(t) before each, and kf_sched_flush(now + lookahead) as time goes. */
#define KF_NOFF 64u
static struct { uint32_t t; uint8_t part, pitch; } kf_offs[KF_NOFF];
static uint32_t kf_noffs;
static void kf_sched_flush(uint32_t upto)       /* post every note-off due before upto, earliest first */
{
    for (;;) {
        uint32_t i, best = KF_NOFF;
        for (i = 0; i < kf_noffs; i++)
            if ((int32_t)(kf_offs[i].t - upto) < 0 && (best == KF_NOFF || (int32_t)(kf_offs[i].t - kf_offs[best].t) < 0)) best = i;
        if (best == KF_NOFF) return;
        kf_note_off(kf_offs[best].t, kf_offs[best].part, kf_offs[best].pitch);
        kf_offs[best] = kf_offs[--kf_noffs];
    }
}
static void kf_sched_note(uint32_t t, uint32_t part, uint32_t patch, uint32_t pitch, uint32_t vel, uint32_t len)
{
    kf_sched_flush(t + 1u);
    if (kf_noffs == KF_NOFF) kf_sched_flush(kf_offs[0].t + 1u);   /* full: the earliest goes now */
    kf_note_on(t, part, patch, pitch, vel, 0, 4096);
    kf_offs[kf_noffs].t = t + len;
    kf_offs[kf_noffs].part = (uint8_t)part;
    kf_offs[kf_noffs].pitch = (uint8_t)pitch;
    kf_noffs++;
}
static void kf_sched_clear(void) { kf_noffs = 0; }

static void kf_set_mix(int32_t mix)              /* 0..100: the player against the band (KNOB 4) */
{
    kf_gp = (int32_t)((50 + mix) * 4096 / 100);
    kf_gb = (int32_t)((150 - mix) * 4096 / 100);
}
/* Rhythm-game mix: foreground melody stays clear above a dense MIDI arrangement. */
static void kf_set_game_balance(uint32_t backing){
    if(backing>100u)backing=100u;
    kf_gp=5734; /* 1.4, before the existing compressor */
    kf_gb=(int32_t)(backing*4096u/100u);
}
static void kf_set_tempo(uint32_t bpm_x100)      /* the delay: a dotted eighth */
{
    uint32_t n = bpm_x100 ? 198450000u / bpm_x100 : 17719u;   /* 0.75 * 60 / bpm * 44100 */
    kf_dly_len = (int32_t)(n > KF_DLY_N - 64u ? KF_DLY_N - 64u : n);
}
static void kf_glue_make(int16_t *t, int32_t a_q12)   /* tanh(a x) / a over x = -1..1 */
{
    uint32_t i;
    for (i = 0; i <= 256u; i++) {
        int32_t x = (int32_t)i * 32 - 4096;      /* Q12 */
        int32_t y = kf_tanh_q15((int32_t)(((int64_t)a_q12 * x) >> 12)) * 4096 / a_q12;   /* (32-bit division) */
        t[i] = (int16_t)(y > 32767 ? 32767 : y < -32767 ? -32767 : y);
    }
}
static void kf_set_grit(uint32_t gr)             /* 0..100 (D7): the pump, the glue, the drums' crunch */
{
    uint32_t s = kf_glue_sel ^ 1u;
    if (gr > 100u) gr = 100u;
    kf_glue_make(kf_glue[s][0], (int32_t)(205 + gr * 9830 / 100));    /* .05 + 2.4 gr */
    kf_glue_make(kf_glue[s][1], (int32_t)(205 + gr * 10650 / 100));   /* .05 + 2.6 gr */
    kf_glue_sel = s;
    kf_makeup = (int32_t)(4096 + gr * 2253 / 100);                     /* 1 + .55 gr */
    kf_pump_depth = gr ? (int32_t)(3932 + gr * 15729 / 100) : 0;       /* .12 + .48 gr */
}

static uint32_t kf_log2_q16(uint32_t x)          /* log2(x), Q16, x >= 1 */
{
    static const uint16_t LG[17] = {0, 5732, 11136, 16248, 21098, 25711, 30109, 34312, 38336, 42196, 45904,
                                    49472, 52911, 56229, 59434, 62534, 65535};   /* log2(1 + i / 16), Q16 */
    uint32_t n = 31u - (uint32_t)__builtin_clz(x | 1u), m, i, r;
    m = n >= 16u ? x >> (n - 16u) : x << (16u - n);    /* 1.xxx, Q16 */
    i = (m >> 12) & 15u;
    r = m & 0xFFFu;
    return (n << 16) + LG[i] + (((uint32_t)(LG[i + 1u] - LG[i]) * r) >> 12);
}

/* the TPT state-variable filter's coefficients for f (Hz Q8) and k = 1 / Q (Q15) */
static void kf_svf_coef(int32_t f_q8, int32_t k, int32_t *a1, int32_t *a2, int32_t *a3)
{
    int32_t fi, i, fr, g, den;
    if (f_q8 < 256) f_q8 = 256;
    if (f_q8 > 21800 * 256) f_q8 = 21800 * 256;
    fi = f_q8 >> 8;
    i = fi >> 5;
    fr = fi & 31;
    g = KF_G[i] + (((KF_G[i + 1] - KF_G[i]) * fr) >> 5);
    den = 32768 + (int32_t)(((int64_t)g * (g + k)) >> 15);
    *a1 = (int32_t)((1u << 30) / (uint32_t)den);
    *a2 = (int32_t)(((int64_t)g * *a1) >> 15);
    *a3 = (int32_t)(((int64_t)g * *a2) >> 15);
}
static int32_t kf_dly_a1, kf_dly_a2, kf_dly_a3;   /* the delay's low-pass: 2600 Hz, Q 1 dB */

/* the PolyBLEP's correction for a falling step of 2 at q = 0 (the saw's), for q within a step of it: + (1 - t)^2
 * just after, - (1 - t)^2 just before, t = |q| / inc (Q15) */
static int32_t kf_blep(uint32_t q, uint32_t ri)
{
    int32_t x;
    if ((int32_t)q >= 0) { x = 32768 - (int32_t)(((uint64_t)q * ri) >> 32); return (x * x) >> 15; }
    x = 32768 - (int32_t)(((uint64_t)(0u - q) * ri) >> 32);
    return -((x * x) >> 15);
}
/* q's next KF_TICK frames (q, q + inc, ...) stay out of the step's window, [-inc, inc) around 0: the test the loops
 * below make every frame (q + inc < 2 inc), made once for the tick. The step is two frames a period, so most ticks are
 * clear and take a loop without it; the same numbers either way */
static inline int kf_clear(uint32_t q, uint32_t inc)
{
    return inc < (1u << 26) && q >= inc && q < 0u - (uint32_t)KF_TICK * inc;
}
static inline uint32_t kf_rinc(uint32_t inc)     /* 2^47 / inc (the BLEP's 1 / dt), 32-bit */
{
    uint32_t d = inc >> 15;
    return d ? 0xFFFFFFFFu / d : 0xFFFFFFFFu;
}

/* ---- a note starts, a note stops */
static kf_voice_t *kf_pick(void)
{
    kf_voice_t *best = 0;
    uint32_t i;
    for (i = 0; i < KF_NV; i++)
        if (!kf_v[i].state) return &kf_v[i];
    for (i = 0; i < KF_NV; i++)                  /* the quietest released voice */
        if (kf_v[i].state == 2u && (!best || kf_v[i].env < best->env)) best = &kf_v[i];
    if (best) return best;
    for (i = 0; i < KF_NV; i++)                  /* else the oldest */
        if (!best || kf_v[i].age < best->age) best = &kf_v[i];
    return best;
}
static void kf_on(const kf_ev_t *e)
{
    const kf_patch_t *p;
    kf_voice_t *v;
    uint32_t inc, o, br = e->bright ? e->bright : 4096u;
    int32_t vel = e->vel > 127 ? 127 : e->vel;
    if (e->patch >= KF_NPATCH) return;
    p = &KF_PATCH[e->patch];
    v = kf_pick();
    v->state = 1;
    v->fast = 0;
    v->patch = e->patch;
    v->part = e->part;
    v->pitch = e->pitch;
    v->age = ++kf_age;
    v->t = 0;
    v->rt = 0;
    v->rel_at = 0;
    v->det = (int32_t)e->detune * 256;
    v->A = (int32_t)(((int64_t)p->amp * (11469 + 21299 * vel / 127)) >> 15);   /* amp (.35 + .65 v) */
    v->env = 0;
    v->envd = 0;
    v->att_step = v->A * 32 / p->att;
    if (v->att_step > v->A || v->att_step <= 0) v->att_step = v->A;
    inc = KF_INC[e->pitch & 127u];
    if (p->fm) {
        v->cph = 0;
        v->cbase = v->cinc = inc;
        for (o = 0; o < 2u; o++) {
            v->mph[o] = p->mph0[o];
            v->minc[o] = (uint32_t)(((uint64_t)inc * p->mratio[o]) >> 16);
            v->mi[o] = (int32_t)(((int64_t)p->midx[o] * (int32_t)br) >> 12);
            v->mtgt[o] = (int32_t)(((int64_t)v->mi[o] * p->msus[o]) >> 15);
        }
    } else {
        int32_t cut = (int32_t)(((int64_t)p->cut * (int32_t)br) >> 4), pk = (int32_t)(((int64_t)(p->cut + p->env) * (int32_t)br) >> 4);
        if (cut > 14000 * 256) cut = 14000 * 256;          /* Hz Q8 */
        if (pk > 16000 * 256) pk = 16000 * 256;
        v->fcut = cut;
        v->fpk = pk;
        if (p->swell) {
            v->fval = cut >> 1;
            v->fstep = (pk - (cut >> 1)) * 32 / p->ramp;       /* (< 2^27: 32-bit) */
        } else {
            v->fval = pk;
            v->fstep = 0;
        }
        v->ic1 = v->ic2 = 0;
        kf_svf_coef(v->fval, p->k, &v->a1, &v->a2, &v->a3);
        v->fcoef = v->fval;
        for (o = 0; o < p->nosc; o++) {
            v->ph[o] = 0;
            v->base[o] = v->inc[o] = (uint32_t)(((uint64_t)inc * p->ratio[o]) >> 16);
            v->rinc[o] = kf_rinc(v->inc[o]);
        }
    }
}
static void kf_off(const kf_ev_t *e)
{
    kf_voice_t *best = 0;
    uint32_t i;
    for (i = 0; i < KF_NV; i++) {
        kf_voice_t *v = &kf_v[i];
        if (v->state == 1u && v->part == e->part && v->pitch == e->pitch && (!best || v->age < best->age)) best = v;
    }
    if (best) {
        best->state = 2;
        best->rel_at = best->t < 7 ? 7 : best->t;    /* not before 5 ms (the reference: t + .005) */
    }
}
static void kf_hit(const kf_ev_t *e)
{
    const kf_kit_t *K;
    uint32_t kind = KF_GM[e->pitch & 127u], j, vel = e->vel > 127 ? 127u : e->vel;
    if (kind == 255u || e->patch >= KF_NKIT || !vel) return;
    K = &KF_KIT[e->patch];
    kf_kit = e->patch;
    if (kind == 0u && kf_pump_depth) kf_duck_t = 0;      /* a kick: the band ducks (GRIT) */
    for (j = K->first[kind]; j < (uint32_t)K->first[kind] + K->n[kind]; j++) {
        const kf_layer_t *d = &KF_LAYER[j];
        kf_layer_rt_t *L = 0;
        uint32_t i;
        int32_t l2;
        for (i = 0; i < KF_NL && !L; i++)
            if (!kf_l[i].on) L = &kf_l[i];
        for (i = 0; i < KF_NL && !L; i++)        /* full: the quietest */
            if (!L || kf_l[i].g < L->g) L = &kf_l[i];
        if (!L) L = &kf_l[0];
        L->on = 1;
        L->def = (uint8_t)j;
        L->t = 0;
        L->at = d->at;
        L->end = d->at + d->end;
        L->ph = 0;
        L->inc = d->inc;
        L->ic1 = L->ic2 = 0;
        L->g = (int32_t)((uint32_t)d->gain * vel * 512u / 127u);   /* Q24 */
        L->gd = 0;
        l2 = -653131 - d->lgain - KF_LOG2VEL[vel];               /* log2(0.001 / (gain v)), Q16 */
        L->m = kf_exp2_small_q24((int32_t)(((int64_t)l2 * d->tdec) >> 16));
        if (d->gate) {
            int32_t open = d->at + 352;                          /* 255 ms, then a 45 ms fade (the gate's impulse) */
            if (open > kf_gate_open) kf_gate_open = open;
        }
    }
}
static void kf_event(const kf_ev_t *e)
{
    uint32_t i;
    if (e->type == KF_EV_ON) kf_on(e);
    else if (e->type == KF_EV_OFF) kf_off(e);
    else if (e->type == KF_EV_DRUM) kf_hit(e);
    else if (e->type == KF_EV_ALLOFF) {
        for (i = 0; i < KF_NV; i++)
            if (kf_v[i].state == 1u) { kf_v[i].state = 2; kf_v[i].rel_at = kf_v[i].t < 7 ? 7 : kf_v[i].t; }
    } else if (e->type == KF_EV_KILL) {
        for (i = 0; i < KF_NV; i++) kf_v[i].state = 0;
        for (i = 0; i < KF_NL; i++) kf_l[i].on = 0;
    }
}

/* ---- a voice, a tick: the control, then 32 frames into its patch's buffer */
#define KF_VIB_STEP 17452522u                    /* 5.6 Hz a tick, in 2^32 */
static void kf_voice_tick(kf_voice_t *v)
{
    const kf_patch_t *p = &KF_PATCH[v->patch];
    int32_t *pb = kf_pb[v->patch], env = v->env, envd, next, s;
    uint32_t i, o;

    /* the pitch: vibrato (5.6 Hz, its depth up over 0.45 s), the wrong note's detune (gone after 0.35 s) */
    if (p->vib || (v->det && v->t <= 483)) {
        int32_t c = 0, f;
        if (p->vib) {
            int32_t depth = v->t >= 620 ? p->vib : p->vib * v->t / 620;
            c = (depth * kf_sine[((uint32_t)v->t * KF_VIB_STEP) >> 20]) >> 15;
        }
        if (v->det && v->t < 483) c += v->det * (483 - v->t) / 483;
        f = 65536 + ((c * 9693) >> 16);          /* 2^(c / 1200), c in cents Q8 */
        if (p->fm) v->cinc = (uint32_t)(((uint64_t)v->cbase * (uint32_t)f) >> 16);
        else for (o = 0; o < p->nosc; o++) {
            v->inc[o] = (uint32_t)(((uint64_t)v->base[o] * (uint32_t)f) >> 16);
            v->rinc[o] = kf_rinc(v->inc[o]);
        }
    }

    /* the level at the end of this tick */
    if (v->state == 1u || v->t < v->rel_at) {
        if (v->t * 32 < p->att) { next = env + v->att_step; if (next > v->A) next = v->A; }
        else next = env + (int32_t)(((int64_t)((int32_t)(((int64_t)v->A * p->sus) >> 15) - env) * p->c_dec) >> 24);
    } else {
        next = env - (int32_t)(((int64_t)env * (v->fast ? 1210000 : p->c_rel)) >> 24);   /* fast: 1 - exp(-T / 10 ms) */
        v->rt++;
    }
    envd = (next - env) >> 5;

    if (p->fm) {
        uint32_t cph = v->cph, cinc = v->cinc, m0 = v->mph[0], m1 = v->mph[1], i0 = v->minc[0], i1 = v->minc[1];
        int32_t x0, x1;
        int8_t dr = p->drive;
        /* the modulators' deviation, as phase steps: index x the modulator's frequency (WebAudio adds it to the
         * carrier's frequency: true FM, the phase integrates it; with ratios like 1 and 2 that is not the same as
         * phase modulation) */
        x0 = (int32_t)(((int64_t)i0 * v->mi[0]) >> 12);
        x1 = p->nmod > 1u ? (int32_t)(((int64_t)i1 * v->mi[1]) >> 12) : 0;
        for (o = 0; o < 2u; o++)                 /* the index falls to its sustain */
            v->mi[o] += (int32_t)(((int64_t)(v->mtgt[o] - v->mi[o]) * p->c_mdec[o]) >> 24);
#define KF_FM_STEP()                                                                     \
            uint32_t ci = cph >> 20;                                                     \
            int32_t fr = (int32_t)((cph >> 4) & 0xFFFFu), a = kf_sine[ci], b = kf_sine[(ci + 1u) & 4095u]; \
            s = a + (int32_t)(((int64_t)(b - a) * fr) >> 16);   /* the carrier, interpolated */ \
            cph += cinc + (uint32_t)(((int64_t)kf_sine[m0 >> 20] * x0 + (int64_t)kf_sine[m1 >> 20] * x1) >> 15); \
            m0 += i0; m1 += i1
        if (dr < 0)                              /* (with the drive or without: the patch decides, not the frame) */
            for (i = 0; i < KF_TICK; i++) {
                KF_FM_STEP();
                pb[i] += (int32_t)(((int64_t)s * env) >> 24);
                env += envd;
            }
        else {
            const int16_t *T = kf_drive[dr];
            for (i = 0; i < KF_TICK; i++) {
                KF_FM_STEP();
                {
                    int32_t x = s + 32768, k = x >> 8, fq = x & 255;
                    if (k > 255) { k = 255; fq = 256; }
                    s = T[k] + (((T[k + 1] - T[k]) * fq) >> 8);
                }
                pb[i] += (int32_t)(((int64_t)s * env) >> 24);
                env += envd;
            }
        }
#undef KF_FM_STEP
        v->cph = cph; v->mph[0] = m0; v->mph[1] = m1;
    } else {
        int32_t buf[KF_TICK];
        int32_t ic1 = v->ic1, ic2 = v->ic2, a1, a2, a3;
        int8_t dr = p->drive;
        int32_t cr = p->crush;

        /* the filter: its envelope, then this tick's coefficients */
        if (p->swell && v->t * 32 < p->ramp) {
            v->fval += v->fstep;
            if (v->fval > v->fpk) v->fval = v->fpk;
        } else {
            int32_t d = v->fval - v->fcut;
            if (d > 256 || d < -256) {
                v->fval = v->fcut + (int32_t)(((int64_t)d * p->m_envt) >> 24);
            } else v->fval = v->fcut;
        }
        if (v->fval != v->fcoef) { kf_svf_coef(v->fval, p->k, &v->a1, &v->a2, &v->a3); v->fcoef = v->fval; }
        a1 = v->a1; a2 = v->a2; a3 = v->a3;

        for (o = 0; o < p->nosc; ) {             /* the oscillators over the tick: equal saws together, the rest alone */
            uint32_t ph = v->ph[o], inc = v->inc[o], ri = v->rinc[o], inc2 = inc << 1;
            int32_t lv = p->lvl[o], first = o == 0;
            /* the PolyBLEP's window is a step either side of the wrap: q + inc < 2 inc catches both in one test,
             * and it is rare (two frames a period), so the common frame is a ramp and a multiply-add */
            if (p->wav[o] == 0u && o + 1u < p->nosc && p->wav[o + 1u] == 0u && p->lvl[o + 1u] == lv) {
                /* two saws of one level (the detuned pairs): summed, then one multiply */
                uint32_t ph1 = v->ph[o + 1u], inc1 = v->inc[o + 1u], ri1 = v->rinc[o + 1u], inc12 = inc1 << 1;
                if (kf_clear(ph + 0x80000000u, inc) && kf_clear(ph1 + 0x80000000u, inc1)) {   /* no step this tick */
                    if (first)
                        for (i = 0; i < KF_TICK; i++) {
                            buf[i] = ((int32_t)((ph + 0x80000000u) >> 16) + (int32_t)((ph1 + 0x80000000u) >> 16) - 65536) * lv;
                            ph += inc; ph1 += inc1;
                        }
                    else
                        for (i = 0; i < KF_TICK; i++) {
                            buf[i] += ((int32_t)((ph + 0x80000000u) >> 16) + (int32_t)((ph1 + 0x80000000u) >> 16) - 65536) * lv;
                            ph += inc; ph1 += inc1;
                        }
                } else for (i = 0; i < KF_TICK; i++) {
                    uint32_t q = ph + 0x80000000u, q1 = ph1 + 0x80000000u;
                    int32_t y = (int32_t)(q >> 16) + (int32_t)(q1 >> 16) - 65536;
                    if (__builtin_expect(q + inc < inc2, 0)) y += kf_blep(q, ri);
                    if (__builtin_expect(q1 + inc1 < inc12, 0)) y += kf_blep(q1, ri1);
                    if (first) buf[i] = y * lv; else buf[i] += y * lv;
                    ph += inc; ph1 += inc1;
                }
                v->ph[o] = ph; v->ph[o + 1u] = ph1;
                o += 2u;
                continue;
            }
            if (p->wav[o] == 0u && kf_clear(ph + 0x80000000u, inc)) {   /* a saw with no step this tick */
                if (first)
                    for (i = 0; i < KF_TICK; i++) { buf[i] = ((int32_t)((ph + 0x80000000u) >> 16) - 32768) * lv; ph += inc; }
                else
                    for (i = 0; i < KF_TICK; i++) { buf[i] += ((int32_t)((ph + 0x80000000u) >> 16) - 32768) * lv; ph += inc; }
            } else if (p->wav[o] == 1u && kf_clear(ph, inc) && kf_clear(ph + 0x80000000u, inc)) {   /* a square, no step: */
                int32_t y = ((int32_t)((ph + 0x80000000u) >> 31) * 65535 - 32768) * lv;          /* one level the tick */
                if (first)
                    for (i = 0; i < KF_TICK; i++) buf[i] = y;
                else
                    for (i = 0; i < KF_TICK; i++) buf[i] += y;
                ph += (uint32_t)KF_TICK * inc;
            } else if (p->wav[o] == 0u) {         /* saw: 0 at phase 0, rising (WebAudio's); PolyBLEP at the step */
                for (i = 0; i < KF_TICK; i++) {
                    uint32_t q = ph + 0x80000000u;
                    int32_t y = (int32_t)(q >> 16) - 32768;
                    if (__builtin_expect(q + inc < inc2, 0)) y += kf_blep(q, ri);
                    if (first) buf[i] = y * lv; else buf[i] += y * lv;
                    ph += inc;
                }
            } else if (p->wav[o] == 1u) {         /* square: +1 then -1 (WebAudio's); PolyBLEP at both steps */
                for (i = 0; i < KF_TICK; i++) {
                    uint32_t q = ph + 0x80000000u;
                    int32_t y = (int32_t)(q >> 31) * 65535 - 32768;   /* ph < 1/2: +1 */
                    if (__builtin_expect(ph + inc < inc2, 0)) y -= kf_blep(ph, ri);
                    if (__builtin_expect(q + inc < inc2, 0)) y += kf_blep(q, ri);
                    if (first) buf[i] = y * lv; else buf[i] += y * lv;
                    ph += inc;
                }
            } else {                              /* triangle: 0 at phase 0, rising */
                for (i = 0; i < KF_TICK; i++) {
                    int32_t d = (int32_t)(ph + 0x40000000u - 0x80000000u);
                    uint32_t ad = (uint32_t)(d ^ (d >> 31));
                    int32_t y = (32768 - (int32_t)(ad >> 15)) * lv;
                    if (first) buf[i] = y; else buf[i] += y;
                    ph += inc;
                }
            }
            v->ph[o] = ph;
            o++;
        }
        /* the filter, the drive, the crushed copy, the level: one loop for each (the patch decides, not the frame) */
#define KF_SVF_STEP()                                                                    \
            int32_t v0 = buf[i] >> 12, v3 = v0 - ic2, v1, v2;                            \
            v1 = (int32_t)(((int64_t)a1 * ic1 + (int64_t)a2 * v3) >> 15);               \
            v2 = ic2 + (int32_t)(((int64_t)a2 * ic1 + (int64_t)a3 * v3) >> 15);         \
            ic1 = 2 * v1 - ic1;                                                          \
            ic2 = 2 * v2 - ic2
#define KF_DRIVE_STEP(s, T) do {                                                         \
                int32_t x_ = (s) < -32767 ? -32767 : (s) > 32767 ? 32767 : (s), k_, fr_;  \
                x_ += 32768; k_ = x_ >> 8; fr_ = x_ & 255;                               \
                (s) = (T)[k_] + ((((T)[k_ + 1] - (T)[k_]) * fr_) >> 8);                  \
            } while (0)
        if (dr < 0)
            for (i = 0; i < KF_TICK; i++) {
                KF_SVF_STEP();
                pb[i] += (int32_t)(((int64_t)v2 * env) >> 24);
                env += envd;
            }
        else {
            const int16_t *T = kf_drive[dr];
            if (!cr)
                for (i = 0; i < KF_TICK; i++) {
                    KF_SVF_STEP();
                    s = v2;
                    KF_DRIVE_STEP(s, T);
                    pb[i] += (int32_t)(((int64_t)s * env) >> 24);
                    env += envd;
                }
            else
                for (i = 0; i < KF_TICK; i++) {
                    KF_SVF_STEP();
                    s = v2;
                    KF_DRIVE_STEP(s, T);
                    s += (((((s * 10) + 16384) >> 15) * 3277) * cr) >> 15;   /* round(x 10) / 10, mixed in */
                    pb[i] += (int32_t)(((int64_t)s * env) >> 24);
                    env += envd;
                }
        }
#undef KF_SVF_STEP
#undef KF_DRIVE_STEP
        v->ic1 = ic1;
        v->ic2 = ic2;
    }
    v->env = next;
    v->t++;
    kf_pb_used |= 1u << v->patch;
    if (v->state == 2u && v->t > v->rel_at && (v->rt > p->rel_end || next < v->A / 100 || next < 64)) v->state = 0;   /* -40 dB: under the mix */
}

/* ---- a drum layer, a tick */
static void kf_layer_tick(kf_layer_rt_t *L, int32_t *dry, int32_t *gate, int32_t *rev)
{
    const kf_layer_t *d = &KF_LAYER[L->def];
    int32_t g = L->g, gd, next, i;
    if (L->t < L->at) { L->t++; return; }
    if (L->t >= L->end) { L->on = 0; return; }
    next = (int32_t)(((int64_t)g * L->m) >> 24);
    gd = (next - g) >> 5;
    if (d->noise) {
        int32_t ic1 = L->ic1, ic2 = L->ic2, a1 = d->a1, a2 = d->a2, a3 = d->a3, k = d->k;
        for (i = 0; i < (int32_t)KF_TICK; i++) {
            int32_t v0 = kf_noise(), v3 = v0 - ic2, v1, v2, y;
            v1 = (int32_t)(((int64_t)a1 * ic1 + (int64_t)a2 * v3) >> 15);
            v2 = ic2 + (int32_t)(((int64_t)a2 * ic1 + (int64_t)a3 * v3) >> 15);
            ic1 = 2 * v1 - ic1;
            ic2 = 2 * v2 - ic2;
            y = d->flt == 1u ? v0 - ((k * v1) >> 15) - v2 : (k * v1) >> 15;   /* high-pass, or band-pass at 0 dB */
            y = (int32_t)(((int64_t)y * g) >> 24);
            dry[i] += y;
            if (d->gate) gate[i] += (y * d->gate) >> 15;
            if (d->rev) rev[i] += (y * d->rev) >> 15;
            g += gd;
        }
        L->ic1 = ic1;
        L->ic2 = ic2;
    } else {
        uint32_t ph = L->ph, inc = L->inc;
        for (i = 0; i < (int32_t)KF_TICK; i++) {
            int32_t s, y;
            if (d->wav == 3u) s = kf_sine[ph >> 20];
            else { int32_t q = (int32_t)(ph + 0x40000000u - 0x80000000u); s = 32768 - (int32_t)((uint32_t)(q ^ (q >> 31)) >> 15); }
            y = (int32_t)(((int64_t)s * g) >> 24);
            dry[i] += y;
            if (d->gate) gate[i] += (y * d->gate) >> 15;
            if (d->rev) rev[i] += (y * d->rev) >> 15;
            g += gd;
            ph += inc;
        }
        L->ph = ph;
        if (L->t - L->at < d->sweep) L->inc = (uint32_t)(((uint64_t)inc * (uint32_t)d->m_sweep) >> 24);
    }
    L->g = next;
    L->t++;
}

/* ---- the effects, a frame at a time */
#define KF_RV_OUT 9000                           /* Q15: the hall's level: its impulse's energy as the reference's (tests) */
#define KF_GT_OUT 9000                           /* Q15: the gate's */
static inline int32_t kf_sat16(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }
static inline int32_t kf_ap_run(uint32_t n, int32_t x)   /* a Schroeder all-pass (g 0.6): the hall's diffuser n */
{
    int16_t *b = kf_ap[n];
    uint32_t i = kf_ap_i[n];
    int32_t y = b[i], w = x + ((y * 19661) >> 15);
    b[i] = (int16_t)kf_sat16(w);
    if (++i >= KF_AP_LEN[n]) i = 0;
    kf_ap_i[n] = i;
    return y - ((w * 19661) >> 15);
}
static inline int32_t kf_lut(const int16_t *T, int32_t x)   /* a 257-point curve over -1..1, input clamped */
{
    int32_t k, fr;
    x = x < -32767 ? -32767 : x > 32767 ? 32767 : x;
    x += 32768; k = x >> 8; fr = x & 255;
    return T[k] + (((T[k + 1] - T[k]) * fr) >> 8);
}

/* the compressor (the reference's DynamicsCompressor: -10 dB, knee 6 dB, ratio 3, 4 ms / 200 ms, its make-up):
 * the gain for a peak level, Q12. In dB: below -10 none; a quadratic knee to -4; then 1/3 of the rest; plus the
 * make-up (the gain at 0 dBFS, to the 0.6th power, as Chromium's) */
#define KF_COMP_MAKEUP_DB_Q16 192020             /* +2.93 dB (measured: tools/ref_tour.cjs --comp) */
static int32_t kf_comp_gain(int32_t peak)        /* peak Q15 -> gain Q12 */
{
    int32_t x, g;
    if (peak < 1) peak = 1;
    x = (int32_t)(((int64_t)((int32_t)kf_log2_q16((uint32_t)peak) - 15 * 65536) * 394566) >> 16);   /* dB Q16 (6.0206 dB a bit) */
    if (x <= -10 * 65536) g = 0;
    else if (x < -4 * 65536) { int32_t d = x + 10 * 65536; g = -(int32_t)((((int64_t)d * d) >> 16) * 2 / 3 / 12); }   /* (1/3 - 1) d^2 / (2 K) */
    else g = -(int32_t)((int64_t)(x + 10 * 65536 - 3 * 65536) * 2 / 3);                                            /* (1/3 - 1)(x - T - K/2) */
    g += KF_COMP_MAKEUP_DB_Q16;
    return (int32_t)(kf_exp2_q16((int32_t)(((int64_t)g * 10885) >> 16)) >> 4);   /* 2^(dB / 6.0206): 10885 = 2^16 / 6.0206 */
}

/* ---- the interrupt: n frames (a multiple of 32), L R L R, added to out (Q15) */
static void kf_render(int32_t *out, uint32_t n)
{
    uint32_t f0, i, k;
    while (kf_qr[0] != kf_qw[0]) { kf_event(&kf_q[0][kf_qr[0] % KF_QN]); kf_qr[0]++; }   /* at once */
    for (f0 = 0; f0 < n; f0 += KF_TICK) {
        int32_t P[KF_TICK], B[KF_TICK], D[KF_TICK], RV[KF_TICK], GT[KF_TICK], DL[KF_TICK], C[KF_TICK];
        uint32_t now = kf_now, voices = 0, used;
        int32_t *o = out + 2u * f0, duck0 = kf_duck, cg0 = kf_cgain, peak = 0;
        const int16_t *glue = kf_glue[kf_glue_sel][0], *dsat = kf_glue[kf_glue_sel][1];
        int32_t gp = kf_gp, gb = kf_gb, mk = kf_makeup, dlen = kf_dly_len;

        while (kf_qr[1] != kf_qw[1]) {           /* the timed events due in this tick */
            const kf_ev_t *e = &kf_q[1][kf_qr[1] % KF_QN];
            if ((int32_t)(e->t - (now + KF_TICK)) >= 0) break;
            kf_event(e);
            kf_qr[1]++;
        }
        for (i = 0; i < KF_TICK; i++) P[i] = B[i] = D[i] = RV[i] = GT[i] = DL[i] = C[i] = 0;

        /* the oscillator budget: the quietest release tails fade out fast while it is exceeded */
        {
            uint32_t osc = 0;
            for (k = 0; k < KF_NV; k++)
                if (kf_v[k].state && !kf_v[k].fast) osc += KF_PATCH[kf_v[k].patch].fm ? 1u + KF_PATCH[kf_v[k].patch].nmod : KF_PATCH[kf_v[k].patch].nosc;
            while (osc > KF_OSC_BUDGET) {
                kf_voice_t *q = 0;
                for (k = 0; k < KF_NV; k++)
                    if (kf_v[k].state == 2u && !kf_v[k].fast && kf_v[k].t >= kf_v[k].rel_at && (!q || kf_v[k].env < q->env)) q = &kf_v[k];
                if (!q) break;
                q->fast = 1;
                osc -= KF_PATCH[q->patch].fm ? 1u + KF_PATCH[q->patch].nmod : KF_PATCH[q->patch].nosc;
            }
        }
        /* the voices, into their patches' buffers; then each patch to its bus and sends */
        for (k = 0; k < KF_NV; k++)
            if (kf_v[k].state) { kf_voice_tick(&kf_v[k]); voices++; }
        used = kf_pb_used;
        kf_pb_used = 0;
        for (k = 0; used; k++, used >>= 1) {
            const kf_patch_t *p;
            int32_t *pb, *bus, rv, dl;
            if (!(used & 1u)) continue;
            p = &KF_PATCH[k];
            pb = kf_pb[k];
            bus = p->bus == 0u ? P : p->bus == 1u ? B : C;
            rv = p->rev;
            dl = p->dly;
            for (i = 0; i < KF_TICK; i++) {
                int32_t s = pb[i];
                bus[i] += s;
                RV[i] += (s * rv) >> 15;
                DL[i] += (s * dl) >> 15;
                pb[i] = 0;
            }
            if (rv) kf_rev_live = 4200;          /* 3 s */
            if (dl) kf_dly_live = 7400;          /* 5.4 s: 0.38^n down to -60 dB at the longest delay */
            if (p->bus == 2u) kf_cho_live = 70;
        }
        /* the drums */
        for (k = 0; k < KF_NL; k++)
            if (kf_l[k].on) { kf_layer_tick(&kf_l[k], D, GT, RV); voices++; kf_rev_live = 4200; }

        /* the pump: to 1 - depth in 4 ms from the kick, back to 1 with 100 ms from 50 ms after it */
        if (kf_duck_t < (1 << 20)) {
            int32_t tgt, c;
            if (kf_duck_t < 69) { tgt = 32768 - kf_pump_depth; c = 2915544; }   /* 1 - exp(-T / 4 ms), Q24 */
            else { tgt = 32768; c = 121299; }                                    /* 1 - exp(-T / 100 ms) */
            kf_duck += (int32_t)(((int64_t)(tgt - kf_duck) * c) >> 24);
            if (kf_duck_t >= 69 && 32768 - kf_duck < 4) { kf_duck = 32768; kf_duck_t = 1 << 20; }
            else kf_duck_t++;
        }
        kf_duck_d = (kf_duck - duck0) >> 5;

        /* the gate: open while a snare or clap rings (255 ms), then a 45 ms fade */
        if (kf_gate_open > 0) { kf_gate_open--; kf_gate_g = 32768; kf_gate_live = 120; }
        else if (kf_gate_g > 0) { kf_gate_g -= 529; if (kf_gate_g < 0) kf_gate_g = 0; }   /* 62 ticks */

        if (!voices && !kf_rev_live && !kf_dly_live && !kf_gate_live && !kf_cho_live) {
            kf_now = now + KF_TICK;              /* silence: nothing to add */
            continue;
        }

        for (i = 0; i < KF_TICK; i++) {
            int32_t back, m, L, R, revL = 0, revR = 0, gL = 0, gR = 0, dout = 0, d, a;
            /* the chorus: into the band, dry and through 2 modulated taps */
            if (kf_cho_live) {
                int32_t c = C[i], t1, t2;
                kf_cho[kf_cho_w & (KF_CHO_N - 1u)] = (int16_t)kf_sat16(c);
                {
                    int32_t d1 = 124186 + ((24832 * kf_sine[kf_cho_ph[0] >> 20]) >> 15);   /* 11 ms +- 2.2 ms, Q8 frames */
                    int32_t d2 = 191923 + ((34995 * kf_sine[kf_cho_ph[1] >> 20]) >> 15);   /* 17 ms +- 3.1 ms */
                    uint32_t p1 = kf_cho_w - (uint32_t)(d1 >> 8), p2 = kf_cho_w - (uint32_t)(d2 >> 8);
                    int32_t a1 = kf_cho[p1 & (KF_CHO_N - 1u)], b1 = kf_cho[(p1 - 1u) & (KF_CHO_N - 1u)];
                    int32_t a2 = kf_cho[p2 & (KF_CHO_N - 1u)], b2 = kf_cho[(p2 - 1u) & (KF_CHO_N - 1u)];
                    t1 = a1 + (((b1 - a1) * (d1 & 255)) >> 8);
                    t2 = a2 + (((b2 - a2) * (d2 & 255)) >> 8);
                }
                kf_cho_w++;
                B[i] += c + (((t1 + t2) * 18022) >> 15);   /* .55 */
            }
            /* the band, pumped; the drums through the kit's dirt and the GRIT saturation */
            back = (int32_t)(((int64_t)B[i] * duck0) >> 15);
            duck0 += kf_duck_d;
            if (D[i]) {
                int32_t x = (D[i] * 23593) >> 15;        /* .72 */
                const kf_kit_t *K = &KF_KIT[kf_kit];
                if (K->drive >= 0) {
                    x = kf_lut(kf_drive[K->drive], x);
                    if (K->crush) x += ((((((x * 8) + 16384) >> 15) * 4096) * K->crush) >> 15);   /* round(x 8) / 8 */
                    x = (x * K->level) >> 15;
                }
                back += kf_lut(dsat, x);
            }
            m = (int32_t)(((int64_t)P[i] * gp + (int64_t)back * gb) >> 12);

            /* the hall */
            if (kf_rev_live) {
                int32_t x = kf_ap_run(0, kf_ap_run(1, kf_ap_run(2, kf_ap_run(3, RV[i])))), y0, y1, y2, y3, h0, h1, h2, h3;
                y0 = kf_rv[0][kf_rv_i[0]]; y1 = kf_rv[1][kf_rv_i[1]]; y2 = kf_rv[2][kf_rv_i[2]]; y3 = kf_rv[3][kf_rv_i[3]];
                h0 = (y0 + y1 + y2 + y3) >> 1; h1 = (y0 - y1 + y2 - y3) >> 1;
                h2 = (y0 + y1 - y2 - y3) >> 1; h3 = (y0 - y1 - y2 + y3) >> 1;
                kf_rv[0][kf_rv_i[0]] = (int16_t)kf_sat16(x + ((h0 * kf_rv_g[0]) >> 15));
                kf_rv[1][kf_rv_i[1]] = (int16_t)kf_sat16(x + ((h1 * kf_rv_g[1]) >> 15));
                kf_rv[2][kf_rv_i[2]] = (int16_t)kf_sat16(x + ((h2 * kf_rv_g[2]) >> 15));
                kf_rv[3][kf_rv_i[3]] = (int16_t)kf_sat16(x + ((h3 * kf_rv_g[3]) >> 15));
                for (k = 0; k < 4u; k++) if (++kf_rv_i[k] >= KF_RV_LEN[k]) kf_rv_i[k] = 0;
                revL = ((y0 + y2) * KF_RV_OUT) >> 15;
                revR = ((y1 + y3) * KF_RV_OUT) >> 15;
            }
            /* the gated reverb: an FDN behind the gate; closed, its loop is cut so it empties */
            if (kf_gate_live) {
                int32_t x = GT[i], y0, y1, y2, y3, h0, h1, h2, h3, open = kf_gate_g ? 1 : 0;
                y0 = kf_gt[0][kf_gt_i[0]]; y1 = kf_gt[1][kf_gt_i[1]]; y2 = kf_gt[2][kf_gt_i[2]]; y3 = kf_gt[3][kf_gt_i[3]];
                h0 = (y0 + y1 + y2 + y3) >> 1; h1 = (y0 - y1 + y2 - y3) >> 1;
                h2 = (y0 + y1 - y2 - y3) >> 1; h3 = (y0 - y1 - y2 + y3) >> 1;
                kf_gt[0][kf_gt_i[0]] = (int16_t)kf_sat16(x + (open ? (h0 * kf_gt_g[0]) >> 15 : 0));
                kf_gt[1][kf_gt_i[1]] = (int16_t)kf_sat16(x + (open ? (h1 * kf_gt_g[1]) >> 15 : 0));
                kf_gt[2][kf_gt_i[2]] = (int16_t)kf_sat16(x + (open ? (h2 * kf_gt_g[2]) >> 15 : 0));
                kf_gt[3][kf_gt_i[3]] = (int16_t)kf_sat16(x + (open ? (h3 * kf_gt_g[3]) >> 15 : 0));
                for (k = 0; k < 4u; k++) if (++kf_gt_i[k] >= KF_GT_LEN[k]) kf_gt_i[k] = 0;
                gL = (int32_t)((((int64_t)(y0 + y2) * KF_GT_OUT >> 15) * kf_gate_g) >> 15);
                gR = (int32_t)((((int64_t)(y1 + y3) * KF_GT_OUT >> 15) * kf_gate_g) >> 15);
            }
            /* the delay: a dotted eighth, its feedback (.38) through a low-pass */
            if (kf_dly_live) {
                int32_t y = kf_dly[(kf_dly_w - (uint32_t)dlen) & (KF_DLY_N - 1u)], v3, v1, v2, ic1 = kf_dly_ic1, ic2 = kf_dly_ic2;
                v3 = y - ic2;
                v1 = (int32_t)(((int64_t)kf_dly_a1 * ic1 + (int64_t)kf_dly_a2 * v3) >> 15);
                v2 = ic2 + (int32_t)(((int64_t)kf_dly_a2 * ic1 + (int64_t)kf_dly_a3 * v3) >> 15);
                kf_dly_ic1 = 2 * v1 - ic1;
                kf_dly_ic2 = 2 * v2 - ic2;
                kf_dly[kf_dly_w & (KF_DLY_N - 1u)] = (int16_t)kf_sat16(DL[i] + ((v2 * 12452) >> 15));   /* .38 */
                kf_dly_w++;
                dout = (v2 * 13763) >> 15;           /* .42 */
            }
            /* the mix: .7, the glue, the make-up, the compressor */
            L = m + ((revL * 13107) >> 15) + ((gL * 19661) >> 15) + dout;   /* the hall .4, the gate .6 */
            R = m + ((revR * 13107) >> 15) + ((gR * 19661) >> 15) + dout;
            L = kf_lut(glue, (L * 22938) >> 15);
            R = kf_lut(glue, (R * 22938) >> 15);
            L = (L * mk) >> 12;
            R = (R * mk) >> 12;
            d = L < 0 ? -L : L; a = R < 0 ? -R : R;
            if (a > d) d = a;
            if (d > peak) peak = d;
            L = (L * cg0) >> 12;
            R = (R * cg0) >> 12;
            cg0 = (kf_cgain_x8 + kf_cgain_d * (int32_t)(i + 1u)) >> 8;
            o[2u * i] += kf_sat16(L);
            o[2u * i + 1u] += kf_sat16(R);
        }
        /* the compressor's gain for the next tick: attack 4 ms, release 200 ms */
        {
            int32_t tgt = kf_comp_gain(peak), g8 = kf_cgain_x8 + kf_cgain_d * (int32_t)KF_TICK, c;
            c = (tgt << 8) < g8 ? 2915544 : 60759;           /* 1 - exp(-T / 4 ms), 1 - exp(-T / 200 ms), Q24 */
            kf_cgain_x8 = g8;
            kf_cgain_d = (int32_t)(((int64_t)((tgt << 8) - g8) * c) >> 24) >> 5;
            kf_cgain = kf_cgain_x8 >> 8;
        }
        if (kf_rev_live) kf_rev_live--;
        if (kf_dly_live) kf_dly_live--;
        if (kf_cho_live) kf_cho_live--;
        if (kf_gate_live && !kf_gate_g && !kf_gate_open) kf_gate_live--;
        kf_cho_ph[0] += 841457u;                 /* .27 Hz a tick */
        kf_cho_ph[1] += 1277767u;                /* .41 Hz */
        kf_now = now + KF_TICK;
    }
}

static void kf_init(void)
{
    static const uint32_t RT_MS = 2200u, GT_MS = 4000u;   /* the hall's and the gate's decay times (-60 dB) */
    uint32_t i;
    for (i = 0; i < 4096u; i++) kf_sine[i] = KF_SINE[i];   /* (the RAM copies) */
    for (i = 0; i < KF_NDRIVE * 257u; i++) kf_drive[i / 257u][i % 257u] = KF_DRIVE[i / 257u][i % 257u];
    for (i = 0; i < 4u; i++) {                   /* g = 10^(-3 L / (fs RT)) = 2^(-3 L log2(10) / (fs RT)), 32-bit only */
        uint32_t e = 3u * KF_RV_LEN[i] * 217706u / (KF_FS * RT_MS / 1000u);   /* Q16 (217706 = log2(10) 2^16) */
        kf_rv_g[i] = (int32_t)(kf_exp2_q16(-(int32_t)e) >> 1);
        e = 3u * KF_GT_LEN[i] * 217706u / (KF_FS * GT_MS / 1000u);
        kf_gt_g[i] = (int32_t)(kf_exp2_q16(-(int32_t)e) >> 1);
    }
    kf_svf_coef(2600 * 256, 29205, &kf_dly_a1, &kf_dly_a2, &kf_dly_a3);   /* Q 1 dB */
    kf_cgain = kf_comp_gain(1);
    kf_cgain_x8 = kf_cgain << 8;
    kf_set_grit(70);
    kf_set_mix(60);
    kf_set_tempo(11200);
}
#endif /* RD_SPC */
