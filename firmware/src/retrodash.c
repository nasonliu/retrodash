/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Joakim (joakimskurk); from Dinghy's dinghy.c by zvenson */
/* VaveHero: a rhythm game for the M-VAVE FM-1 (README.md, docs/concept.md). The notes fall down a highway, a lane
 * a white key; you play them on the keys, the key LEDs show what comes. The browser reference
 * (reference/vavehero.html) is the fasit for how it plays, looks and sounds; this is it on the FM-1.
 *
 *   splash     PLAY: the songs. FX: the DEMO (ENGINE TOUR from bar 41 on autoplay: the engine's worst case,
 *              which fm1-emu times in CI); HOME or PLAY ends it
 *   songs      SELECT the song (the last row: MIDI MODE; each song has its one level, D12), KNOB 2 the voice (TONE:
 *              SONG or one of five), KNOB 3 the speed (50-100 %), KNOB 4 MIX, ALGORITHM GRIT (D7, everywhere but
 *              in a song, as MIX); PLAY starts, GLO
 *              turns SPREAD on and off (D2), SEQ AUTO (the song plays itself, its result is not kept), HOME back
 *   play       the eight keys of the layout play the lanes (C4..C5, or SPREAD's every other white key); OCT- OCT+
 *              follow the octave shifts; FX: OP BOOST when the six OPs are lit; PLAY pauses, HOME quits
 *   results    PLAY again, HOME the songs; the best result of each song is kept in flash
 *   MIDI MODE  LIVE (D5): the FM-1 is the song's sound module, a part a channel (D3), keyswitches on notes 0-11
 *              (D6); the keys play channel 1's voice and go out on channel 1 for the DAW to record; HOME leaves
 *
 * The time: the song runs on the audio clock (kf_now, frames), and a key is judged by the frame that was being heard
 * when it went down: its edge is read in LCD_IDLE too (while the screen's DMA runs), so a long frame does not make
 * it late.
 *
 * Rules of the road (the chip has no FPU and no libgcc): integers only, no float, no 64-bit division.
 * tests/host.c runs this file on a computer: the screen as a PNG, the sound as a WAV.
 * What platform.c offers: docs/API.md. The chip's rules: docs/RULES.md. */
#define APP_NAME "RETRODASH"                      /* the rescue and the crash screen say this */
#define STORE_MAGIC 0x43515032u                 /* "VHRO": VaveHero's settings and blobs, never another fork's */
#define CV_MAX (240u * 240u)                    /* the whole screen in one canvas (113 KB of the pool) */
static void app_idle(void);
#define LCD_IDLE app_idle                       /* while the screen's DMA runs: keys and MIDI in (platform.c) */
#include <stdint.h>
static void app_note_edge(uint32_t key, uint32_t down);
#define FM1_NOTE_EDGE(key, down) app_note_edge((key), (down))   /* each key's edge stamped by the scan (fm1_input.h) */
#ifndef LCD_BAUD
#define LCD_BAUD 0u /* FM-1 display SPI: 60 MHz; measured in the device profile. */
#endif
#include "platform.c"
#include "kf_engine.h"
#include "kf_song.h"
#include "kf_bench.h"
static volatile uint32_t rd_click_trigger,rd_click_amp;
static uint32_t rd_scroll_speed=100; /* visual speed, independent of song tempo */
static void rd_hit_feedback(uint32_t judge){
#ifndef RD_SPC_REFERENCE
rd_click_amp=judge==1u?1000u:judge==2u?800u:650u;rd_click_trigger++;
#else
(void)judge;
#endif
}
#ifdef RD_SPC
#define KG_HIT_FEEDBACK(index,judge) (rd_hit_feedback((uint32_t)(judge)),rd_spc_hit(index))
#else
#define KG_HIT_FEEDBACK(index,judge) rd_hit_feedback((uint32_t)(judge))
#endif
#include "kf_screens.h"
#include "cq_bundle.h"
#include "cq_adventure.h"

/* ---- the sound, and the audio clock as heard */
static volatile uint32_t rd_volume = 80;
static volatile uint32_t au_frames, au_ms;     /* kf_now and fm1_ms when the last block was rendered */
#if defined(RD_SPC) && defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
static volatile uint32_t rd_audio_total,rd_audio_max,rd_audio_blocks,rd_audio_late;
#endif
static void app_audio(int32_t *out, uint32_t n)
{
#if defined(RD_SPC) && defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
    uint32_t audio_start=fm1_ticks();
#endif
    static uint32_t seen,phase,left;
    kf_render(out,n);
    if(seen!=rd_click_trigger){seen=rd_click_trigger;phase=0;left=529;}
    for(uint32_t i=0;i<n;i++){
        int32_t click=0;
        if(left){int32_t amp=(int32_t)(rd_click_amp*left/529u);click=kf_sine[phase>>20]*amp/32768;phase+=233739717u;left--;}
        out[i*2]=kf_sat16(out[i*2]+click)*(int32_t)rd_volume/100;
        out[i*2+1]=kf_sat16(out[i*2+1]+click)*(int32_t)rd_volume/100;
    }
    au_frames = kf_now;
    au_ms = fm1_ms;
#if defined(RD_SPC) && defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
    uint32_t cost=(fm1_ticks()-audio_start)/FM1_TICKS_PER_US;rd_audio_total+=cost;rd_audio_blocks++;
    if(cost>rd_audio_max)rd_audio_max=cost;if(cost>5805u)rd_audio_late++;
#endif
}
#define KF_LATENCY 512u                          /* frames between a rendered block and the speaker (two halves) */
static int32_t offset_ms;                        /* the player's own latency (the calibration) */
static uint32_t heard_now(void)                  /* the frame being heard now */
{
    uint32_t f, m, f2;
    do { f = au_frames; m = au_ms; f2 = au_frames; } while (f != f2);
    return f - KF_LATENCY + (fm1_ms - m) * 441u / 10u - (uint32_t)(offset_ms * 441 / 10);
}

