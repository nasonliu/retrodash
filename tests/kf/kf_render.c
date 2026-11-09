/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* The engine (firmware/src/kf_engine.h) on the host, without the platform:
 *   kf_render tour OUT.wav OUT.json   every patch alone (4 notes) and every kit (one hit of each drum), as
 *                                     tools/ref_tour.cjs plays them in the reference: the levels to compare
 *   kf_render stress OUT.wav          the worst case: every part on its gritty patch at once, 8 s (the cost)
 *   kf_render song N SECS OUT.wav [OUT.json]   a built-in song as the reference's renderSong plays it, its
 *                                     levels every 4 bars
 * The tour is defined twice, here and in tools/ref_tour.cjs: keep them the same. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define KF_POOL
#include "kf_engine.h"
#include "kf_song.h"
#include "kf_bench.h"

#define SR 44100
static int16_t *pcm;
static uint32_t pcm_n, pcm_cap;

static void render_to(uint32_t frames)           /* run the engine up to this frame */
{
    static int32_t buf[2 * 256];
    while (kf_now + 256u <= frames) {
        uint32_t i;
        memset(buf, 0, sizeof buf);
        kf_render(buf, 256);
        if (pcm_n + 256u > pcm_cap) { pcm_cap = pcm_cap ? pcm_cap * 2u : SR * 8u; pcm = realloc(pcm, pcm_cap * 4u); }
        for (i = 0; i < 512u; i++) pcm[2u * pcm_n + i] = (int16_t)(buf[i] > 32767 ? 32767 : buf[i] < -32767 ? -32767 : buf[i]);
        pcm_n += 256u;
    }
}
static uint32_t fr(double s) { return (uint32_t)(s * SR + .5); }
static void wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t n = pcm_n * 4u, v;
    if (!f) { perror(path); exit(1); }
    fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 1 | 2 << 16; fwrite(&v, 4, 1, f); v = SR; fwrite(&v, 4, 1, f);
    v = SR * 4; fwrite(&v, 4, 1, f); v = 4 | 16 << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
    fwrite(pcm, 4, pcm_n, f);
    fclose(f);
    printf("wav    -> %s (%.1f s)\n", path, pcm_n / (double)SR);
}
static void stats(FILE *js, const char *name, double t0, double t1, int last)
{
    uint32_t a = fr(t0), b = fr(t1), i;
    double sum = 0, dsum = 0, peak = 0;
    int32_t pl = 0, pr = 0;
    if (b > pcm_n) b = pcm_n;
    for (i = a; i < b; i++) {
        int32_t l = pcm[2 * i], r = pcm[2 * i + 1];
        double x = fabs(l / 32768.0) > fabs(r / 32768.0) ? fabs(l / 32768.0) : fabs(r / 32768.0);
        sum += (double)l * l + (double)r * r;
        dsum += (double)(l - pl) * (l - pl) + (double)(r - pr) * (r - pr);
        pl = l; pr = r;
        if (x > peak) peak = x;
    }
    sum /= 2.0 * (b - a) * 32768.0 * 32768.0;
    dsum /= 2.0 * (b - a) * 32768.0 * 32768.0;
    fprintf(js, "  {\"name\": \"%s\", \"rms_db\": %.2f, \"peak\": %.4f, \"bright\": %.4f}%s\n", name,
            10 * log10(sum + 1e-12), peak, sqrt(dsum / (sum + 1e-12)), last ? "" : ",");
    printf("  %-12s rms %6.2f dB  peak %.3f  bright %.3f\n", name, 10 * log10(sum + 1e-12), peak, sqrt(dsum / (sum + 1e-12)));
}

/* the tour: a patch's notes are its part's base + 0 4 7 12, every 0.4 s, 0.35 s long, velocity 100;
 * PATCH_S apart. A kit: kick snare clap rim hat ohat crash tom1..tom5 (GM 36 38 39 37 42 46 49 41 45 47 48 50),
 * every 0.2 s, velocity 100; KIT_S apart. */
#define PATCH_S 3.0
#define KIT_S 4.0
static const int BASE[6] = {72, 36, 60, 72, 72, 60};   /* PLAY BASS PAD ARP LEAD KEYS */
static const int HITS[12] = {36, 38, 39, 37, 42, 46, 49, 41, 45, 47, 48, 50};
static int part_of(int patch) { int p; for (p = 5; p >= 0; p--) if (patch >= KF_PART_FIRST[p]) return p; return 0; }

