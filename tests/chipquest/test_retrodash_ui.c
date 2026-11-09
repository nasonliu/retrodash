#define DINGHY_HOST
#define main rd_host_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
static uint32_t changes;
static void press(unsigned id){fm1_ms+=20;rd_controls(1u<<id,&changes);}
static void turn(int steps){host_enc[ENC_SELECT]+=steps;fm1_ms+=20;rd_controls(0,&changes);}
static void shot(const char *name){char p[256];redraw=1;draw();snprintf(p,sizeof p,"build/rd-captures/%s.ppm",name);write_ppm_as(p);}
static uint32_t surface_hash(void){uint32_t h=2166136261u;for(unsigned i=0;i<240*240;i++)h=(h^fb[i])*16777619u;return h;}
int main(void){
 cq_init_project();assert(cq_ready);set_defaults();kf_init();kh_init();scr=SCR_SPLASH;shot("title");uint32_t title_hash=surface_hash();
 /* Generic screens do not depend on cq_data or song-specific palettes/actors. */
 const uint8_t *project=cq_data;cq_data=NULL;shot("title-independent");assert(surface_hash()==title_hash);cq_data=project;
 press(BTN_PLAY);assert(scr==SCR_MAIN);shot("main");turn(1);press(BTN_PLAY);assert(scr==SCR_SETTINGS);shot("settings");
 press(BTN_PLAY);assert(rd_setting_edit);turn(100);assert(set.pad0==100);turn(-100);assert(!set.pad0);turn(12);assert(set.pad0==60&&rd_volume==60);press(BTN_PLAY);
 turn(1);press(BTN_PLAY);turn(-100);assert(set.speed==50);turn(100);assert(set.speed==100);press(BTN_PLAY);
 turn(1);press(BTN_PLAY);turn(-100);assert(set.offset==-200&&offset_ms==-200);turn(100);assert(set.offset==200);turn(-40);assert(!offset_ms);press(BTN_PLAY);
 assert(changes);settings_save(&set,sizeof set);set_defaults();assert(!settings_load(&set,sizeof set));assert(set.pad0==60&&set.speed==100&&set.offset==0);
 press(BTN_HOME);assert(scr==SCR_MAIN);turn(1);press(BTN_PLAY);assert(scr==SCR_HELP);shot("help");press(BTN_PLAY);assert(scr==SCR_MAIN);turn(-2);press(BTN_PLAY);assert(scr==SCR_SONGS);shot("songs");
 press(BTN_SEQ);assert(autoplay);press(BTN_SEQ);assert(!autoplay);press(BTN_PLAY);assert(scr==SCR_PLAY&&!paused&&!kg.auto_on);shot("countdown");
 kg.pos=kg.vpos=(int32_t)kg.notes[0].t*256;au_frames=kg_note_frame(0)+KF_LATENCY;au_ms=fm1_ms;
 kg_ms=fm1_ms;kg_press(kg.notes[0].lane,kg_note_frame(0));assert(kg.judge==KJ_PERFECT);fm1_ms+=60;au_ms=fm1_ms;kg.combo=20;shot("perfect");
 kg.judge=KJ_GOOD;kg.judge_ms=fm1_ms-70;shot("good");kg.judge=KJ_MISS;shot("miss");
 press(BTN_PLAY);assert(paused);shot("pause");press(BTN_PLAY);assert(!paused);
 press(BTN_PLAY);turn(1);press(BTN_PLAY);assert(scr==SCR_PLAY&&!paused&&kg.combo==0);
 press(BTN_HOME);assert(scr==SCR_SONGS);press(BTN_PLAY);press(BTN_PLAY);turn(2);press(BTN_PLAY);assert(scr==SCR_SONGS&&!paused);
 /* Real judgement totals, not fabricated persistent records. */
 press(BTN_PLAY);kg.cnt[0]=30;kg.cnt[1]=1;kg.cnt[2]=1;kg.cnt[3]=0;kg.max_combo=32;kg_finish();song_stop();scr=SCR_RESULTS;rd_result_sel=0;shot("results");turn(1);press(BTN_PLAY);assert(scr==SCR_SONGS);press(BTN_PLAY);kg_finish();song_stop();scr=SCR_RESULTS;rd_result_sel=0;press(BTN_PLAY);assert(scr==SCR_PLAY);press(BTN_HOME);
 cq_ready=0;shot("error");cq_ready=1;
 puts("RetroDash: all pages, independent global art, navigation, settings bounds/save, AUTO, pause/resume/retry/quit, result choices passed");return 0;
}