/* ---- what survives a power-off: the settings and the best results */
#define NBEST 16u
static struct {
    uint8_t pad0, speed, tone, scroll, grit, spread, sel, pad;
    int16_t offset, pad2;
    uint16_t best_hash[NBEST];                   /* the song's hash | its level << 14 (the key as before D12: saves stay) */
    int16_t best_acc[NBEST];                     /* accuracy x 10 */
} set;
static void set_defaults(void)
{
    uint32_t i;
    set.pad0 = 80; set.pad = 0x54; set.pad2 = 30; set.speed = 100; set.tone = 0; set.scroll = 100; rd_scroll_speed=100; set.grit = 70; set.spread = 0; set.sel = CQ_SLOT; set.offset = 0;
    for (i = 0; i < NBEST; i++) { set.best_hash[i] = 0xFFFFu; set.best_acc[i] = -1; }
}
static uint32_t best_key(uint32_t song) { return KF_SONG[song].hash | (uint32_t)KF_SONG[song].level << 14; }
static int32_t best_of(uint32_t song)
{
    uint32_t i, key = best_key(song);
    for (i = 0; i < NBEST; i++) if (set.best_hash[i] == key) return set.best_acc[i];
    return -1;
}
static int32_t best_put(uint32_t song, int32_t acc)   /* 1: a new best */
{
    uint32_t i, key = best_key(song), freei = NBEST;
    for (i = 0; i < NBEST; i++) {
        if (set.best_hash[i] == key) { if (acc > set.best_acc[i]) { set.best_acc[i] = (int16_t)acc; return 1; } return 0; }
        if (set.best_hash[i] == 0xFFFFu && freei == NBEST) freei = i;
    }
    if (freei == NBEST) freei = song % NBEST;    /* full (songs that are gone): take a place */
    set.best_hash[freei] = (uint16_t)key;
    set.best_acc[freei] = (int16_t)acc;
    return 1;
}

/* ---- the player's voice: KNOB 2's TONE, or what channel 1 asks for (SONG) */
static uint32_t tone_patch(void) { return set.tone ? KF_PART_FIRST[KF_P_PLAY] + set.tone - 1u : kf_seq_play_patch(); }
static const char *tone_name(void) { return set.tone ? KF_PATCH[KF_PART_FIRST[KF_P_PLAY] + set.tone - 1u].name : "SONG"; }
static uint8_t key_note[27];                     /* the pitch each key sounds (for its release) */
static void kg_sound_on(uint32_t key, uint32_t pitch, int32_t detune, uint32_t bright)
{
    if (key >= 27u) return;
    if (key_note[key]) kf_note_off(0, KF_P_PLAY, key_note[key]);
    key_note[key] = (uint8_t)pitch;
    kf_note_on(0, KF_P_PLAY, tone_patch(), pitch, 104, detune, bright);
}
static void kg_sound_off(uint32_t key, uint32_t pitch)
{
    (void)pitch;
    if (key < 27u && key_note[key]) { kf_note_off(0, KF_P_PLAY, key_note[key]); key_note[key] = 0; }
}

/* ---- the screens and their state */
enum { SCR_SPLASH, SCR_SONGS, SCR_PLAY, SCR_RESULTS, SCR_MIDI, SCR_MAIN, SCR_SETTINGS, SCR_HELP };
static uint32_t scr, paused, paused_at, dirty_set, t_set, redraw = 1, demo, autoplay;
static uint32_t rd_menu_sel,rd_setting_sel,rd_setting_edit,rd_pause_sel,rd_result_sel;
static char toast_s[24];
static uint32_t toast_ms;
static void toast(const char *s) { kput(toast_s, s); toast_ms = fm1_ms | 1u; redraw = 1; }
static int32_t mm_oct;                           /* MIDI MODE: the keys' octave */
static const char *const PART_NAME7[7] = {"PLAY", "BASS", "PAD", "ARP", "LEAD", "KEYS", "DRUMS"};

