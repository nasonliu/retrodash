/* SPDX-License-Identifier: GPL-3.0-only: universal RetroDash pages, no song actors outside battle. */
#pragma once
static void rd_title(void){
 rd_background();rd_asset(RD_LOGO,RD_LOGO_W,RD_LOGO_H,12,65,216,52);
 rd_center(1,120,126,"BEAT SIGNAL",RD_PINK);rd_button(23,183,194,30,"PRESS PLAY",1);rd_footer("RETRODASH  /  FM-1");
}
static void rd_main_menu(void){
 rd_background();rd_header("RETRODASH");
 rd_button(52,78,136,60,"",rd_menu_sel==0);rd_center(5,120,88,"PLAY",rd_menu_sel==0?RD_BG:RD_TEXT);
 rd_button(20,164,200,26,"SETTINGS",rd_menu_sel==1);rd_button(20,195,200,26,"HOW TO PLAY",rd_menu_sel==2);
 rd_footer("SELECT MOVE  /  PLAY OK  /  HOME BACK");
}
static void rd_song_select(void){
 const uint8_t *im=cq_image(cq_get(144));char s[24],*p;rd_background();rd_header("SONG SELECT");p=knum(s,CQ_SLOT+1u,2);kput(p," / ");p+=3;knum(p,KF_NSONG,2);rd_right(0,231,13,s,RD_DIM);
 krect(60,34,120,120,RD_YELLOW);cq_blit_scaled(cq_get(144),64,38,0,0,(int)cq_u16(im),(int)cq_u16(im+2),112,112);
 rd_center(3,120,159,KF_SONG[CQ_SLOT].title,RD_TEXT);p=knum(s,KF_SONG[CQ_SLOT].bpm_x100/100u,0);kput(p," BPM  /  MELODY");rd_center(1,120,182,s,RD_TEXT);
 rd_button(47,197,146,25,"START",1);rd_footer(autoplay?"SELECT SONG / PLAY START / AUTO ON":"SELECT SONG / PLAY START / SEQ AUTO");
}
static void rd_settings(void){
 static const char *const label[4]={"MUSIC","SPEED","OFFSET","BACKING"};char s[24];int i;rd_background();rd_header("SETTINGS");
 for(i=0;i<4;i++){int y=41+i*37,sel=rd_setting_sel==i;rd_button(17,y,206,34,"",sel);cq_ui_text(2,28,y+11,label[i],sel?RD_BG:RD_TEXT);rd_value(s,i==0?set.pad0:i==1?set.speed:i==2?set.offset:set.pad2,i==2?" ms":"%");rd_right(2,210,y+11,s,sel?RD_BG:RD_TEXT);}
 rd_center(0,120,196,rd_setting_edit?"TURN SELECT TO ADJUST":"PLAY TO EDIT SELECTED VALUE",RD_TEXT);
 rd_footer(rd_setting_edit?"SELECT ADJUST  /  PLAY DONE":"SELECT MOVE  /  PLAY EDIT  /  HOME BACK");
}
static void rd_help(void){
 rd_background();rd_header("HOW TO PLAY");
 cq_ui_text(3,16,47,"FOLLOW THE MELODY",RD_YELLOW);
 cq_ui_text(1,16,79,"NEXT: BRIGHT, STEADY KEY",RD_TEXT);cq_ui_text(1,16,99,"THEN: DIM, PULSES ON BEAT",RD_TEXT);
 cq_ui_text(1,16,124,"HIT WHEN ENEMY REACHES LINE",RD_TEXT);cq_ui_text(1,16,144,"KNOB1: ENEMY SCROLL SPEED",RD_TEXT);
 cq_ui_text(1,16,171,"PLAY: PAUSE  /  HOME: SONGS",RD_MINT);cq_ui_text(1,16,191,"SEQ ON SONG PAGE: AUTO DEMO",RD_MINT);
 rd_footer("PLAY OR HOME TO RETURN");
}
static void rd_pause_menu(void){
 static const char *const labels[3]={"RESUME","RETRY","SONGS"};int i;rd_background();rd_header("PAUSED");
 for(i=0;i<3;i++)rd_button(36,56+i*44,168,33,labels[i],rd_pause_sel==i);
 rd_footer("SELECT MOVE  /  PLAY OK  /  HOME SONGS");
}
static void rd_results(void){
 char s[24],g[2]={(char)kg.grade,0},*p;int i;static const char *const labels[4]={"PERFECT","GREAT","GOOD","MISS"};kfill_rows(0,240,RD_BG);rd_header("RESULTS");
 rd_asset(RD_RING,RD_RING_W,RD_RING_H,9,52,86,86);rd_grade(52,64,g);
 p=knum(s,(uint32_t)kg.acc10/10u,0);*p++='.';p=knum(p,(uint32_t)kg.acc10%10u,0);kput(p,"%");rd_right(5,231,44,s,RD_YELLOW);rd_right(0,230,76,kg.auto_on?"AUTO / NO RECORD":"ACCURACY",RD_TEXT);
 cq_ui_text(1,106,98,"MAX COMBO",RD_TEXT);knum(s,(uint32_t)kg.max_combo,0);rd_right(2,231,96,s,RD_YELLOW);
 for(i=0;i<4;i++){cq_ui_text(1,106,116+i*17,labels[i],i==3?RD_PINK:RD_TEXT);knum(s,(uint32_t)kg.cnt[i],0);rd_right(1,230,116+i*17,s,RD_TEXT);}
 if(kg.full)rd_center(1,51,145,"FULL COMBO",RD_MINT);
 if(kg.new_best)rd_center(1,51,162,"NEW BEST",RD_YELLOW);
 rd_button(8,192,108,28,"RETRY",rd_result_sel==0);rd_button(124,192,108,28,"SONGS",rd_result_sel==1);rd_footer("SELECT MOVE  /  PLAY OK  /  HOME SONGS");
}
static void rd_project_error(void){rd_background();rd_header("PROJECT ERROR");rd_center(4,120,74,"CANNOT LOAD SONG",RD_PINK);rd_center(1,120,119,"REINSTALL A VALID FIRMWARE",RD_TEXT);rd_center(1,120,143,"WITH A COMPILED SONG PROJECT",RD_TEXT);rd_footer("USB MIDI UPDATE AVAILABLE");}