static void tour(const char *wavp, const char *jsp)
{
    FILE *js = fopen(jsp, "w");
    int p, j, k;
    double s = 0.1;
    if (!js) { perror(jsp); exit(1); }
    fprintf(js, "[\n");
    for (p = 0; p < (int)KF_NPATCH; p++, s += PATCH_S) {
        int part = part_of(p);
        for (j = 0; j < 4; j++) {
            int pitch = BASE[part] + (j == 0 ? 0 : j == 1 ? 4 : j == 2 ? 7 : 12);
            double t = s + j * 0.4;
            render_to(fr(t)); kf_note_on(fr(t), part, p, pitch, 100, 0, 4096);
            render_to(fr(t + .35)); kf_note_off(fr(t + .35), part, pitch);
        }
    }
    for (k = 0; k < (int)KF_NKIT; k++, s += KIT_S)
        for (j = 0; j < 12; j++) { double t = s + j * 0.2; render_to(fr(t)); kf_drum(fr(t), k, HITS[j], 100); }
    render_to(fr(s + .5));
    s = 0.1;
    for (p = 0; p < (int)KF_NPATCH; p++, s += PATCH_S) stats(js, KF_PATCH[p].name, s, s + PATCH_S, 0);
    for (k = 0; k < (int)KF_NKIT; k++, s += KIT_S) stats(js, KF_KIT[k].name, s, s + KIT_S, k == (int)KF_NKIT - 1);
    fprintf(js, "]\n");
    fclose(js);
    printf("json   -> %s\n", jsp);
    wav(wavp);
}

/* the worst case (firmware/src/kf_bench.h), 8 s */
static void stress(const char *wavp)
{
    uint32_t f;
    kf_bench_start(fr(.1));
    for (f = 0; f < fr(8.1); f += 256u) { kf_bench_post(f + 2048u); render_to(f + 256u); }
    wav(wavp);
}

/* a song as the reference's renderSong plays it: the band from 0.05 s, and the melody (the song's chart, heard at
 * pitch + transpose) on the player's voice of channel 1's keyswitches, velocity 104, .95 of each note */
static void song(int idx, double secs, const char *wavp, const char *jsp)
{
    const kf_song_t *s = &KF_SONG[idx];
    const kf_cnote_t *m = s->chart;
    uint32_t f, mi = 0, n = s->nchart;
    kf_set_grit(70);
    kf_seq_start(s, fr(.05), 100);
    for (f = 0; f < fr(secs); f += 256u) {
        uint32_t upto = f + 2048u;
        while (mi < n && (int32_t)(kf_seq_frame(m[mi].t) - upto) < 0) {   /* the melody, in time with the band */
            uint32_t t = kf_seq_frame(m[mi].t);
            kf_seq_post(t + 1u);
            kf_sched_note(t, KF_P_PLAY, kf_seq_play_patch(), (uint32_t)(m[mi].pitch + s->transpose), 104, kf_seq_len(m[mi].len) * 19u / 20u);
            mi++;
        }
        kf_seq_post(upto);
        render_to(f + 256u);
    }
    if (jsp) {                                   /* the levels, every 4 bars */
        FILE *js = fopen(jsp, "w");
        double bar = 4 * 60.0 / (s->bpm_x100 / 100.0), t0;
        int k = 0;
        fprintf(js, "[\n");
        for (t0 = .05; t0 + 4 * bar <= secs + 1e-6; t0 += 4 * bar, k++) {
            char name[32];
            snprintf(name, sizeof name, "bars %d-%d", 4 * k + 1, 4 * k + 4);
            stats(js, name, t0, t0 + 4 * bar, t0 + 8 * bar > secs + 1e-6);
        }
        fprintf(js, "]\n");
        fclose(js);
    }
    wav(wavp);
}

int main(int argc, char **argv)
{
    kf_init();
    if (argc >= 4 && !strcmp(argv[1], "tour")) tour(argv[2], argv[3]);
    else if (argc >= 3 && !strcmp(argv[1], "stress")) stress(argv[2]);
    else if (argc >= 5 && !strcmp(argv[1], "song")) song(atoi(argv[2]), atof(argv[3]), argv[4], argc >= 6 ? argv[5] : 0);
    else { fprintf(stderr, "usage: kf_render tour OUT.wav OUT.json | stress OUT.wav | song N SECS OUT.wav [OUT.json]\n"); return 2; }
    return 0;
}