/* MIDI in (D3, D6): in MIDI MODE a channel is a part; in a song channel 1's notes are FM-1 keys */
static const int8_t CH_PART[16] = {0, 1, 2, 3, 4, 5, -1, -1, -1, 6, -1, -1, -1, -1, -1, -1};
static uint8_t live_patch[7];
static uint32_t idle_ok, midi_seen, clk_n, clk_t0, clk_bpm_x100, ch_act[16];
static void kg_press(uint32_t key, uint32_t frame);
static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2)) {
        uint32_t ty = st & 0xF0u, ch = st & 15u, on = ty == 0x90u && d2, off = ty == 0x80u || (ty == 0x90u && !d2);
        int32_t part;
        if (st == 0xF8u) {                      /* clock: the delay's tempo in MIDI MODE, from a bar's worth */
            if (clk_n % 96u == 0u) {
                if (clk_n && fm1_ms != clk_t0) { clk_bpm_x100 = 24000000u / (fm1_ms - clk_t0); if (scr == SCR_MIDI) kf_set_tempo(clk_bpm_x100); }
                clk_t0 = fm1_ms;
            }
            clk_n++;
            continue;
        }
        if (st == 0xFAu) { clk_n = 0; continue; }
        if (st == 0xFCu) { if (scr == SCR_MIDI) { kf_ev_t e = {0, KF_EV_ALLOFF, 0, 0, 0, 0, 0, 0}; kf_post(&e); } continue; }
        if (!on && !off) continue;
        if (scr == SCR_PLAY && ch == 0u && !paused && !demo) {   /* a MIDI keyboard plays the game: notes as FM-1 keys */
            int32_t key = (int32_t)d1 - 53 - 12 * kg_oct_at(kg.pos);
            if (d1 < 12u) continue;
            kg.midi_used = 1;
            if (key >= 0 && key < 27) { if (on) kg_press((uint32_t)key, heard_now()); else kg_release((uint32_t)key, heard_now()); }
            continue;
        }
        if (scr != SCR_MIDI) continue;
        part = CH_PART[ch];
        if (part < 0) continue;
        midi_seen = fm1_ms | 1u;
        if (d1 < 12u) {                          /* a keyswitch (D6); on channel 1, note 11 is the OP note */
            if (on && d1 < KF_PART_COUNT[part] && !(ch == 0u && d1 == 11u)) live_patch[part] = d1;
            continue;
        }
        ch_act[ch] = fm1_ms | 1u;
        if (part == (int32_t)KF_P_DRUMS) { if (on) kf_drum(0, live_patch[part] < KF_NKIT ? live_patch[part] : 0u, d1, d2); continue; }
        if (on) kf_note_on(0, (uint32_t)part, KF_PART_FIRST[part] + live_patch[part], d1, d2, 0, 4096);
        else kf_note_off(0, (uint32_t)part, d1);
    }
}

/* the keys: their edges, read from the main loop and while the screen is sent, and stamped with the frame being
 * heard when the key moved: the scan (10 kHz, the timer ISR) tells when (app_note_edge), so a press is not late by
 * however long the main loop was away (a frame being drawn: ~7 ms). The state is the truth (fm1_in.notes); a board
 * whose scan does not stamp (SMK-37) gets the time of the reading, as before. A press and its release both between
 * two readings (a quick tap while the loop was away) still play: the scan counts the edges. */
static uint32_t keys_prev;
static volatile uint32_t key_ms[27][2];          /* each key's last press [0] and release [1]: fm1_ms at the scan */
static volatile uint8_t key_edges[27];           /* its edges so far (wrapping) */
static uint8_t key_seen[27];
static void app_note_edge(uint32_t key, uint32_t down)   /* the scan ISR */
{
    if (key < 27u) {
        key_ms[key][down ? 0 : 1] = fm1_ms;
        key_edges[key]++;
    }
}
static uint32_t key_at(uint32_t k, uint32_t which, uint32_t now, uint32_t now_ms)   /* the frame key k moved at */
{
    uint32_t age = now_ms - key_ms[k][which];
    return age > 100u ? now : now - age * 441u / 10u;   /* (no stamp, or an old one: now) */
}
static void keys_sync(void)                      /* a song starts: what was before does not count */
{
    uint32_t k;
    keys_prev = fm1_in.notes & 0x7FFFFFFu;
    for (k = 0; k < 27u; k++) key_seen[k] = key_edges[k];
}
static void keys_in(void)
{
    uint32_t keys = fm1_in.notes & 0x7FFFFFFu, down = keys & ~keys_prev, up = keys_prev & ~keys, tap = 0, k, now, now_ms;
    for (k = 0; k < 27u; k++) {
        uint8_t e = key_edges[k], d = (uint8_t)(e - key_seen[k]);
        key_seen[k] = e;
        if (d >= 2u && !((keys | keys_prev) >> k & 1u)) tap |= 1u << k;   /* down and up again, unseen */
    }
    if (!down && !up && !tap) return;
    keys_prev = keys;
    now = heard_now();
    now_ms = fm1_ms;
    for (k = 0; k < 27u; k++) {
        if (scr == SCR_PLAY) {
            if (paused || demo) continue;        /* paused: the keys rest; the DEMO plays itself */
            if (down >> k & 1u) kg_press(k, key_at(k, 0, now, now_ms));
            if (up >> k & 1u) kg_release(k, key_at(k, 1, now, now_ms));
            if (tap >> k & 1u) { kg_press(k, key_at(k, 0, now, now_ms)); kg_release(k, key_at(k, 1, now, now_ms)); }
        } else {                                 /* free play; in MIDI MODE they go out on channel 1 too */
            uint32_t pitch = (uint32_t)(53 + (int32_t)k + 12 * (scr == SCR_MIDI ? mm_oct : 0));
            if (down >> k & 1u) {
                kg_sound_on(k, pitch, 0, 4096);
                if (scr == SCR_MIDI) midi_send(0x90u, pitch, 100);
            }
            if (up >> k & 1u) {
                if (scr == SCR_MIDI && key_note[k]) midi_send(0x80u, key_note[k], 0);
                kg_sound_off(k, 0);
            }
        }
    }
}
static void app_idle(void)
{
    if (!idle_ok) return;
    keys_in();
    midi_in();
}

