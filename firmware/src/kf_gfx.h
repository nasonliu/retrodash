/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk) */
/* VaveHero's drawing on Dinghy's canvas (gfx.c), the whole screen at once (CV_MAX 240 x 240): the reference's
 * pixel shapes (reference/vavehero.html, "pixel shapes", "the highway", "Terminus"), in integers.
 *
 *   colours   0xRRGGBB as the reference writes them; kmix() is its mix(), c565() the screen's
 *   text      Terminus, alpha-blended onto what is there (the reference's text, textOut, textC, textR), scale 1 or 2
 *   pcap      the capsule (keys, gems, pills), rounded as the reference rounds it
 *   highway   the perspective: Y(z) for a depth, the lanes' x at a row, the fade at the top */
#pragma once
#include <stdint.h>

typedef uint32_t rgb_t;
/* the reference's palette (P), the knobs' colours (KC), the lanes (LANE) */
#define K_BG 0x0c041eu
#define K_SURF 0x1d0d38u
#define K_SURF2 0x2e1752u
#define K_LINE 0x47226fu
#define K_TEXT 0xf4eefau
#define K_DIM 0xa58cccu
#define K_MUTE 0x7a5aa0u
#define K_CYAN 0x5fe3ffu
#define K_AMBER 0xffb347u
#define K_VIOLET 0xb388ffu
#define K_CORAL 0xff6b5au
#define K_BLUE 0x5b9bffu
#define K_GOLD 0xffd36bu
#define K_INK 0x1a1206u
#define K_GREEN 0x6ee7a0u
#define K_WHITE 0xffffffu
#define K_BLACK 0x000000u
/* the splash's: its horizon's neon, the floor's grid, the floor from the horizon to you, the ink round text on the art */
#define K_NEON 0xff4fd8u
#define K_GRID 0xff3df0u
#define K_FLOOR0 0x1a0638u
#define K_FLOOR1 0x06020eu
#define K_OUT 0x0b0220u
static const rgb_t KC[4] = {0x5b9be0u, 0xf59a3au, 0xf2e4c4u, 0xee6048u};
static const rgb_t LANE[8] = {0x2ee6c8u, 0x3fc1ffu, 0x5b9bffu, 0x8b7dffu, 0xb86cffu, 0xff5fc8u, 0xff7a59u, 0xffb43du};

static inline uint16_t c565(rgb_t c) { return (uint16_t)(((c >> 19) & 31u) << 11 | ((c >> 10) & 63u) << 5 | ((c >> 3) & 31u)); }
static rgb_t kmix(rgb_t a, rgb_t b, int32_t t)   /* a + (b - a) t, t Q12, each channel rounded (the reference's mix) */
{
    rgb_t o = 0;
    int32_t sh;
    for (sh = 16; sh >= 0; sh -= 8) {
        int32_t va = (int32_t)(a >> sh) & 255, vb = (int32_t)(b >> sh) & 255, v = va + (((vb - va) * t + 2048) >> 12);
        o |= (rgb_t)(v < 0 ? 0 : v > 255 ? 255 : v) << sh;
    }
    return o;
}
#define Q12(x) ((int32_t)((x) * 4096 + .5))     /* a constant fraction, Q12 (folded by the compiler: no float at run time) */

/* ---- pixels (the canvas keeps RGB565, byte-swapped) */
static inline rgb_t kpx_get(int32_t x, int32_t y)
{
    uint32_t c = swap16(cv_px[(uint32_t)y * cv_w + (uint32_t)x]);
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}
/* a pixel blended in RGB565 itself (the canvas's own form: a little coarser than the reference's 8-bit mix, and
 * much cheaper), a in 1/64 */
