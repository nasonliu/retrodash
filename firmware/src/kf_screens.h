/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* VaveHero's screens, drawn as the reference draws them (reference/vavehero.html, "the highway", "the screens"):
 * the splash, the songs, the play (the highway with its gems, holds, chords, OCT bands, beams and bursts, the key
 * strip, the header, the OP meter, the banners) and the results. Each draws the whole screen into the canvas
 * (kf_gfx.h); vavehero.c sends it. */
#pragma once
#include "kf_gfx.h"
#include "kf_game.h"
#include "kf_splash.h"

static const char *const KS_LEVELS[4] = {"EASY", "MED", "HARD", "EXP"};
static rgb_t kjcol(uint32_t j)
{
    static const rgb_t C[7] = {K_TEXT, K_CYAN, K_BLUE, K_VIOLET, K_CORAL, K_CORAL, K_VIOLET};
    return C[j < 7u ? j : 0u];
}

/* ---- the night the menus sit in (the splash's sky): its colour down the screen, and its stars, some twinkling */
#define KS_NSTAR 44
static uint8_t ks_star[KS_NSTAR][3];             /* x, y, brightness (1/255) */
static void ks_night(uint32_t ms)
{
    int32_t y, k;
    if (!ks_star[0][2]) {                        /* (placed once, the reference's way: the same numbers) */
        uint32_t r = 1985u;
        for (k = 0; k < KS_NSTAR; k++) {
            r = r * 1664525u + 1013904223u; ks_star[k][0] = (uint8_t)(3u + (r >> 8) % 234u);
            r = r * 1664525u + 1013904223u; ks_star[k][1] = (uint8_t)(22u + (r >> 8) % 196u);
            r = r * 1664525u + 1013904223u; ks_star[k][2] = (uint8_t)(70u + (r >> 8) % 180u);
        }
    }
    for (y = 0; y < 240; y++) kfill_rows(y, y + 1, kmix(0x07021au, 0x2a0c50u, y * 4096 / 239));
    for (k = 0; k < KS_NSTAR; k++) {
        int32_t b = ks_star[k][2], x = ks_star[k][0], yy = ks_star[k][1];
        if (k % 4 == 0) {                        /* a quarter twinkle, each in its own time */
            int32_t ph = (int32_t)((ms / 40u + (uint32_t)k * 13u) % 64u);
            b = b * (32 + (ph < 32 ? ph : 63 - ph)) / 64;
        }
        kpx_blend(x, yy, K_WHITE, b * 16);
        if (b > 200) {
            kpx_blend(x - 1, yy, 0xffd6f8u, b * 5); kpx_blend(x + 1, yy, 0xffd6f8u, b * 5);
            kpx_blend(x, yy - 1, 0xffd6f8u, b * 5); kpx_blend(x, yy + 1, 0xffd6f8u, b * 5);
        }
    }
}

/* ---- the strips at the top and the bottom: the splash's neon horizon under the header, the song's progress as its
 * sunset (magenta to gold) */
static void ks_header(const char *num, const char *title, const char *right, int32_t prog_q12, rgb_t rcol)
{
    int32_t x = 4;
    krect(0, 0, 240, 18, K_SURF);
    if (num) { char s[8]; kput(kput(s, num), " "); x = ktext(&FONT_S, x, 1, s, K_AMBER); }
    ktext(&FONT_S, x, 1, title, K_TEXT);
    if (right) ktextR(&FONT_S, 236, 1, right, rcol);
    if (prog_q12 >= 0) {
        int32_t w = (240 * prog_q12 + 2048) >> 12;
        krect(0, 18, 240, 2, 0x3a1a56u);
        for (x = 0; x < w; x++) krect(x, 18, 1, 2, kmix(K_NEON, K_GOLD, x * 4096 / 239));
    } else { krect(0, 18, 240, 1, K_NEON); krect(0, 19, 240, 1, kmix(K_BG, K_NEON, Q12(.3))); }
}
static void ks_panel(int32_t x, int32_t y, int32_t w, int32_t h)   /* a box on the night, a neon line along its top */
{
    krect(x, y, w, h, K_SURF);
    krect(x, y, w, 1, kmix(K_SURF, K_NEON, Q12(.55)));
}
static void ks_selbar(int32_t x, int32_t y, int32_t w, int32_t h)  /* the chosen row: neon fading to the right */
{
    int32_t i;
    for (i = 0; i < w; i++) krect(x + i, y, 1, h, kmix(kmix(K_SURF2, K_NEON, Q12(.34)), K_SURF2, i * 4096 / (w - 1)));
    krect(x, y, 3, h, K_CYAN);
}
static void ks_knobs(const char *const *labels, uint32_t muted)   /* muted: bit i */
{
    uint32_t i;
    krect(0, 222, 240, 18, K_SURF);
    for (i = 0; i < 4u; i++) {
        int32_t cx = 30 + 60 * (int32_t)i;
        rgb_t col = muted >> i & 1u ? K_MUTE : KC[i];
        krect(cx - 22, 222, 44, 2, col);
        if (i) krect(60 * (int32_t)i, 226, 1, 11, K_LINE);
        ktextC(&FONT_S, cx, 223, labels[i], col);
    }
}
static void ks_banner(const char *big, const char *small, rgb_t col, int32_t y)   /* the big words in the logo's chrome */
{
    int32_t h = small ? 56 : 42;
    rgb_t band = kmix(K_BG, K_BLACK, Q12(.35));
    krect(0, y, 240, h, band);
    krect(0, y, 240, 1, col); krect(0, y + 1, 240, 1, kmix(band, col, Q12(.35)));
    krect(0, y + h - 1, 240, 1, col); krect(0, y + h - 2, 240, 1, kmix(band, col, Q12(.35)));
    ktext_chrome(&FONT_L, KH_CX - (ktw(&FONT_L, big) >> 1) - 1, y + 4, big, 1);
    if (small) ktextC(&FONT_S, KH_CX, y + 36, small, K_TEXT);
}
static void ks_pill(int32_t x, int32_t y, const char *t, int32_t on, rgb_t oncol, int32_t locked)
{
    int32_t w = ktw(&FONT_S, t) + 10;
    if (on) kpcap(x, y, w, 18, oncol);
    else { kpcap(x, y, w, 18, K_LINE); kpcap(x + 1, y + 1, w - 2, 16, K_SURF); }
    ktext(&FONT_S, x + 5, y + 1, t, on ? K_INK : locked ? K_MUTE : K_DIM);
}