/* ---- a song */
static void voices_off(void)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) if (key_note[k]) kg_sound_off(k, 0);
}
static void song_start(uint32_t idx, uint32_t is_demo)
{
    if (!cq_ready || !cq_select_project(idx)) return;
#ifdef RD_SPC
    rd_spc_select(idx);
#endif
    voices_off();
    demo = is_demo;
    kg_start(idx, is_demo ? 100u : set.speed, is_demo ? 0u : set.spread, kf_now + 6615u);   /* beat 0 in 150 ms */
    kg.auto_on = (uint8_t)(is_demo || autoplay);
    if (is_demo) kg_seek(kf_bench_beat);
    keys_sync();                                 /* (a key held from before does not count) */
    paused = 0;
    scr = SCR_PLAY;
}
static void song_stop(void)
{
    kf_seq_stop();
    voices_off();
    paused = 0;
    demo = 0;
}
static void song_pause(void)
{
    if (!paused) {
        kf_ev_t e = {0, KF_EV_ALLOFF, 0, 0, 0, 0, 0, 0};
        voices_off();
        kf_sched_clear();
        kf_post(&e);
        paused = 1;
        paused_at = heard_now();
#ifdef RD_SPC
        rd_spc_pause(1);
#endif
    } else {                                     /* the song goes on where it stopped: the band from there */
        const kf_song_t *s = kg.song;
        kf_seq.t0 += heard_now() - paused_at;
        kf_seq.bi = 0;
        while (kf_seq.bi < s->nband && (int32_t)s->band[kf_seq.bi].t * 256 < kg.pos) {
            const kf_bev_t *e = &s->band[kf_seq.bi++];
            if (e->ks) kf_seq.patch[e->part] = (uint8_t)(e->ks - 1u);
        }
        paused = 0;
#ifdef RD_SPC
        rd_spc_pause(0);
#endif
    }
}

/* ---- the LEDs, as the reference's ledStates: a key lit when its note is at the line (or held), glowing when it
 * comes within 2 beats; OCT- OCT+ for a shift; PLAY on the beat; FX in a boost, blinking when the OPs are full */
static void leds(void)
{
    uint8_t L[27] = {0};
    uint32_t k, i, pending = 0;
    int32_t oct_m = 0, oct_p = 0, play = 0, fx = 0;
    if (scr == SCR_PLAY && !paused) {
        if (!kg.auto_on) {
            for (i = kg.first; i < kg.n; i++) {
                const kf_cnote_t *n = &kg.notes[i];
                if (kg.nst[i] == KN_PENDING) {
                    uint32_t key = kg.song->transpose == 127 ? n->lane : KG_LAYOUT[kg.spread ? 1 : 0][n->lane];
                    if (key < 27u) {
                        if (!pending) L[key] = 2;
                        /* Music-position phase: bright for the first half of each beat. */
                        else if (L[key] != 2 && kg.pos >= 0 && kg.pos % KG_BEAT < KG_BEAT / 2) L[key] = 1;
                    }
                    if (++pending == 2) break;
                }
            }
        }
        play = kg.pos >= 0 && (kg.pos / (KG_BEAT / 4)) % 4 == 0 ? 2 : 0;
    }
    for (k = 0; k < 27u; k++) { fm1_led_key(KEY_LED(k), L[k] == 2u); fm1_led_key_glow(KEY_LED(k), L[k] == 1u); }
    fm1_led_key(BTN_OCT_DOWN, oct_m == 2); fm1_led_key_glow(BTN_OCT_DOWN, oct_m == 1);
    fm1_led_key(BTN_OCT_UP, oct_p == 2); fm1_led_key_glow(BTN_OCT_UP, oct_p == 1);
    fm1_led_key(BTN_PLAY, play == 2 || (scr == SCR_SPLASH && (fm1_ms / 500u) % 2u == 0u));
    fm1_led_key(BTN_FX, fx == 2);
    fm1_led_key(BTN_GLO, set.spread != 0u);
    fm1_led_key(BTN_SEQ, autoplay != 0u);           /* AUTO (as the reference's panel) */
}

#include "rd_screens.h"

