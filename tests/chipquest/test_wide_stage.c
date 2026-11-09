#define DINGHY_HOST
#define main rd_unused_main
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
int main(void){
 cq_init_project();kf_init();kh_init();set_defaults();assert(set.pad2==30&&set.pad==0x54);
 kf_init();rd_volume=100;kf_set_game_balance(0);rd_hit_feedback(1);int32_t click[512]={0};app_audio(click,256);int peak=0;for(unsigned i=0;i<512;i++){int a=click[i]<0?-click[i]:click[i];if(a>peak)peak=a;}assert(peak>200&&peak<=1000);
 for(unsigned j=0;j<4;j++){for(unsigned i=0;i<512;i++)click[i]=0;app_audio(click,256);}for(unsigned i=0;i<512;i++)assert(click[i]==0);

 rd_click_trigger=0;
 kf_set_game_balance((uint32_t)set.pad2);assert(kf_gp==5734&&kf_gb==1228);
 scr=SCR_SETTINGS;rd_setting_sel=3;rd_setting_edit=1;uint32_t changed=0;
 host_enc[ENC_SELECT]=-99;rd_controls(0,&changed);assert(set.pad2==0&&kf_gb==0);
 host_enc[ENC_SELECT]=99;rd_controls(0,&changed);assert(set.pad2==100&&kf_gb==4096);
 host_enc[ENC_SELECT]=-14;rd_controls(0,&changed);assert(set.pad2==30&&kf_gb==1228&&changed);
 kg_start(CQ_SLOT,100,0,44100);unsigned assist=0;for(;assist<kg.song->nband;assist++)if(kg.song->band[assist].part==0&&!kg.song->band[assist].ks)break;
 assert(assist<kg.song->nband);kf_seq.bi=assist;kf_now=kf_seq_frame(kg.song->band[assist].t);kf_seq_post(kf_now+1);int32_t audio[64]={0};kf_render(audio,32);
 unsigned found=0;for(unsigned i=0;i<KF_NV;i++)if(kf_v[i].state&&kf_v[i].part==0&&kf_v[i].pitch==kg.song->band[assist].pitch)found=1;assert(found);
 /* An automatic fill creates a foreground voice, without advancing score or key hints. */
 assert(kg.score==0&&kg.combo==0&&rd_click_trigger==0);for(unsigned k=0;k<27;k++)assert(!key_note[k]);
 cv_begin(240,240,c565(RD_BG));kg.judge=KJ_PERFECT;kg.judge_ms=100;kg.combo=174;kg.pos=kg.vpos=kg.notes[10].t*256;kg.first=10;cq_stage(200,0,0);send_changed();write_ppm_as("build/wide-stage.ppm");
 /* Cue footer is only26px; no large white-key rectangles remain at the old keyboard rows. */
 assert(fb[210*240+120]!=c565(RD_BG));assert(fb[239*240+120]==c565(RD_BG));
 puts("1.5x actors / expanded stage, editable quiet backing, foreground automatic fills without key hints passed");return 0;
}