/* ---- the highway */
/* the floor and the lanes on it, row by row (kh_init's colours): the floor's grid beside the highway, the lanes, their lines */
static void ks_highway_bg(int32_t boost, int32_t spread)
{
    int32_t y, i;
    for (y = KH_TOPY; y < KH_HIT; y++) {
        const uint8_t *xs = kh_x[y - KH_TOPY], *gx = kh_gx[y - KH_TOPY];
        const uint16_t *lc = kh_lane[boost ? 1 : 0][y - KH_TOPY], *nc = kh_line[boost ? 1 : spread ? 2 : 0][y - KH_TOPY];
        uint16_t *row = cv_px + (uint32_t)y * 240u, fc = kh_floor565[y - KH_TOPY], gc = kh_gridc[y - KH_TOPY];
        int32_t x;
        for (x = 0; x < xs[0]; x++) row[x] = fc;
        for (x = xs[8]; x < 240; x++) row[x] = fc;
        for (i = 0; i < 2 * KH_NGX; i++) if (gx[i] != 255u) row[gx[i]] = gc;
        for (i = 0; i < 8; i++) { uint16_t c = lc[i]; for (x = xs[i]; x < xs[i + 1]; x++) row[x] = c; }
        for (i = 0; i <= 8; i++) row[xs[i] - (i == 8)] = i == 0 || i == 8 ? nc[i] : (boost ? nc[i] : nc[i]);
    }
}
static int32_t ks_z(int32_t t96, int32_t pos) { return (t96 * 256 - pos) / 6; }   /* the depth of a time, Q12 (a beat a unit) */
static void ks_beat_lines(int32_t pos, int32_t boost)
{
    int32_t b;
    for (b = (pos + 25) / KG_BEAT + 1; ; b++) {  /* ceil(beat + .001) */
        int32_t z = ks_z(b * 96, pos), y, s, xl, xr, bar;
        if (z > KH_ZTOP_Q12) break;
        if (z < 82) continue;                    /* .02 */
        y = kh_round(kh_Yq(z));
        s = kh_s_at(2 * y + 1);
        bar = b % 4 == 0;
        xl = kh_round(kh_X(KH_NL * 16, s)); xr = kh_round(kh_X(KH_NR * 16, s));
        krect(xl, y, xr - xl, 1, kh_fc(y, boost ? (bar ? 0xc79a3cu : 0x6b5330u) : (bar ? 0x8a2f9eu : 0x3b1a58u)));
        krect(0, y, xl, 1, kh_grid_h(y, bar));   /* (carried on across the floor: the splash's grid, moving with the beat) */
        krect(xr, y, 240 - xr, 1, kh_grid_h(y, bar));
    }
}
static void ks_tail(int32_t lane, int32_t z0, int32_t z1, rgb_t col, int32_t held)
{
    int32_t y, y1 = kh_round(kh_Yq(z1)), y0 = kh_round(kh_Yq(z0));
    rgb_t body = kmix(K_BG, col, held ? Q12(.9) : Q12(.55)), core = kmix(col, K_WHITE, Q12(.55));
    for (y = y1; y <= y0; y++) {
        int32_t s = kh_s_at(2 * y + 1), half = KH_LW * 15 * s / 100, xc = kh_X(kh_U4(lane * 256 + 128), s);   /* .15 LW s */
        if (half < 4096) half = 4096;
        krect(kh_round(xc - half), y, kh_round(xc + half) - kh_round(xc - half), 1, kh_fc(y, body));
        if (held) krect(kh_round(xc) - 1, y, 2, 1, core);
    }
}
/* a gem: its halo, its body, a light row at the top and a shade at the foot, all faded into fl by f (the splash's too) */
static void ks_gem_px(int32_t x0, int32_t y0, int32_t w, int32_t h, rgb_t col, rgb_t halo, int32_t ha, rgb_t fl, int32_t f)
{
    int32_t r = (h + 1) / 2, iw = w - 2 * r > 1 ? w - 2 * r : 1;
    kpcap(x0 - 1, y0 - 1, w + 2, h + 2, kmix(fl, kmix(K_BG, halo, ha), f));
    kpcap(x0, y0, w, h, kmix(fl, col, f));
    krect(x0 + r, y0 + 1, iw, 1, kmix(fl, kmix(col, K_WHITE, Q12(.6)), f));
    krect(x0 + r, y0 + h - 1, iw, 1, kmix(fl, kmix(col, K_BLACK, Q12(.3)), f));
}
static void ks_gem(int32_t lane, int32_t z, rgb_t col, int32_t boost, int32_t op)
{
    int32_t s = kh_sc(z) >> 4, yq = kh_Yq(z), yr = kh_round(yq), w, h;
    w = (84 * KH_LW * s / 100 + 2048) >> 12;
    h = (10 * s + 2048) >> 12;
    if (h < 3) h = 3;
    ks_gem_px(kh_round(kh_X(kh_U4(lane * 256 + 128), s) - w * 2048), kh_round(yq - h * 2048), w, h, col,
              boost ? K_GOLD : op ? 0xfff0c0u : col, op || boost ? Q12(.8) : Q12(.5), kh_floor_at(yr), kh_fade_at(yr));
}
static void ks_chord_bar(int32_t lane_lo, int32_t lane_hi, int32_t z)
{
    int32_t s = kh_sc(z) >> 4, y = kh_round(kh_Yq(z)), xa = kh_X(kh_U4(lane_lo * 256 + 128), s), xb = kh_X(kh_U4(lane_hi * 256 + 128), s);
    krect(kh_round(xa), y, kh_round(xb - xa), 1, kmix(kh_floor_at(y), K_WHITE, Q12(.4) * kh_fade_at(y) / 4096));
}
static void ks_oct_band(int32_t z, int32_t dir, int32_t auto_)
{
    static const int8_t DX[4] = {-48, -36, 36, 48};
    int32_t yc = kh_round(kh_Yq(z)), y, i, k;
    for (y = yc - 8; y < yc + 8; y++) {
        int32_t s = kh_s_at(2 * y + 1), xl = kh_round(kh_X(KH_NL * 16, s)), xr = kh_round(kh_X(KH_NR * 16, s));
        krect(xl, y, xr - xl, 1, (y == yc - 8 || y == yc + 7) ? K_VIOLET : kmix(K_BG, K_VIOLET, Q12(.26)));
    }
    ktextC(&FONT_S, KH_CX, yc - 8, auto_ ? "AUTO" : dir > 0 ? "OCT+" : "OCT-", 0xefe8ffu);
    for (k = 0; k < 4; k++)
        for (i = 0; i < 4; i++) {
            int32_t x = KH_CX + DX[k], yy = dir > 0 ? yc - 2 + i : yc + 1 - i;
            krect(x - i, yy, 1, 1, K_VIOLET);
            krect(x + i, yy, 1, 1, K_VIOLET);
        }
}
static void ks_beam(int32_t lane, rgb_t col)
{
    int32_t y;
    for (y = KH_HIT - 40; y < KH_HIT; y++) {
        int32_t s = kh_s_at(2 * y + 1), a = (y - (KH_HIT - 40)) * 4096 / 40, xc = kh_X(kh_U4(lane * 256 + 128), s);
        int32_t half = 46 * KH_LW * s / 100;     /* .46 LW s, Q12 */
        krect_a(kh_round(xc - half), y, kh_round(2 * half), 1, col, (int32_t)(((int64_t)a * a * Q12(.4)) >> 24));
    }
}
static void ks_burst(int32_t cx, int32_t cy, rgb_t col, int32_t age, int32_t big)   /* age Q12 */
{
    static const int8_t LB[4] = {5, 8, 11, 14}, LS[3] = {4, 7, 9};
    int32_t rays = big ? 9 : 5, nl = big ? 4 : 3, k, i, grow = 4096 + age * 6 / 10, al = 4096 - age * 7 / 10;
    for (k = 0; k < rays; k++) {
        uint32_t ph = (uint32_t)(2048 + (2 * k + 1) * 1024 / rays) & 4095u;   /* a = pi + (k + .5) / rays pi */
        int32_t ca = KF_SINE[(ph + 1024u) & 4095u], sa = KF_SINE[ph];
        for (i = 0; i < nl; i++) {
            int32_t t = big ? LB[i] : LS[i], px = cx + (int32_t)(((int64_t)ca * t * grow + (1 << 26)) >> 27), py = cy + (int32_t)(((int64_t)sa * t * grow + (1 << 26)) >> 27);
            krect_a(px, py, i ? 1 : 2, i ? 1 : 2, i % 2 ? col : K_WHITE, al);
        }
    }
    if (age < 2048) { krect_a(cx - 3, cy - 1, 7, 2, K_WHITE, al); krect_a(cx - 1, cy - 4, 2, 7, K_WHITE, al); }
}
/* the FM-1's keys: two rows of capsules (D1: the black row is shown and never used), or SPREAD's 15 (D2) */
static void ks_cap(int32_t cx, int32_t y, int32_t w, int32_t h, rgb_t col, int32_t state, int32_t black)   /* state: 0, 1 next, 2 lit */
{
    int32_t x = cx - w / 2;
    if (state == 2) { kpcap(x - 1, y - 1, w + 2, h + 2, kmix(K_BG, col, Q12(.55))); kpcap(x, y, w, h, col); krect(x + h / 2, y + 2, w - h, 1, kmix(col, K_WHITE, Q12(.65))); }
    else if (state == 1) { kpcap(x, y, w, h, col); kpcap(x + 1, y + 1, w - 2, h - 2, kmix(black ? 0x261336u : 0x39224eu, col, Q12(.22))); }
    else { kpcap(x, y, w, h, black ? 0x532878u : 0x52316fu); kpcap(x + 1, y + 1, w - 2, h - 2, black ? 0x251137u : 0x39224eu); }
}
static void ks_keys(const uint8_t *st, const uint8_t *led, int32_t spread)   /* st[lane]: 0 / 1 next / 2 lit; led[lane]: 0 1 2 */
{
    static const uint8_t BB[5] = {1, 2, 4, 5, 6};
    int32_t i, j;
    if (!spread) {
        for (i = 0; i < 5; i++) ks_cap(KH_NL + KH_LW * BB[i], 193, 18, 12, K_CORAL, 0, 1);
        for (i = 0; i < 8; i++) ks_cap(KH_NL + KH_LW * i + 14, 207, 25, 14, LANE[i], st[i], 0);   /* (U(i + .5), rounded as x is) */
        return;
    }
    for (j = 1; j <= 15; j++) {
        int32_t x = (29 * j - 3 + 1) / 2, y = 194, h = 26, w = 11, lv, s;   /* round(NL + j LW / 2 - 5.5) */
        rgb_t col, base;
        if (j % 2 == 0) { kpcap(x + 1, 198, w - 2, 18, 0x391b54u); kpcap(x + 2, 199, w - 4, 16, 0x1d0c2cu); continue; }
        i = (j - 1) / 2; col = LANE[i]; s = st[i]; lv = led[i];
        if (s == 2) { kpcap(x - 1, y - 1, w + 2, h + 2, kmix(K_BG, col, Q12(.55))); kpcap(x, y, w, h, col); }
        else if (s == 1) { kpcap(x, y, w, h, col); kpcap(x + 1, y + 1, w - 2, h - 2, kmix(0x39224eu, col, Q12(.22))); }
        else { kpcap(x, y, w, h, 0x52316fu); kpcap(x + 1, y + 1, w - 2, h - 2, 0x39224eu); }
        base = s == 2 ? kmix(col, K_BLACK, Q12(.35)) : 0x261436u;
        kpcap(x + 4, y + 5, 3, 12, lv == 2 ? K_WHITE : lv == 1 ? kmix(base, K_WHITE, Q12(.4)) : base);
    }
}