static inline uint16_t kblend565(uint16_t under_sw, uint32_t c, uint32_t a)
{
    uint32_t u = swap16(under_sw);
    int32_t ur = (int32_t)(u >> 11), ug = (int32_t)((u >> 5) & 63u), ub = (int32_t)(u & 31u);
    int32_t cr = (int32_t)(c >> 11), cg = (int32_t)((c >> 5) & 63u), cb = (int32_t)(c & 31u), aa = (int32_t)a;
    ur += ((cr - ur) * aa + 32) >> 6; ug += ((cg - ug) * aa + 32) >> 6; ub += ((cb - ub) * aa + 32) >> 6;
    return swap16((uint32_t)ur << 11 | (uint32_t)ug << 5 | (uint32_t)ub);
}
/* the same blend with the colour split once (a glyph, a rectangle: one colour for many pixels); the same numbers */
typedef struct { int32_t r, g, b; uint16_t sw; } kcol_t;
static inline kcol_t kcol(uint32_t c565v)
{
    kcol_t k = {(int32_t)(c565v >> 11), (int32_t)((c565v >> 5) & 63u), (int32_t)(c565v & 31u), swap16(c565v)};
    return k;
}
static inline uint16_t kblend_k(uint16_t under_sw, const kcol_t *k, int32_t aa)
{
    uint32_t u = swap16(under_sw);
    int32_t ur = (int32_t)(u >> 11), ug = (int32_t)((u >> 5) & 63u), ub = (int32_t)(u & 31u);
    ur += ((k->r - ur) * aa + 32) >> 6; ug += ((k->g - ug) * aa + 32) >> 6; ub += ((k->b - ub) * aa + 32) >> 6;
    return swap16((uint32_t)ur << 11 | (uint32_t)ug << 5 | (uint32_t)ub);
}
static const uint8_t KA64[16] = {0, 4, 8, 12, 17, 21, 25, 29, 34, 38, 42, 46, 51, 55, 59, 64};   /* v * 64 / 15 */
static inline void kpx_text(int32_t x, int32_t y, uint32_t c565v, uint32_t v)   /* a glyph's pixel, v 1..15 */
{
    uint16_t *px;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h) return;
    px = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    *px = v == 15u ? swap16(c565v) : kblend565(*px, c565v, v * 64u / 15u);
}
static inline void kpx_blend(int32_t x, int32_t y, rgb_t c, int32_t a)   /* a Q12 */
{
    uint16_t *px;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h || a <= 0) return;
    px = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    *px = a >= 4096 ? swap16(c565(c)) : kblend565(*px, c565(c), (uint32_t)a >> 6);
}
static void krect(int32_t x, int32_t y, int32_t w, int32_t h, rgb_t c) { if (w > 0 && h > 0) cv_rect(x, y, w, h, c565(c)); }
static void krect_a(int32_t x, int32_t y, int32_t w, int32_t h, rgb_t c, int32_t a)   /* blended, a Q12 */
{
    int32_t i, j, x1 = x + w, y1 = y + h, aa = a >> 6;
    kcol_t k = kcol(c565(c));
    if (a <= 0) return;
    if (x < 0) x = 0;                            /* (clipped once) */
    if (y < 0) y = 0;
    if (x1 > (int32_t)cv_w) x1 = (int32_t)cv_w;
    if (y1 > (int32_t)cv_h) y1 = (int32_t)cv_h;
    for (j = y; j < y1; j++) {
        uint16_t *row = cv_px + (uint32_t)j * cv_w;
        if (a >= 4096) for (i = x; i < x1; i++) row[i] = k.sw;
        else for (i = x; i < x1; i++) row[i] = kblend_k(row[i], &k, aa);
    }
}
static void kfill_rows(int32_t y0, int32_t y1, rgb_t c)   /* whole rows of the canvas, two pixels a store */
{
    uint16_t sw = swap16(c565(c));
    if (y0 < 0) y0 = 0;
    if (y1 > (int32_t)cv_h) y1 = (int32_t)cv_h;
    if (y1 <= y0) return;
    if (((uintptr_t)cv_px & 3u) == 0u && (cv_w & 1u) == 0u) {
        uint32_t *p = (uint32_t *)(void *)(cv_px + (uint32_t)y0 * cv_w), n = (uint32_t)(y1 - y0) * cv_w / 2u, v = (uint32_t)sw | (uint32_t)sw << 16, i;
        for (i = 0; i < n; i++) p[i] = v;
    } else {
        uint32_t i, n = (uint32_t)(y1 - y0) * cv_w;
        uint16_t *p = cv_px + (uint32_t)y0 * cv_w;
        for (i = 0; i < n; i++) p[i] = sw;
    }
}

