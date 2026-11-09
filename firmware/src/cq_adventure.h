/* SPDX-License-Identifier: GPL-3.0-only
 * ChipQuest's first horizontal play screen, using VaveHero's audio clock and judgement. */
#pragma once
#include "cq_stage.h"
static int32_t cq_note_x(int32_t t96, int32_t pos) {
    int32_t delta = t96 * 256 - pos;
    return 56 + (int32_t)((int64_t)delta * 150 / (4 * KG_BEAT));
}
static void cq_play_fixed(uint32_t ms, const uint8_t *keys_lit, const char *num, const char *right) {
    int32_t i, x, hit = (kg.judge == KJ_PERFECT || kg.judge == KJ_GREAT || kg.judge == KJ_GOOD) && ms - kg.judge_ms < 180u;
    char s[24];
    kfill_rows(0, 240, 0x101629u);
    krect(0, 24, 240, 37, 0x1d3150u);
    krect(31, 32, 8, 8, 0xf4d6b4u); krect(29, 40, 12, 12, 0x5dd0e8u);
    krect(30, 52, 4, 7, K_WHITE); krect(37, 52, 4, 7, K_WHITE);
    krect(41, 43, hit ? 15 : 7, 2, hit ? K_GOLD : K_WHITE);
    krect(53, 62, 2, 136, K_CYAN);
    for (i = 0; i < 27; ++i) {
        int y = 63 + i * 5;
        krect(4, y, 34, 4, keys_lit[i] ? LANE[i % 8] : 0x27354au);
        krect(56, y + 2, 176, 1, 0x364658u);
    }
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; ++i) {
        const kf_cnote_t *n = &kg.notes[i];
        if (kg.nst[i] != KN_PENDING && kg.nst[i] != KN_HELD) continue;
        x = cq_note_x(n->t, kg.vpos);
        if (x > 232) break;
        if (x < 40 || n->lane >= 27) continue;
        if ((n->flags & 1u) && n->len) {
            int32_t end = cq_note_x(n->t + n->len, kg.vpos);
            if (end > 232) end = 232;
            if (end > x) krect(x, 64 + n->lane * 5, end - x, 2, LANE[n->lane % 8]);
        }
        krect(x - 2, 63 + n->lane * 5, 5, 4, LANE[n->lane % 8]);
    }
    if (kg.judge && ms - kg.judge_ms < 450u) ktext(&FONT_S, 85, 29, KJ_TEXT[kg.judge], kjcol(kg.judge));
    ks_header(num, kg.song->title, right ? right : "FIXED", -1, K_AMBER);
    ktext(&FONT_S, 5, 205, "COMBO", K_DIM); knum(s, (uint32_t)kg.combo, 0); ktext(&FONT_L, 48, 202, s, K_TEXT);
    knum(s, (uint32_t)kg.score, 6); ktext(&FONT_S, 150, 206, s, K_TEXT);
    if (kg.done) ks_banner(kg.full ? "FULL COMBO" : "CLEAR", 0, kg.full ? K_GOLD : K_GREEN, 99);
}
static void cq_play(uint32_t ms, const uint8_t *keys_lit, const uint8_t *led, const char *num, const char *right) {
    if (cq_ready && kg.idx == CQ_SLOT) { (void)led; cq_stage(ms, keys_lit, right); return; }
    if (kg.song->transpose == 127) { (void)led; cq_play_fixed(ms, keys_lit, num, right); return; }
    int32_t i, x, phase = (kg.vpos / (KG_BEAT / 4)) & 1;
    int32_t attack = (kg.judge == KJ_PERFECT || kg.judge == KJ_GREAT || kg.judge == KJ_GOOD) && ms - kg.judge_ms < 180u;
    char s[24];
    static const uint16_t digit[8] = {0x2492, 0x7397, 0x73cf, 0x5bc9, 0x79cf, 0x79ef, 0x7249, 0x7bef};
    (void)led;
    kfill_rows(0, 240, 0x101629u);
    krect(0, 30, 240, 64, 0x1d3150u);
    krect(0, 91, 240, 3, 0x668b90u);
    for (i = 0; i < 8; ++i) {
        int32_t y = 100 + i * 11;
        krect(54, y + 5, 181, 1, 0x364658u);
        krect(5, y, 32, 9, keys_lit[i] ? LANE[i] : 0x27354au);
        { int32_t d; for (d = 0; d < 15; ++d)
            if (digit[i] & (1u << (14 - d))) krect(19 + d % 3, y + 2 + d / 3, 1, 1, keys_lit[i] ? K_WHITE : LANE[i]); }
    }
    krect(54, 97, 2, 90, K_CYAN);
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; ++i) {
        const kf_cnote_t *n = &kg.notes[i];
        if (kg.nst[i] != KN_PENDING && kg.nst[i] != KN_HELD) continue;
        x = cq_note_x(n->t, kg.vpos);
        if (x > 232) break;
        if (x < 40 || n->lane >= 8) continue;
        if ((n->flags & 1u) && n->len) {
            int32_t end = cq_note_x(n->t + n->len, kg.vpos);
            if (end > 233) end = 233;
            if (end > x) krect(x, 103 + n->lane * 11, end - x, 3, LANE[n->lane]);
        }
        krect(x - 3, 100 + n->lane * 11, 7, 8, LANE[n->lane]);
    }
    /* Original pixel character: stepping on the beat and slashing on a judged hit. */
    krect(32, 53, 10, 10, 0xf4d6b4u);
    krect(30, 63, 14, 16, 0x5dd0e8u);
    krect(31 + phase * 3, 79, 4, 10, K_WHITE);
    krect(39 - phase * 3, 79, 4, 10, K_WHITE);
    krect(42, 65, attack ? 18 : 8, 3, attack ? K_GOLD : K_WHITE);
    if (attack) { krect(61, 58, 3, 18, K_GOLD); krect(59, 64, 8, 3, K_GOLD); }
    for (i = (int32_t)kg.first; i < (int32_t)kg.n; ++i) {
        if (kg.nst[i] != KN_PENDING) continue;
        x = cq_note_x(kg.notes[i].t, kg.vpos);
        if (x >= 65 && x < 225) {
            krect(x, 75, 12, 10, 0x8bd68cu);
            krect(x + 2, 72, 8, 3, 0x8bd68cu);
            krect(x + 3, 78, 2, 2, K_INK); krect(x + 8, 78, 2, 2, K_INK);
        }
        break;
    }
    { int32_t oct = kg_oct_at(kg.vpos);
      if (oct) ktext(&FONT_S, 185, 39, oct > 0 ? "OCT+" : "OCT-", K_VIOLET); }
    if (kg.judge && ms - kg.judge_ms < 450u) ktext(&FONT_S, 80, 40, KJ_TEXT[kg.judge], kjcol(kg.judge));
    ks_header(num, kg.song->title, right ? right : KS_LEVELS[kg.level], -1, K_AMBER);
    ktext(&FONT_S, 5, 194, "COMBO", K_DIM);
    knum(s, (uint32_t)kg.combo, 0); ktext(&FONT_L, 48, 191, s, K_TEXT);
    knum(s, (uint32_t)kg.score, 6); ktext(&FONT_S, 150, 195, s, K_TEXT);
    if (kg.done) ks_banner(kg.full ? "FULL COMBO" : "CLEAR", 0, kg.full ? K_GOLD : K_GREEN, 99);
}