/* ---- the screens */
/* the splash, as an 80s video game cover: the art above the horizon (tools/gen_splash.py: the sky, the stars, the
 * striped sun, the mountains, the chrome logo, the project's mark), and under it, live, the game's own highway as the
 * road into the sunset, playing itself: the floor's neon grid beside it, the lanes, the beat lines, the gems, the hit
 * line with its bursts, the keys lighting. A loop of 16 notes at 125 BPM. The reference's drawSplash does the same,
 * number for number. */
#define KS_HZN 148                               /* the horizon: the art above, the floor below */
#define KS_HIT 214                               /* the hit line */
#define KS_HY 124                                /* the vanishing point, in the sun */
#define KS_NL 28                                 /* the lanes at the hit line: 28 to 212, 23 px */
#define KS_LW 23
#define KS_BEAT 480                              /* ms a beat */
#define KS_ZTOP 33792                            /* the horizon's depth, 3 / s(148) - 3 = 8.25, Q12 */
#define KS_ZBOT (-2662)                          /* the bottom row's */
static const uint8_t KS_NOTES[16][2] = {{0, 1}, {1, 3}, {2, 5}, {3, 4}, {4, 2}, {4, 6}, {6, 7}, {7, 5},
                                        {8, 3}, {9, 1}, {10, 0}, {10, 4}, {12, 6}, {13, 5}, {14, 3}, {15, 2}};   /* half beat, lane */
