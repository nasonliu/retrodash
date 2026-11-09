#define DINGHY_HOST
#ifndef RD_SPC
#define RD_SPC
#endif
#define main rd_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
#ifndef EXPECTED_TARGETS
#define EXPECTED_TARGETS 200
#define EXPECTED_FILLS 2
#define TEST_SECONDS 155
#endif
int main(void){
 cq_init_project();assert(cq_ready);kf_init();kh_init();set_defaults();
 scr=SCR_PLAY;kg_start(CQ_SLOT,100,0,4410);assert(kg.n==EXPECTED_TARGETS&&kg.song->nband==0);
 uint32_t changed=0,lead=cq_approach_pos(),fpt=kf_seq.fpt,note_frame=kg_note_frame(0);
 host_enc[ENC_KNOB1]=5;rd_controls(0,&changed);assert(changed&&set.scroll==150&&rd_scroll_speed==150);
 assert(cq_approach_pos()<lead&&kf_seq.fpt==fpt&&kg_note_frame(0)==note_frame&&rd_spc_status(1)==0);
 host_enc[ENC_KNOB1]=-99;rd_controls(0,&changed);assert(set.scroll==50&&abs(cq_approach_pos()-(int32_t)lead*2)<=1);
 host_enc[ENC_KNOB1]=99;rd_controls(0,&changed);assert(set.scroll==200&&cq_approach_pos()==lead/2);
 host_enc[ENC_KNOB1]=-10;rd_controls(0,&changed);assert(set.scroll==100&&cq_approach_pos()==lead);
 unsigned next=0,lit=0,dim=0;
 for(unsigned f=0;f<TEST_SECONDS*44100u;f+=256){
  fm1_ms=f*10u/441u;kg_ms=fm1_ms;
  while(next<kg.n&&kg_note_frame(next)<f+256){
   unsigned t=kg_note_frame(next),key=kg.notes[next].lane;
   kg_press(key,t);kg_release(key,t+1);next++;
  }
  int32_t out[512]={0};app_audio(out,256);kg_update(kf_now);
  leds();unsigned bright=(unsigned)(host_leds>>14),glow=(unsigned)(host_glow>>14);
  assert(!bright||!(bright&(bright-1)));assert(!glow||!(glow&(glow-1)));assert(!(bright&glow));
  lit|=bright;dim|=glow;
 }
 assert(lit&&dim);assert(kg.done&&kg.acc10==1000&&kg.max_combo==EXPECTED_TARGETS&&kg.cnt[3]==0);
 assert(rd_spc_status(0)==0&&rd_spc_status(2)==EXPECTED_TARGETS&&rd_spc_status(3)==EXPECTED_TARGETS&&rd_spc_status(4)==EXPECTED_FILLS);
 uint32_t before=rd_spc_status(1);rd_spc_pause(1);int32_t silent[512]={0};app_audio(silent,256);
 assert(rd_spc_status(1)==before);for(unsigned i=0;i<512;i++)assert(!silent[i]);
 rd_spc_start(kf_now,100);app_audio(silent,256);assert(rd_spc_status(1)<1024000);
 printf("SPC: %u manual hits, %u suppressed automatic triggers, %u fills; full combo, NEXT/THEN, pause/restart, KNOB1 50-200%% changes visual lead without changing audio or judgment timing\n",EXPECTED_TARGETS,EXPECTED_TARGETS,EXPECTED_FILLS);
}