/* ---- the capsule: each row inset as a circle of radius min(w, h) / 2 would (x 0.92), standing ones round at the
 * top and the bottom; worked in half pixels with an integer square root */
static uint32_t kisqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}
static void kpcap(int32_t x, int32_t y, int32_t w, int32_t h, rgb_t c)
{
    int32_t j, d = w < h ? w : h;                /* d = 2r */
    uint16_t sc;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    d = w < h ? w : h;
    sc = c565(c);
    for (j = 0; j < h; j++) {
        int32_t yc2 = 2 * j + 1, dy2, ins = 0;  /* half pixels: yc = j + .5, r = d / 2 */
        if (h <= w) dy2 = yc2 > h ? yc2 - h : h - yc2;
        else dy2 = yc2 < d ? d - yc2 : yc2 > 2 * h - d ? yc2 - (2 * h - d) : 0;
        if (d >= 4) {                            /* r >= 2 */
            int32_t s16 = dy2 >= d ? 0 : (int32_t)kisqrt((uint32_t)(d * d - dy2 * dy2) * 256u);
            ins = (23 * (16 * d - s16) + 400) / 800;   /* round(.92 (r - sqrt(r^2 - dy^2))) */
        }
        cv_rect(x + ins, y + j, w - 2 * ins < 1 ? 1 : w - 2 * ins, 1, sc);
    }
}