static int32_t ks_s(int32_t y) { return (2 * y + 1 - 2 * KS_HY) * 4096 / (2 * (KS_HIT - KS_HY)); }   /* a row's scale, Q12 */
static int32_t ks_xu(int32_t b16, int32_t s)     /* the x (Q12) of lane position b16 (1/16 lane) at scale s */
{
    return 120 * 4096 + ((((KS_NL * 16 + b16 * KS_LW) - 120 * 16) * s) >> 4);
}
static int32_t ks_scz(int32_t z) { return 50331648 / (12288 + z); }   /* 3 / (3 + z), Q12 */
static int32_t ks_Yq(int32_t z) { return KS_HY * 4096 + (KS_HIT - KS_HY) * ks_scz(z); }
static rgb_t ks_fl(int32_t y) { return kmix(K_FLOOR0, K_FLOOR1, (y - KS_HZN) * 4096 / 92); }
static int32_t ks_near(int32_t y) { int32_t n = (y - KS_HZN) * 4096 / (KS_HIT - KS_HZN); return n > 4096 ? 4096 : n; }
static int32_t ks_f(int32_t y) { int32_t f = (y - KS_HZN) * 4096 / 16; return f < 0 ? 0 : f > 4096 ? 4096 : f; }
static void ks_splash(uint32_t ms, const char *version)
{
    static const uint8_t BLACK_B[9] = {0, 1, 1, 0, 1, 1, 1, 0, 0};
    int32_t i, k, x, y, beat = (int32_t)(ms % (8u * KS_BEAT)) * 4096 / KS_BEAT;
    uint8_t st[8] = {0};
    for (i = 0; i < (int32_t)(KF_SPLASH_W * KF_SPLASH_H); i++) cv_px[i] = KF_SPLASH[i];   /* the art, as the canvas keeps it */
    for (y = KS_HZN; y < 240; y++) {             /* the floor, its grid, the lanes */
        int32_t s = ks_s(y), near = ks_near(y), f = ks_f(y);
        rgb_t fl = ks_fl(y);
        uint16_t *row = cv_px + (uint32_t)y * 240u, fc = swap16(c565(fl)), gc = swap16(c565(kmix(fl, K_GRID, Q12(.16) + near / 2)));
        for (x = 0; x < 240; x++) row[x] = fc;
        for (k = -9; k <= 17; k++) {
            if (y < KS_HIT && k >= 0 && k <= 8) continue;
            x = kh_round(ks_xu(k * 16, s));
            if (x >= 0 && x < 240) row[x] = gc;
        }
        if (y >= KS_HIT) continue;
        for (i = 0; i < 8; i++) {
            int32_t xl = kh_round(ks_xu(i * 16, s)), xr = kh_round(ks_xu(i * 16 + 16, s));
            rgb_t col = kmix(i % 2 ? 0x1a0a29u : 0x14071fu, LANE[i], Q12(.06) + near * 615 / 4096);
            krect(xl, y, xr - xl, 1, kmix(fl, col, f));
        }
        for (i = 0; i <= 8; i++)
            krect(kh_round(ks_xu(i * 16, s)) - (i == 8), y, 1, 1, kmix(fl, i == 0 || i == 8 ? 0xd043b8u : BLACK_B[i] ? 0x4a1f6eu : 0x2b1340u, f));
    }
    for (k = -1; k <= 9; k++) {                  /* the beat lines, carried on across the floor: its grid moves with the beat */
        int32_t z = (k * 4096 - beat) * 3 / 2, bar = (k + 8) % 4 == 0, near, s, xl, xr;
        rgb_t fl, gcol;
        if (z > KS_ZTOP || z < KS_ZBOT) continue;
        y = kh_round(ks_Yq(z));
        if (y <= KS_HZN + 1 || y >= 240) continue;
        near = ks_near(y); fl = ks_fl(y); s = ks_s(y);
        gcol = kmix(fl, K_GRID, (bar ? Q12(.30) : Q12(.16)) + near * (bar ? 55 : 40) / 100);
        if (y >= KS_HIT) { krect(0, y, 240, 1, gcol); continue; }
        xl = kh_round(ks_xu(0, s)); xr = kh_round(ks_xu(128, s));
        krect(xl, y, xr - xl, 1, kmix(fl, bar ? 0x8a2f9eu : 0x3b1a58u, ks_f(y)));
        krect(0, y, xl, 1, gcol);
        krect(xr, y, 240 - xr, 1, gcol);
    }
    krect(0, KS_HZN, 240, 1, K_NEON);            /* the horizon */
    krect(0, KS_HZN + 1, 240, 1, kmix(K_FLOOR0, K_NEON, Q12(.45)));
    for (k = 1; k >= 0; k--)                     /* the notes: this loop's and the next's, chords barred */
        for (i = 0; i < 16; i++) {
            int32_t tq = KS_NOTES[i][0] * 2048 + k * 8 * 4096 - beat, z = tq * 3 / 2, sc, yq, yr, w, h, lane = KS_NOTES[i][1];
            if (tq <= 0 || z > KS_ZTOP) continue;
            sc = ks_scz(z); yq = ks_Yq(z); yr = kh_round(yq);
            if (i + 1 < 16 && KS_NOTES[i + 1][0] == KS_NOTES[i][0]) {
                int32_t xa = ks_xu(lane * 16 + 8, sc), xb = ks_xu(KS_NOTES[i + 1][1] * 16 + 8, sc);
                krect(kh_round(xa), yr, kh_round(xb - xa), 1, kmix(ks_fl(yr), K_WHITE, Q12(.4) * ks_f(yr) / 4096));
            }
            w = (84 * KS_LW * sc / 100 + 2048) >> 12;
            h = (8 * sc + 2048) >> 12;
            if (h < 3) h = 3;
            ks_gem_px(kh_round(ks_xu(lane * 16 + 8, sc) - w * 2048), kh_round(yq - h * 2048), w, h, LANE[lane], LANE[lane],
                      Q12(.5), ks_fl(yr), ks_f(yr));
            if (tq <= 4096 && !st[lane]) st[lane] = 1;
        }
    krect(KS_NL, KS_HIT - 2, 8 * KS_LW, 1, kmix(K_BG, K_CYAN, Q12(.4)));   /* the hit line */
    krect(KS_NL, KS_HIT - 1, 8 * KS_LW, 2, K_CYAN);
    for (k = 0; k >= -1; k--)                    /* the notes hitting it: a burst, the key lit */
        for (i = 0; i < 16; i++) {
            int32_t tq = KS_NOTES[i][0] * 2048 + k * 8 * 4096 - beat, age = -tq * KS_BEAT / 4096, lane = KS_NOTES[i][1];
            if (tq > 0 || age >= 260) continue;
            ks_burst(KS_NL + KS_LW * lane + 12, KS_HIT - 1, LANE[lane], age * 4096 / 260, 1);
            if (age < 150) st[lane] = 2;
        }
    for (i = 0; i < 8; i++) ks_cap(39 + KS_LW * i, 218, 19, 11, LANE[i], st[i], 0);
    ktextOut(&FONT_S, 120 - (ktw(&FONT_S, "A RHYTHM GAME FOR FM-1") >> 1), 50, "A RHYTHM GAME FOR FM-1", K_CYAN, K_OUT, 1);
    if ((ms / 500u) % 2u == 0u) ktextOut(&FONT_S, 120 - (ktw(&FONT_S, "PRESS PLAY") >> 1), 186, "PRESS PLAY", K_AMBER, K_OUT, 1);
    ktextOut(&FONT_S, 4, 0, "FX DEMO", K_DIM, K_OUT, 1);
    ktextOut(&FONT_S, 236 - ktw(&FONT_S, version), 0, version, K_DIM, K_OUT, 1);
}