/* ---- MIDI MODE LIVE's screen: the channels, their patches, the clock */
static void draw_midi_mode(void)
{
    static const uint8_t CHS[7] = {0, 1, 2, 3, 4, 5, 9};
    static const char *const KM[4] = {"MODE", "TONE", "BPM", "MIX"};
    char s[32], *p;
    uint32_t i;
    ks_night(fm1_ms);
    ks_header(0, "MIDI MODE", "LIVE", -1, K_CYAN);
    if (clk_bpm_x100 && fm1_ms - clk_t0 < 3000u) { p = knum(s, (clk_bpm_x100 + 50u) / 100u, 0); kput(p, " BPM"); ktext(&FONT_S, 4, 21, s, K_GREEN); }
    else ktext(&FONT_S, 4, 21, "NO CLOCK", K_DIM);
    ktextR(&FONT_S, 236, 21, midi_seen ? "MIDI IN" : "DAW > FM-1", K_DIM);
    for (i = 0; i < 7u; i++) {
        uint32_t ch = CHS[i], part = (uint32_t)CH_PART[ch], act = ch_act[ch] && fm1_ms - ch_act[ch] < 150u;
        int32_t y = 44 + (int32_t)i * 22;
        const char *pn = part == KF_P_DRUMS ? KF_KIT[live_patch[part] < KF_NKIT ? live_patch[part] : 0u].name :
                         part == KF_P_PLAY ? tone_name() : KF_PATCH[KF_PART_FIRST[part] + live_patch[part]].name;
        p = kput(s, "CH "); knum(p, ch + 1u, 0);
        krect(2, y - 2, 236, 20, act ? K_SURF2 : K_SURF);
        krect(2, y - 2, 3, 20, act ? (i ? K_VIOLET : K_CYAN) : K_LINE);
        ktext(&FONT_S, 9, y, s, i ? K_VIOLET : K_CYAN);
        ktext(&FONT_S, 60, y, PART_NAME7[part], K_TEXT);
        ktextR(&FONT_S, 234, y, pn, act ? K_TEXT : K_DIM);
    }
    ktextC(&FONT_S, 120, 200, "KEYS OUT ON CH 1", K_MUTE);
    ks_knobs(KM, 5u);
}

/* the screen goes out over SPI at 12 MHz (lcd.c, LCD_BAUD 4): a whole frame is 921 600 bits, 77 ms. So only what
 * changed since the last frame goes out. Each row is 8 blocks of 30 px with a hash each; a row's changed blocks make
 * its span, rows with the same span one rectangle, and a rectangle narrower than the screen is copied out first (two
 * staging buffers: one is filled while the other goes out). In the play about a third of the rows change a frame,
 * and of them about a third of the width. A band of SEND_HEAL rows goes out whole besides, walking round the screen,
 * so whatever else the screen may show (the update screen's text, a hash that matched by chance) is gone within a few
 * seconds. Keys and MIDI are read while each part goes out (LCD_IDLE) */
#define SEND_HEAL 4u
#define SEND_BW 30u                              /* a block: 30 px, 8 a row */
#define SEND_NB (240u / SEND_BW)
#define SEND_STAGE (240u * 24u)                  /* px a staging buffer */
static uint32_t blk_hash[240][SEND_NB], heal_y;
static uint16_t send_stage[2][SEND_STAGE] __attribute__((section(".pool")));
static void send_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    static uint32_t which;
    if (w == 240u) {                             /* whole rows: straight from the canvas */
        lcd_blit(0, y, 240u, h, cv_px + y * 240u);
        return;
    }
    while (h) {
        uint32_t n = SEND_STAGE / w, r, i;
        uint16_t *s = send_stage[which];         /* (the other one may still be going out) */
        if (n > h) n = h;
        for (r = 0; r < n; r++) {
            const uint16_t *src = cv_px + (y + r) * 240u + x;
            for (i = 0; i < w; i++) s[r * w + i] = src[i];
        }
        lcd_blit(x, y, w, n, s);
        which ^= 1u;
        y += n;
        h -= n;
    }
}
static void send_changed(void)
{
    uint32_t y, b, y0 = 0, lo0 = SEND_NB, hi0 = 0;
    for (y = 0; y <= 240u; y++) {
        uint32_t lo = SEND_NB, hi = 0;
        if (y < 240u) {
            uint32_t heal = (y + 240u - heal_y) % 240u < SEND_HEAL;
            const uint32_t *r=(const uint32_t *)(cv_px+y*240u);
            for (b = 0; b < SEND_NB; b++) {
                uint32_t h = 2166136261u, i;
                for (i = 0; i < SEND_BW/2u; i++) h = (h ^ r[b*(SEND_BW/2u)+i]) * 16777619u;   /* (FNV-1a) */
                if (h != blk_hash[y][b] || heal) { if (lo == SEND_NB) lo = b; hi = b; }
                blk_hash[y][b] = h;
            }
        }
        if (lo != lo0 || hi != hi0) {            /* the span changes: the rectangle above ends */
            if (lo0 != SEND_NB) send_rect(lo0 * SEND_BW, y0, (hi0 - lo0 + 1u) * SEND_BW, y - y0);
            lo0 = lo; hi0 = hi; y0 = y;
        }
    }
    heal_y = (heal_y + SEND_HEAL) % 240u;
}