/* ---- text: Terminus as cv_text has it, but blended onto what is under it, and at scale 1 or 2 */
static int32_t ktext_n(const felucca_font_t *f, int32_t x, int32_t y, const char *s, uint32_t n, rgb_t col, int32_t sc)
{
    uint32_t k, cc = c565(col);
    kcol_t kc = kcol(cc);
    for (k = 0; k < n && s[k]; k++) {
        uint32_t gi = glyph(f, (uint8_t)s[k]), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        int32_t x0 = x - f->pad * sc;
        if (s[k] != ' ' && sc == 1 && x0 >= 0 && x0 + (int32_t)w <= (int32_t)cv_w && y >= 0 && y + (int32_t)f->h <= (int32_t)cv_h) {
            for (gy = 0; gy < f->h; gy++) {      /* all of it on the canvas: no checks a pixel, the colour split once */
                const uint8_t *src = gd + gy * bpr;
                uint16_t *row = cv_px + (uint32_t)(y + (int32_t)gy) * cv_w + (uint32_t)x0;
                for (gx = 0; gx < w; gx += 2u) {
                    uint32_t b = src[gx >> 1], v;
                    if (!b) continue;
                    if ((v = b >> 4) != 0u) row[gx] = v == 15u ? kc.sw : kblend_k(row[gx], &kc, KA64[v]);
                    if ((v = b & 15u) != 0u && gx + 1u < w) row[gx + 1u] = v == 15u ? kc.sw : kblend_k(row[gx + 1u], &kc, KA64[v]);
                }
            }
        } else if (s[k] != ' ')
            for (gy = 0; gy < f->h; gy++)
                for (gx = 0; gx < w; gx += 2u) {   /* (two pixels a byte: an empty byte skips both) */
                    uint32_t b = gd[gy * bpr + gx / 2u], h;
                    if (!b) continue;
                    for (h = 0; h < 2u && gx + h < w; h++) {
                        uint32_t v = h ? (b & 15u) : (b >> 4);
                        if (v) {
                            int32_t px = x - f->pad * sc + (int32_t)(gx + h) * sc, py = y + (int32_t)gy * sc, ii, jj;
                            for (jj = 0; jj < sc; jj++) for (ii = 0; ii < sc; ii++) kpx_text(px + ii, py + jj, cc, v);
                        }
                    }
                }
        x += f->adv[gi] * sc;
    }
    return x;
}
static int32_t ktext(const felucca_font_t *f, int32_t x, int32_t y, const char *s, rgb_t col) { return ktext_n(f, x, y, s, 999u, col, 1); }
static int32_t ktw(const felucca_font_t *f, const char *s) { return text_w(f, s); }
static void ktextC(const felucca_font_t *f, int32_t cx, int32_t y, const char *s, rgb_t col)
{
    ktext(f, cx - (ktw(f, s) >> 1), y, s, col);  /* round(cx - w / 2) */
}
static void ktextR(const felucca_font_t *f, int32_t rx, int32_t y, const char *s, rgb_t col) { ktext(f, rx - ktw(f, s), y, s, col); }
static int32_t ktextOut(const felucca_font_t *f, int32_t x, int32_t y, const char *s, rgb_t col, rgb_t out, int32_t sc)
{
    static const int8_t D8[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
    uint32_t k;
    for (k = 0; k < 8u; k++) ktext_n(f, x + D8[k][0], y + D8[k][1], s, 999u, out, sc);
    return ktext_n(f, x, y, s, 999u, col, sc);
}

/* ---- the logo's chrome on Terminus (tools/gen_splash.py, logo): down the letters white, pale blue, blue, the horizon's
 * white line, violet, pink, orange, yellow; an edge below and to the right, a violet outline */
static rgb_t kchrome(int32_t t)                  /* t Q12, 0 the letters' top, 4096 their foot */
{
    static const int16_t T[8] = {0, 958, 2129, 2130, 2523, 3113, 3662, 4096};
    static const rgb_t C[8] = {0xffffffu, 0xe4f6ffu, 0x62b4ffu, 0x6a2aa0u, 0xa62a9au, 0xff3d8au, 0xff8a3du, 0xffe27au};
    int32_t k;
    if (t <= 0) return C[0];
    for (k = 1; k < 8; k++)
        if (t <= T[k]) return kmix(C[k - 1], C[k], (t - T[k - 1]) * 4096 / (T[k] - T[k - 1]));
    return C[7];
}
static void kfont_caps(const felucca_font_t *f, int32_t *top, int32_t *bot)   /* the rows a capital H inks */
{
    uint32_t gi = glyph(f, 'H'), w = f->bw[gi], bpr = (w + 1u) / 2u, gy, gx;
    const uint8_t *gd = f->data + f->off[gi];
    *top = -1; *bot = 0;
    for (gy = 0; gy < f->h; gy++)
        for (gx = 0; gx < w; gx++) {
            uint32_t v = gd[gy * bpr + gx / 2u];
            if ((gx & 1u) ? (v & 15u) : (v >> 4)) { if (*top < 0) *top = (int32_t)gy; *bot = (int32_t)gy; break; }
        }
    if (*top < 0) { *top = 0; *bot = (int32_t)f->h - 1; }
}
static int32_t ktext_chrome(const felucca_font_t *f, int32_t x, int32_t y, const char *s, int32_t sc)
{
    static const int8_t D8[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
    static const int8_t DE[3][2] = {{3, 3}, {3, 2}, {2, 3}};
    uint16_t rc[64];
    int32_t top, bot, j, k, n = (int32_t)f->h * sc;
    uint32_t i;
    kfont_caps(f, &top, &bot);
    for (j = 0; j < n && j < 64; j++) {          /* a colour a row, along the caps' height (the horizon a little whiter) */
        int32_t t = ((2 * j + 1) * 4096 / (2 * sc) - top * 4096) / (bot - top + 1);
        rgb_t c = kchrome(t);
        if (t >= 2010 && t < 2250) c = kmix(c, K_WHITE, Q12(.7));
        rc[j] = (uint16_t)c565(c);
    }
    for (k = 0; k < 8; k++) ktext_n(f, x + D8[k][0] * sc, y + D8[k][1] * sc, s, 999u, 0x3a1470u, sc);
    for (k = 0; k < 3; k++) ktext_n(f, x + DE[k][0] * sc, y + DE[k][1] * sc, s, 999u, 0x3a1470u, sc);
    ktext_n(f, x + 2 * sc, y + 2 * sc, s, 999u, 0x5a1a8au, sc);
    ktext_n(f, x + sc, y + sc, s, 999u, 0xb04ad8u, sc);
    for (i = 0; s[i]; i++) {                     /* the face, its rows coloured */
        uint32_t gi = glyph(f, (uint8_t)s[i]), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        if (s[i] != ' ')
            for (gy = 0; gy < f->h; gy++)
                for (gx = 0; gx < w; gx++) {
                    uint32_t v = gd[gy * bpr + gx / 2u];
                    v = (gx & 1u) ? (v & 15u) : (v >> 4);
                    if (v) {
                        int32_t px = x - f->pad * sc + (int32_t)gx * sc, py = y + (int32_t)gy * sc, ii, jj;
                        for (jj = 0; jj < sc; jj++) for (ii = 0; ii < sc; ii++) kpx_text(px + ii, py + jj, rc[(int32_t)gy * sc + jj < 64 ? (int32_t)gy * sc + jj : 63], v);
                    }
                }
        x += f->adv[gi] * sc;
    }
    return x;
}

/* ---- numbers and strings, small */
static char *kput(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static char *knum(char *p, uint32_t v, int32_t pad)   /* decimal, zero-padded to pad digits */
{
    char t[12];
    int32_t n = 0;
    do t[n++] = (char)('0' + v % 10u); while ((v /= 10u) && n < 11);
    while (n < pad) t[n++] = '0';
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}

/* ---- the highway (reference/vavehero.html, "the highway"): HIT 190, TOPY 40, lanes from 4 to 236 (29 px), the eye 3
 * deep; a row's scale s = (y - HY) / (HIT - HY), the depth's row Y(z) = HY + (HIT - HY) 3 / (3 + z) */
#define KH_HIT 190
#define KH_TOPY 40
#define KH_NL 4
#define KH_NR 236
#define KH_CX 120
#define KH_LW 29
#define KH_HYQ (-4317647)                        /* HY = 190 - 150 / (1 - 96 / 232) = -65.882, Q16 */
#define KH_DENQ 16769647                         /* HIT - HY = 255.882, Q16 */
#define KH_ZTOP_Q12 17408                        /* the far end: 3 / (96 / 232) - 3 = 4.25, Q12 */
#define KH_ROWS (KH_HIT - KH_TOPY)
static int16_t kh_s[KH_ROWS];                    /* a row's scale at its middle (y + .5), Q12 */
static uint8_t kh_x[KH_ROWS][9];                 /* a row's lane edges, round(X(U(b), s)) */
static int16_t kh_fade[KH_ROWS];                 /* clamp((y - TOPY) / 26, 0, 1), Q12 */
static uint16_t kh_lane[2][KH_ROWS][8];          /* the lanes' colours a row (normal, boost), swapped RGB565 */
static uint16_t kh_line[3][KH_ROWS][9];          /* the lane lines' (normal, boost, SPREAD) */
/* the floor the highway runs on, the splash's: its colour a row (what the far end fades into), and its grid, the lane
 * lines carried on beyond the edges to the same vanishing point */
#define KH_NGX 8
static rgb_t kh_floor[KH_ROWS];
static uint16_t kh_floor565[KH_ROWS], kh_gridc[KH_ROWS];
static uint8_t kh_gx[KH_ROWS][2 * KH_NGX];       /* x, 255 off the screen */
static int32_t kh_s_at(int32_t y2)               /* the scale at y2 / 2 (half pixels), Q12 */
{
    return (y2 * 32768 - KH_HYQ) / (KH_DENQ >> 12);   /* (32-bit: y2 < 480) */
}
static int32_t kh_X(int32_t u_q4, int32_t s_q12) /* X(u, s) = CX + (u - CX) s, u in 1/16 px: x Q12 */
{
    return KH_CX * 4096 + (((u_q4 - KH_CX * 16) * s_q12) >> 4);
}
static int32_t kh_round(int32_t v_q12) { return v_q12 >= 0 ? (v_q12 + 2048) >> 12 : -((-v_q12 + 2047) >> 12); }
static int32_t kh_U4(int32_t p_q8) { return KH_NL * 16 + ((KH_LW * 16 * p_q8) >> 8); }   /* U(p) = NL + p LW, p Q8, in 1/16 px */
static int32_t kh_sc(int32_t z_q12)              /* sc(z) = 3 / (3 + z), Q16 */
{
    return (int32_t)(805306368u / (uint32_t)(12288 + (z_q12 < -6000 ? -6000 : z_q12)));
}
static int32_t kh_Yq(int32_t z_q12)              /* Y(z), Q12 */
{
    return (int32_t)(((int64_t)KH_HYQ + (((int64_t)(KH_DENQ >> 4) * kh_sc(z_q12)) >> 12)) >> 4);
}
static int32_t kh_fade_at(int32_t y)             /* the fade at a row, Q12 */
{
    int32_t f = (y - KH_TOPY) * 4096 / 26;
    return f < 0 ? 0 : f > 4096 ? 4096 : f;
}
static rgb_t kh_floor_at(int32_t y) { return kh_floor[y < KH_TOPY ? 0 : y >= KH_HIT ? KH_ROWS - 1 : y - KH_TOPY]; }
static rgb_t kh_fc(int32_t y, rgb_t c)           /* c at row y, the far end fading into the floor */
{
    return y >= KH_HIT ? c : kmix(kh_floor_at(y), c, kh_fade_at(y));
}
static rgb_t kh_grid_h(int32_t y, int32_t bar)   /* a beat line carried on across the floor */
{
    int32_t near = (y - KH_TOPY) * 4096 / KH_ROWS;
    if (near < 0) near = 0;
    return kmix(kh_floor_at(y), K_GRID, (bar ? Q12(.30) : Q12(.16)) + near * (bar ? 55 : 40) / 100);
}
static void kh_init(void)
{
    static const uint8_t BLACK_B[9] = {0, 1, 1, 0, 1, 1, 1, 0, 0};   /* lane lines with a black key */
    int32_t y, b, v;
    for (y = KH_TOPY; y < KH_HIT; y++) {
        int32_t s = kh_s_at(2 * y + 1), r = y - KH_TOPY, f = kh_fade_at(y), near = r * 4096 / KH_ROWS;
        kh_s[r] = (int16_t)s;
        kh_fade[r] = (int16_t)f;
        kh_floor[r] = kmix(K_FLOOR0, K_FLOOR1, near);
        kh_floor565[r] = swap16(c565(kh_floor[r]));
        kh_gridc[r] = swap16(c565(kmix(kh_floor[r], K_GRID, Q12(.16) + near / 2)));
        for (b = 0; b < KH_NGX; b++) {
            int32_t xl = kh_round(kh_X((KH_NL - (b + 1) * KH_LW) * 16, s)), xr = kh_round(kh_X((KH_NR + (b + 1) * KH_LW) * 16, s));
            kh_gx[r][b] = (uint8_t)(xl >= 0 && xl < 240 ? xl : 255);
            kh_gx[r][KH_NGX + b] = (uint8_t)(xr >= 0 && xr < 240 ? xr : 255);
        }
        for (b = 0; b <= 8; b++) kh_x[r][b] = (uint8_t)kh_round(kh_X(kh_U4(b * 256), s));
        for (b = 0; b < 8; b++)                  /* (the reference's highwayBg) */
            for (v = 0; v < 2; v++) {
                rgb_t col = b % 2 ? 0x1a0a29u : 0x14071fu;
                col = v ? kmix(col, b % 2 ? 0x6a3cb0u : 0x4a2a86u, Q12(.18) + near * 901 / 4096) : kmix(col, LANE[b], Q12(.06) + near * 615 / 4096);
                kh_lane[v][r][b] = swap16(c565(kmix(kh_floor[r], col, f)));
            }
        for (b = 0; b <= 8; b++)
            for (v = 0; v < 3; v++) {
                int32_t edge = b == 0 || b == 8;
                rgb_t col = edge ? (v == 1 ? K_GOLD : 0xd043b8u) : v == 2 ? 0x3f1e5bu : BLACK_B[b] ? 0x4a1f6eu : 0x2b1340u;
                kh_line[v][r][b] = swap16(c565(kmix(kh_floor[r], col, f)));
            }
    }
}