typedef struct { uint32_t sel, speed, spread, tone; int32_t best[8]; uint32_t mm_mode, ms; } ks_menu_t;   /* best: acc10, -1 none */
static void ks_songs(const ks_menu_t *m, const char *tone)
{
    static const char *const KN[4] = {"", "TONE", "SPEED", "MIX"}, *const KM[4] = {"MODE", "TONE", "BPM", "MIX"};
    uint32_t total = KF_NSONG + 1u, top, r;
    char s[40], *p;
    ks_night(m->ms);
    p = knum(s, m->sel + 1u, 2); p = kput(p, "/"); knum(p, total, 2);
    ks_header(0, "SONGS", m->spread ? "SPREAD" : s, -1, m->spread ? K_AMBER : K_DIM);
    top = m->sel < 2u ? 0u : m->sel - 2u;
    if (total > 6u && top > total - 6u) top = total - 6u;
    if (total <= 6u) top = 0;
    for (r = 0; r < 6u && top + r < total; r++) {
        uint32_t i = top + r, sel = i == m->sel;
        int32_t y = 22 + (int32_t)r * 20, best, medals, k;
        if (sel) ks_selbar(2, y - 1, 236, 19);
        if (i == KF_NSONG) {
            ktext(&FONT_S, 9, y, "MI", K_CYAN); ktext(&FONT_S, 33, y, "MIDI MODE", sel ? K_CYAN : K_TEXT); ktextR(&FONT_S, 236, y, "DAW", K_DIM);
            continue;
        }
        knum(s, i + 1u, 2);
        ktext(&FONT_S, 9, y, s, sel ? K_CYAN : K_MUTE);
        ktext(&FONT_S, 33, y, KF_SONG[i].title, K_TEXT);
        best = i < 8u ? m->best[i] : -1;
        if (best < 0) { ktextR(&FONT_S, 236, y, "NEW", K_CYAN); continue; }
        medals = best >= 900 ? 3 : best >= 800 ? 2 : best >= 700 ? 1 : 0;
        for (k = 0; k < 3; k++) {
            int32_t mx = 181 + k * 8, my = y + 6;
            if (k < medals) { krect(mx, my, 5, 5, K_AMBER); krect(mx + 1, my + 1, 2, 1, 0xffe2b0u); }
            else { krect(mx, my, 5, 5, 0x522e72u); krect(mx + 1, my + 1, 3, 3, K_BG); }
        }
        p = knum(s, (uint32_t)(best + 5) / 10u, 0); kput(p, "%");
        ktextR(&FONT_S, 236, y, s, sel ? K_TEXT : K_DIM);
    }
    if (m->sel == KF_NSONG) {                    /* MIDI MODE */
        int32_t x;
        ks_panel(2, 144, 236, 74);
        ktext(&FONT_S, 9, 148, "DAW > USB > FM-1", K_TEXT);
        ktext(&FONT_S, 9, 168, "CH 1 PLAY", K_CYAN); ktextR(&FONT_S, 231, 168, "2-6 10 BAND", K_VIOLET);
        x = 120 - (ktw(&FONT_S, "LIVE") + 10 + ktw(&FONT_S, "PREVIEW") + 10 + 6) / 2;
        ks_pill(x, 192, "LIVE", m->mm_mode == 0u, K_CYAN, 0);
        ks_pill(x + ktw(&FONT_S, "LIVE") + 16, 192, "PREVIEW", m->mm_mode == 1u, K_CYAN, 0);
        ks_knobs(KM, 0);
        return;
    }
    {
        const kf_song_t *so = &KF_SONG[m->sel];
        uint32_t bpm = so->bpm_x100 * m->speed / 100u, secs, i, x, tw = 0;
        int32_t lo = so->oct0, hi = so->oct0, o = so->oct0;
        secs = (uint32_t)so->end * 60u * 100u / (bpm ? bpm : 1u);
        ks_panel(2, 144, 236, 74);
        p = kput(s, "BPM "); knum(p, (bpm + 50u) / 100u, 0);
        ktext(&FONT_S, 9, 148, s, K_TEXT);
        if (m->speed != 100u) { p = knum(s, m->speed, 0); kput(p, "%"); ktext(&FONT_S, 81, 148, s, K_AMBER); }
        p = knum(s, secs / 60u, 0); p = kput(p, ":"); knum(p, secs % 60u, 2);
        ktextR(&FONT_S, 231, 148, s, K_DIM);
        for (i = 0; i < so->noct; i++) { o += so->octs[i].dir; if (o < lo) lo = o; if (o > hi) hi = o; }
        p = kput(s, "C"); p = knum(p, (uint32_t)(4 + lo), 0); p = kput(p, "-C"); knum(p, (uint32_t)(5 + hi), 0);
        ktext(&FONT_S, 9, 168, "RANGE", K_DIM); ktext(&FONT_S, 57, 168, s, K_CYAN);
        p = s; *p = 0;
        if (so->noct) p = kput(p, "OCT");
        {
            uint32_t chords = 0, k2;
            for (k2 = 1; k2 < so->nchart; k2++) if (so->chart[k2].t == so->chart[k2 - 1].t) chords = 1;
            if (chords) { if (p != s) p = kput(p, " "); p = kput(p, "CHORDS"); }
        }
        if (so->transpose) { if (p != s) p = kput(p, " "); p = kput(p, so->transpose > 0 ? "T+" : "T-"); knum(p, (uint32_t)(so->transpose > 0 ? so->transpose : -so->transpose), 0); }
        ktextR(&FONT_S, 231, 168, s, K_VIOLET);
        for (i = 0; i < 4u; i++) tw += (uint32_t)ktw(&FONT_S, KS_LEVELS[i]) + 10u;
        x = 120u - (tw + 18u) / 2u;
        for (i = 0; i < 4u; i++) {               /* the song's own level lit, the others locked (D12) */
            ks_pill((int32_t)x, 192, KS_LEVELS[i], i == so->level, K_AMBER, i != so->level);
            x += (uint32_t)ktw(&FONT_S, KS_LEVELS[i]) + 16u;
        }
    }
    (void)tone;
    ks_knobs(KN, 1u);                            /* (KNOB 1: nothing here, a song's level is its own) */
}