/* the screen is behind the sound: a frame is drawn, then goes out over SPI, the hit line's rows among the last of it.
 * So the play is drawn where the song will be when the frame reaches the panel, by vis_lead_us: how long the last
 * frames took from the start of the drawing until their last rows were handed to the DMA (smoothed, 40 ms at most).
 * The judgement does not use it: a press is judged by the sound, by the frame being heard when the key moved (the
 * player's own latency, the calibration, moves both). On the host a frame takes no time, so no lead */
#ifdef DINGHY_HOST
static uint32_t vis_lead_us;
#else
static uint32_t vis_lead_us = 8000u;
#endif
#if defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
static uint32_t rd_prof_frames,rd_prof_total,rd_prof_max,rd_prof_gap,rd_prof_last,rd_prof_paint,rd_prof_send;
#endif
static void draw(void)
{
#if defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
    uint32_t profile_start=fm1_ticks(),profile_gap=(profile_start-rd_prof_last)/FM1_TICKS_PER_US;
    if(rd_prof_last&&profile_gap>rd_prof_gap)rd_prof_gap=profile_gap;
    rd_prof_last=profile_start;
#endif
    uint32_t i;
    char num[4];
#ifndef DINGHY_HOST
    uint32_t t_draw = 0;
#endif
    idle_ok = 1;                                 /* (cv_begin waits for the last frame's DMA: keys and MIDI meanwhile) */
    if (scr == SCR_PLAY) {                       /* the play draws every row itself (ks_play): no clear first */
        lcd_sync();
        cv_w = 240u;
        cv_h = 240u;
    } else
        cv_begin(240, 240, c565(K_BG));
    idle_ok = 0;
    knum(num, (scr == SCR_PLAY || scr == SCR_RESULTS ? kg.idx : set.sel) + 1u, 2);
    if (!cq_ready) rd_project_error();
    else if (scr == SCR_SPLASH) rd_title();
    else if (scr == SCR_MAIN) rd_main_menu();
    else if (scr == SCR_SETTINGS) rd_settings();
    else if (scr == SCR_HELP) rd_help();
    else if (scr == SCR_SONGS) {
        rd_song_select();
    } else if (scr == SCR_PLAY) {
        uint8_t lit[27] = {0}, led[27] = {0};
#ifndef DINGHY_HOST
        t_draw = fm1_ticks();
#endif
        kg.vpos = paused || kg.done ? kg.pos : kg_pos_at(heard_now() + vis_lead_us * 441u / 10000u);
        for (i = 0; i < (kg.song->transpose == 127 ? 27u : 8u); i++) {
            uint32_t key = kg.song->transpose == 127 ? i : KG_LAYOUT[kg.spread ? 1 : 0][i];
            lit[i] = (uint8_t)(key_note[key] != 0u);
            led[i] = (uint8_t)(lit[i] ? 2u : 0u);
        }
        cq_play(fm1_ms, lit, led, num, demo ? "DEMO" : kg.auto_on ? "AUTO" : 0);
        if (paused) rd_pause_menu();
    } else if (scr == SCR_RESULTS) rd_results();
    else draw_midi_mode();
    if (toast_ms && fm1_ms - toast_ms < 900u){krect(40,87,160,30,RD_BG);rd_center(2,120,94,toast_s,RD_YELLOW);}
    else toast_ms = 0;
    idle_ok = 1;
#if defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
    uint32_t send_start=fm1_ticks();rd_prof_paint+=(send_start-profile_start)/FM1_TICKS_PER_US;
#endif
    send_changed();                              /* (the last run's DMA goes on while the loop does) */
    idle_ok = 0;
#ifndef DINGHY_HOST
    if (scr == SCR_PLAY) {                       /* the lead, for the next frames */
        uint32_t d = (fm1_ticks() - t_draw)/FM1_TICKS_PER_US;
        vis_lead_us = (vis_lead_us * 7u + (d > 40000u ? 40000u : d)) / 8u;
    }
#endif
#if defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
    {uint32_t end=fm1_ticks(),cost=(end-profile_start)/FM1_TICKS_PER_US;rd_prof_send+=(end-send_start)/FM1_TICKS_PER_US;rd_prof_total+=cost;rd_prof_frames++;if(cost>rd_prof_max)rd_prof_max=cost;}
#endif
}

static int32_t enc_step(uint32_t enc, int32_t v, int32_t by, int32_t lo, int32_t hi, uint32_t *changed)
{
    int32_t e = fm1_enc_take(enc);
    if (!e) return v;
    v += e * by;
    v = v < lo ? lo : v > hi ? hi : v;
    *changed = 1;
    return v;
}
static void toast_num(const char *label, uint32_t v, const char *unit)
{
    char s[24], *p = kput(s, label);
    p = knum(p, v, 0);
    kput(p, unit);
    toast(s);
}

static uint32_t rd_wrap(uint32_t old,int32_t step,uint32_t count){int32_t n=(int32_t)old+step%(int32_t)count;return(uint32_t)((n+(int32_t)count)%(int32_t)count);}
static void rd_controls(uint32_t b,uint32_t *changed){
 int32_t e=fm1_enc_take(ENC_SELECT),v;
 int32_t scroll_step=fm1_enc_take(ENC_KNOB1);
 if(scroll_step&&(scr==SCR_PLAY||scr==SCR_SONGS)){
  int32_t next=(int32_t)set.scroll+scroll_step*10;
  if(next<50)next=50;if(next>200)next=200;
  if(next!=set.scroll){set.scroll=(uint8_t)next;rd_scroll_speed=(uint32_t)next;*changed=1;toast_num("SCROLL ",(uint32_t)next,"%");}
 }
 if(scr==SCR_SPLASH){if(b&(1u<<BTN_PLAY))scr=SCR_MAIN;}
 else if(scr==SCR_MAIN){
  rd_menu_sel=rd_wrap(rd_menu_sel,e,3);
  if(b&(1u<<BTN_HOME))scr=SCR_SPLASH;
  else if(b&(1u<<BTN_PLAY)){scr=rd_menu_sel==0?SCR_SONGS:rd_menu_sel==1?SCR_SETTINGS:SCR_HELP;rd_setting_edit=0;}
 }else if(scr==SCR_SETTINGS){
  if(b&(1u<<BTN_HOME)){scr=SCR_MAIN;rd_setting_edit=0;}
  else if(b&(1u<<BTN_PLAY))rd_setting_edit^=1;
  if(rd_setting_edit){
   int32_t old=rd_setting_sel==0?set.pad0:rd_setting_sel==1?set.speed:rd_setting_sel==2?set.offset:set.pad2;
   v=old+e*5;int32_t lo=rd_setting_sel==0?0:rd_setting_sel==1?50:rd_setting_sel==2?-200:0,hi=rd_setting_sel==2?200:100;
   if(v<lo)v=lo;
   if(v>hi)v=hi;
   if(v!=old){if(rd_setting_sel==0){set.pad0=(uint8_t)v;rd_volume=(uint32_t)v;}else if(rd_setting_sel==1)set.speed=(uint8_t)v;else if(rd_setting_sel==2){set.offset=(int16_t)v;offset_ms=v;}else{set.pad2=(int16_t)v;kf_set_game_balance((uint32_t)v);}*changed=1;}
  }else rd_setting_sel=rd_wrap(rd_setting_sel,e,4);
 }else if(scr==SCR_HELP){if(b&((1u<<BTN_PLAY)|(1u<<BTN_HOME)))scr=SCR_MAIN;}
 else if(scr==SCR_SONGS){
  if(e&&KF_NSONG>1){uint32_t choice=(uint32_t)rd_wrap(CQ_SLOT,e,KF_NSONG);if(cq_select_project(choice)){set.sel=(uint8_t)choice;*changed=1;}}
  if(b&(1u<<BTN_HOME))scr=SCR_MAIN;
  else if(b&(1u<<BTN_PLAY))song_start(CQ_SLOT,0);
  if(b&(1u<<BTN_SEQ))autoplay^=1u;
 }else if(scr==SCR_PLAY){
  if(b&(1u<<BTN_HOME)){song_stop();scr=SCR_SONGS;}
  else if(paused){rd_pause_sel=rd_wrap(rd_pause_sel,e,3);if(b&(1u<<BTN_PLAY)){if(rd_pause_sel==0)song_pause();else if(rd_pause_sel==1){song_stop();song_start(CQ_SLOT,0);}else{song_stop();scr=SCR_SONGS;}}}
  else if(b&(1u<<BTN_PLAY)&&!kg.done){song_pause();rd_pause_sel=0;}
 }else if(scr==SCR_RESULTS){
  rd_result_sel=rd_wrap(rd_result_sel,e,2);
  if(b&(1u<<BTN_HOME))scr=SCR_SONGS;
  else if(b&(1u<<BTN_PLAY)){if(rd_result_sel==0)song_start(CQ_SLOT,0);else scr=SCR_SONGS;}
 }
 if(e||b)redraw=1;
}