/* the sky over the highway, copied from flash into RAM once: from flash, every frame, it missed the flash cache */
static uint16_t ks_sky[240u * KF_SKY_H] __attribute__((section(".pool")));
static uint8_t ks_sky_ok;
static void ks_play(uint32_t ms, const uint8_t *keys_lit, const uint8_t *led, const char *num, const char *right)
{
    const kf_song_t *so = kg.song;
    int32_t pos = kg.vpos, boost = kg_boosting(), i, ocn;   /* (drawn where the song will be when the frame is seen) */
    uint8_t st[8] = {0};
    char s[24], *p;
    /* (the canvas starts as the background: vavehero.c's draw) the splash's sunset over the highway's far end, the
     * highway on the floor, its neon horizon */
    if (!ks_sky_ok) { for (i = 0; i < (int32_t)(240u * KF_SKY_H); i++) ks_sky[i] = KF_SKY[i]; ks_sky_ok = 1; }
    for (i = 0; i < (int32_t)(240u * KF_SKY_H); i++) cv_px[KF_SKY_Y * 240u + (uint32_t)i] = ks_sky[i];
    ks_highway_bg(boost, kg.spread);
    kfill_rows(KH_HIT, 222, K_BG);               /* (the keys' rows; the rest is drawn whole: vavehero.c's draw does not clear) */
    ks_beat_lines(pos, boost);
    krect(0, KH_TOPY, 240, 1, K_NEON);
    krect(0, KH_TOPY + 1, 240, 1, kmix(K_FLOOR0, K_NEON, Q12(.45)));
    for (i = 0; i < 8; i++) if (keys_lit[i]) ks_beam(i, LANE[i]);
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; i++) {   /* the holds' tails */
        const kf_cnote_t *n = &kg.notes[i];
        int32_t z0 = ks_z(n->t, pos), z1 = ks_z(n->t + n->len, pos), dead;
        if (z0 > KH_ZTOP_Q12) break;
        if (!(n->flags & 1u) || kg.nst[i] == KN_DONE || kg.nst[i] == KN_HIT) continue;
        if (z1 > KH_ZTOP_Q12) z1 = KH_ZTOP_Q12;
        if (z1 < 0) continue;
        dead = kg.nst[i] == KN_MISSED || kg.nst[i] == KN_BROKEN;
        ks_tail(n->lane, z0 < 0 ? 0 : z0, z1, dead ? 0x59406eu : LANE[n->lane], kg.nst[i] == KN_HELD);
    }
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; i++) {   /* chords: a bar under the notes that start together */
        const kf_cnote_t *n = &kg.notes[i];
        int32_t z = ks_z(n->t, pos), j, lo = n->lane, hi = n->lane;
        if (z > KH_ZTOP_Q12) break;
        if (z < -205 || kg.nst[i] != KN_PENDING || (i > 0 && kg.notes[i - 1].t == n->t && kg.nst[i - 1] == KN_PENDING)) continue;
        for (j = i + 1; j < (int32_t)kg.n && kg.notes[j].t == n->t; j++)
            if (kg.nst[j] == KN_PENDING) { if (kg.notes[j].lane < lo) lo = kg.notes[j].lane; if (kg.notes[j].lane > hi) hi = kg.notes[j].lane; }
        if (hi > lo) ks_chord_bar(lo, hi, z);
    }
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; i++) {   /* the gems */
        const kf_cnote_t *n = &kg.notes[i];
        int32_t z = ks_z(n->t, pos), ph = kg_ph((uint32_t)i);
        if (z > KH_ZTOP_Q12) break;
        if (z < -205 || kg.nst[i] != KN_PENDING) continue;
        ks_gem(n->lane, z < 0 ? 0 : z, LANE[n->lane], boost, ph >= 0 && !kg.ph_failed[ph]);
    }
    for (i = 0; i < (int32_t)so->noct && i < (int32_t)KG_MAXO; i++) {
        int32_t z = ks_z(so->octs[i].t, pos);
        if (kg.ost[i] == KO_PENDING && z >= -410 && z <= KH_ZTOP_Q12) ks_oct_band(z < 0 ? 0 : z, so->octs[i].dir, kg.level == 0 || kg.midi_used);
    }
    krect(KH_NL, KH_HIT - 2, KH_NR - KH_NL, 1, kmix(K_BG, boost ? K_GOLD : K_CYAN, Q12(.4)));
    krect(KH_NL, KH_HIT - 1, KH_NR - KH_NL, 2, boost ? K_GOLD : K_CYAN);
    for (i = 0; i < 8 && i < (int32_t)kg.nfx; i++) {
        uint32_t k = (kg.nfx - 1u - (uint32_t)i) % 8u, age = ms - kg.fx[k].ms;
        if (age < 260u) ks_burst(KH_NL + KH_LW * kg.fx[k].lane + 15, KH_HIT - 1, kg.fx[k].big ? LANE[kg.fx[k].lane] : K_GREEN, (int32_t)(age * 4096u / 260u), kg.fx[k].big);
    }
    for (i = 0; i < 8; i++) st[i] = keys_lit[i] ? 2 : 0;
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; i++) {   /* the next notes light their keys */
        const kf_cnote_t *n = &kg.notes[i];
        if ((int32_t)n->t * 256 > pos + KG_BEAT) break;
        if (kg.nst[i] == KN_PENDING && (int32_t)n->t * 256 >= pos - KG_BEAT / 10 && !st[n->lane]) st[n->lane] = 1;
    }
    ks_keys(st, led, kg.spread);
    if (kg.judge && ms - kg.judge_ms < 450u) {
        const char *jt = KJ_TEXT[kg.judge];
        int32_t y = 150 - (int32_t)((ms - kg.judge_ms) * 4u / 450u);
        ktextOut(&FONT_S, KH_CX - (ktw(&FONT_S, jt) >> 1), y, jt, kjcol(kg.judge), K_OUT, 1);
    }
    {
        int32_t prog = pos <= 0 ? 0 : pos / (((int32_t)so->end * KG_BEAT) >> 12);   /* Q12 (32-bit) */
        ks_header(num, so->title, right ? right : KS_LEVELS[kg.level], prog > 4096 ? 4096 : prog, right ? K_MUTE : K_AMBER);
    }
    knum(s, (uint32_t)kg.score, 6);
    ktextOut(&FONT_S, 4, 21, s, K_TEXT, K_OUT, 1);
    {                                            /* the OP meter */
        int32_t n = kg_ops_shown(), x0 = 120 - (6 * 12 + 5 * 2) / 2;
        rgb_t c = boost ? K_GOLD : K_AMBER;
        for (i = 0; i < 6; i++) {
            int32_t x = x0 + i * 14, y = 22;
            char d[2] = {(char)('1' + i), 0};
            if (i < n) { krect(x, y, 12, 14, c); krect(x + 1, y + 1, 10, 1, kmix(c, K_WHITE, Q12(.6))); }
            else { krect(x, y, 12, 14, 0x522e72u); krect(x + 1, y + 1, 10, 12, 0x200d30u); }
            ktext(&FONT_S, x + 2, y - 1, d, i < n ? K_INK : K_MUTE);
        }
    }
    p = kput(s, "X"); knum(p, (uint32_t)(kg_mult() * (boost ? 2 : 1)), 0);
    kpcap(204, 21, 32, 16, boost ? K_GOLD : K_AMBER);
    ktextC(&FONT_S, 220, 21, s, K_INK);
    ktext(&FONT_S, 4, 40, "COMBO", K_DIM);
    knum(s, (uint32_t)kg.combo, 0);
    ktext(&FONT_L, 4, 54, s, boost ? K_GOLD : K_TEXT);
    ocn = kg_oct_at(pos);
    if (ocn != 0) ktext(&FONT_S, 4, 92, ocn > 0 ? "OCT+" : "OCT-", (kg.oct == ocn || kg.level == 0 || kg.midi_used) ? K_VIOLET : K_CORAL);
    {
        int32_t banner_up = boost && pos - kg.boost_from < 3 * KG_BEAT;
        if (kg.plus_ms && ms - kg.plus_ms < 700u && !banner_up) ktextOut(&FONT_S, 108, 38, "+OP", K_AMBER, K_OUT, 1);
        if (banner_up) {
            krect(56, 44, 128, 30, kmix(K_BG, K_BLACK, Q12(.3)));
            krect(56, 43, 128, 1, K_GOLD); krect(56, 74, 128, 1, K_GOLD);
            ktextOut(&FONT_L, KH_CX - (ktw(&FONT_L, "OP BOOST") >> 1), 42, "OP BOOST", K_GOLD, K_OUT, 1);
        }
    }
    {
        static const char *const KN[4] = {"SWEEP", "TONE", "SPEED", "MIX"};
        ks_knobs(KN, 1u);
    }
    if (pos < 4 * KG_BEAT) {
        p = kput(s, KS_LEVELS[kg.level]);
        if (kg.speed != 100u) { p = kput(p, " "); p = knum(p, kg.speed, 0); kput(p, "%"); }
        { uint32_t n = 0; while (so->title[n]) n++; ks_banner(n > 14u ? num : so->title, s, K_CYAN, 92); }
    } else if (pos < 8 * KG_BEAT) {
        knum(s, (uint32_t)(8 - pos / KG_BEAT), 0);
        ks_banner(s, 0, K_AMBER, 99);
    }
    if (kg.done) ks_banner(kg.full ? "FULL COMBO" : "CLEAR", 0, kg.full ? K_GOLD : K_GREEN, 99);
}