static uint32_t rd_now_us(void){
 /* TIMER4/24 jumps back to zero every 178.9 s. fm1_ms already accumulates
  * unsigned raw-tick differences; multiplying it has the ordinary uint32
  * wrap semantics required by the display deadline comparisons. */
 return fm1_ms*1000u;
}
static void app_main(void)
{
    uint32_t last_draw=0,next_play_draw=0;
    if (settings_load(&set, sizeof set) || set.speed < 50u || set.speed > 100u || set.tone > 5u ||
        set.offset < -200 || set.offset > 200 || set.grit > 100u)
        set_defaults();
    if((set.pad!=0x52&&set.pad!=0x53&&set.pad!=0x54)||set.pad0>100){set.pad0=80;dirty_set=1;t_set=fm1_ms;}
    if((set.pad!=0x53&&set.pad!=0x54)||set.pad2<0||set.pad2>100){set.pad2=30;dirty_set=1;t_set=fm1_ms;}
    if(set.pad!=0x54||set.scroll<50||set.scroll>200){set.scroll=100;set.pad=0x54;dirty_set=1;t_set=fm1_ms;}
    rd_scroll_speed=set.scroll;
    rd_volume=set.pad0;
    cq_init_project();
    if(set.sel>=KF_NSONG)set.sel=0;
    cq_select_project(set.sel);
    offset_ms = set.offset;
    kf_init();
    kf_set_grit(set.grit);
    kf_set_game_balance((uint32_t)set.pad2);
    kh_init();
    scr = SCR_SPLASH;

    for (;;) {
        uint32_t b, rel, now_ms, ch = 0;
        platform_poll();
        keys_in();
        midi_in();
        now_ms = fm1_ms;

#if defined(RD_PROFILE_BOOT) && !defined(DINGHY_HOST)
        {
            static uint32_t report_at;
            if(now_ms>=1500u&&scr==SCR_SPLASH){
#ifdef RD_PROFILE_MANUAL
                autoplay=0;
#else
                autoplay=1;
#endif
                song_start(CQ_SLOT,0);
#ifdef RD_PROFILE_START_BEAT
                kg_seek(RD_PROFILE_START_BEAT);
#endif
                redraw=1;}
            if(now_ms-report_at>=1000u&&rd_prof_frames){
                uint32_t dt=now_ms-report_at,values[7]={rd_prof_frames*1000u/dt,rd_prof_total/rd_prof_frames/1000u,rd_prof_max/1000u,rd_prof_gap/1000u,rd_prof_paint/rd_prof_frames/1000u,rd_prof_send/rd_prof_frames/1000u,fm1_lcd_timeouts};
                for(uint32_t q=0;q<7;q++)midi_send(0xbfu,110u+q,values[q]>127u?127u:values[q]);
#ifdef RD_SPC
                uint32_t av[10]={rd_audio_blocks?rd_audio_total/rd_audio_blocks/100u:0,rd_audio_max/100u,rd_audio_late,rd_spc_status(0),rd_spc_status(2)%128u,rd_spc_status(3)%128u,rd_spc_status(4)%128u,rd_spc_status(1)/1024000u%128u,uac.underruns%128u,uac.missed%128u};
                for(uint32_t q=0;q<10;q++)midi_send(0xbfu,117u+q,av[q]>127u?127u:av[q]);
                uint32_t hash=rd_spc_status(6),sec=rd_spc_status(7),clock=rd_spc_status(1);
                uint8_t proof[33]={0xf0,0x7d,0x52,0x44,(uint8_t)(sec&127u),(uint8_t)(sec>>7)};
                for(uint32_t q=0;q<5;q++)proof[6+q]=(uint8_t)(hash>>(q*7u)&127u);
                for(uint32_t q=0;q<5;q++)proof[11+q]=(uint8_t)(clock>>(q*7u)&127u);
                for(uint32_t q=0;q<4;q++){proof[16+q*2]=(uint8_t)(kg.cnt[q]&127);proof[17+q*2]=(uint8_t)(kg.cnt[q]>>7);}
                proof[24]=(uint8_t)(kg.max_combo&127);proof[25]=(uint8_t)(kg.max_combo>>7);proof[26]=kg.auto_on;
                uint32_t timer_ticks=fm1_ticks();for(uint32_t q=0;q<5;q++)proof[27+q]=(uint8_t)(timer_ticks>>(q*7u)&127u);proof[32]=0xf7;
                ota_wire_send(proof,sizeof(proof));
                rd_audio_total=rd_audio_max=rd_audio_blocks=rd_audio_late=0;
#endif
                rd_prof_frames=rd_prof_total=rd_prof_max=rd_prof_gap=rd_prof_paint=rd_prof_send=0;report_at=now_ms;
            }
        }
#endif
        b = fm1_input_edges(&rel);
        rd_controls(b,&ch);
        if(scr==SCR_PLAY&&!paused){
            kf_seq_post(kf_now+4096u);
            if(kg_update(heard_now())){
#ifdef RD_PROFILE_BOOT
                kg.new_best=0;ch=1; /* Automated hardware validation never writes a personal best. */
#else
                kg.new_best=kg.auto_on?0:best_put(kg.idx,kg.acc10);ch=1;
#endif
                song_stop();scr=SCR_RESULTS;rd_result_sel=0;redraw=1;
#ifdef RD_PROFILE_SOAK
                song_start(CQ_SLOT,0); /* Test only: keep rendering across the 179 s hardware-timer wrap. */
#endif
            }
        }
        if (ch) { dirty_set = 1; t_set = now_ms; }

        leds();
        if (redraw || (scr == SCR_PLAY && (int32_t)(rd_now_us()-next_play_draw)>=0) ||
            (scr != SCR_PLAY && now_ms - last_draw >= 50u)) {
            kg_ms = now_ms;                      /* the play at up to 60 fps, the rest at 30 when it moves */
            uint32_t frame_start=rd_now_us();
            draw();
            if(scr==SCR_PLAY){if((int32_t)(frame_start-next_play_draw)>16667)next_play_draw=frame_start;next_play_draw+=16667u;}
            redraw = 0;
            last_draw = now_ms;
        }

        if (dirty_set && scr != SCR_PLAY && now_ms - t_set > 2000u) {   /* when silent: an erase stalls the sound */
            uint32_t k, quiet = 1;
            for (k = 0; k < KF_NV; k++) if (kf_v[k].state) quiet = 0;
            if (quiet) { settings_save(&set, sizeof set); dirty_set = 0; }
        }
    }
}