static void ks_results(const char *num, uint32_t ms)
{
    const kf_song_t *so = kg.song;
    char s[24], *p;
    const char *const NAME[4] = {"PERFECT", "GREAT", "GOOD", "MISS"};
    const int32_t V[4] = {kg.cnt[0] + kg.cnt[4], kg.cnt[1], kg.cnt[2], kg.cnt[3]};
    const rgb_t COL[4] = {K_CYAN, K_BLUE, K_VIOLET, K_CORAL};
    int32_t i, x, m;
    char g[2] = {(char)kg.grade, 0};
    ks_night(ms);
    ks_header(num, so->title, KS_LEVELS[kg.level], -1, K_AMBER);
    ktext_chrome(&FONT_L, 8, 14, g, 2);           /* the grade in the logo's chrome */
    ktext(&FONT_S, 62, 24, "ACCURACY", K_DIM);
    p = knum(s, (uint32_t)kg.acc10 / 10u, 0); p = kput(p, "."); p = knum(p, (uint32_t)kg.acc10 % 10u, 0); kput(p, "%");
    ktext(&FONT_L, 62, 38, s, K_TEXT);
    knum(s, (uint32_t)kg.score, 6);
    ktext(&FONT_S, 62, 72, s, K_DIM);
    if (kg.new_best) { int32_t nb = ktw(&FONT_S, "NEW BEST") + 10; kpcap(236 - nb, 72, nb, 17, K_CYAN); ktext(&FONT_S, 241 - nb, 72, "NEW BEST", K_INK); }
    for (i = 0; i < 4; i++) {
        int32_t y = 96 + i * 18;
        ktext(&FONT_S, 4, y, NAME[i], COL[i]);
        krect(70, y + 7, 128, 4, 0x2b1440u);
        if (V[i] && kg.total) { int32_t w = (128 * V[i] + kg.total / 2) / kg.total; krect(70, y + 7, w < 3 ? 3 : w, 4, COL[i]); }
        knum(s, (uint32_t)V[i], 0);
        ktextR(&FONT_S, 236, y, s, K_TEXT);
    }
    ks_panel(2, 172, 236, 46);
    ktext(&FONT_S, 9, 176, "MAX COMBO", K_DIM);
    knum(s, (uint32_t)kg.max_combo, 0);
    ktextR(&FONT_S, 231, 176, s, K_TEXT);
    m = kg.dt_n ? (kg.dt_sum >= 0 ? (kg.dt_sum + kg.dt_n / 2) / kg.dt_n : -((-kg.dt_sum + kg.dt_n / 2) / kg.dt_n)) : 0;
    ktext(&FONT_S, 9, 196, "TIMING", K_DIM);
    if (m == 0) kput(s, "ON TIME");
    else { p = kput(s, m > 0 ? "+" : "-"); p = knum(p, (uint32_t)(m > 0 ? m : -m), 0); kput(p, m < 0 ? " MS EARLY" : " MS LATE"); }
    ktextR(&FONT_S, 231, 196, s, K_TEXT);
    krect(0, 222, 240, 18, K_SURF);
    x = ktext(&FONT_S, 6, 223, "PLAY", 0xffa52au);
    ktext(&FONT_S, x, 223, " RETRY", K_TEXT);
    x = 234 - ktw(&FONT_S, "HOME SONGS");
    x = ktext(&FONT_S, x, 223, "HOME", K_DIM);
    ktext(&FONT_S, x, 223, " SONGS", K_TEXT);
}
static void ks_toast(const char *t)
{
    int32_t w = ktw(&FONT_S, t) + 20, x = 120 - w / 2;
    krect(x, 96, w, 28, kmix(K_SURF2, K_NEON, Q12(.6)));
    krect(x + 1, 97, w - 2, 26, K_SURF2);
    ktextC(&FONT_S, 120, 102, t, K_TEXT);
}
