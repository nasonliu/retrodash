#define DINGHY_HOST
#ifndef RD_SPC
#define RD_SPC
#endif
#define main rd_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
static void play(unsigned index,unsigned seconds,unsigned targets,unsigned fills){
 song_start(index,0);assert(kg.n==targets&&CQ_SLOT==index);unsigned next=0;
 for(unsigned elapsed=0;elapsed<(seconds+3u)*44100u;elapsed+=256){
  unsigned f=kf_now;fm1_ms=(uint32_t)((uint64_t)f*1000/44100);kg_ms=fm1_ms;
  while(next<kg.n&&kg_note_frame(next)<f+256){unsigned t=kg_note_frame(next),key=kg.notes[next].lane;kg_press(key,t);kg_release(key,t+1);next++;}
  int32_t out[512]={0};app_audio(out,256);kg_update(kf_now);
 }
 assert(kg.done&&kg.acc10==1000&&kg.max_combo==targets&&kg.cnt[3]==0);
 assert(rd_spc_status(0)==0&&rd_spc_status(2)==targets&&rd_spc_status(3)==targets&&rd_spc_status(4)==fills);
 printf("Song %u: %s, %u manual hits, %u automatic fills, full combo\n",index+1,kg.song->title,targets,fills);
 song_stop();int32_t out[512]={0};app_audio(out,256);
}
int main(void){
 cq_init_project();assert(cq_ready&&KF_NSONG==2);set_defaults();kf_init();kh_init();
 assert(!strcmp(KF_SONG[0].title,"PEACEFUL DAYS"));assert(!strcmp(KF_SONG[1].title,"TERRA SNOW MARCH"));
 unsigned hash0=KF_SONG[0].hash,hash1=KF_SONG[1].hash;assert(hash0!=hash1);
 play(0,151,200,2);best_put(0,970);
 scr=SCR_SONGS;unsigned changed=0;host_enc[ENC_SELECT]=1;rd_controls(0,&changed);
 assert(changed&&CQ_SLOT==1&&set.sel==1&&cq_weather()==1&&cq_enemy_clip(0)!=cq_enemy_clip(1));
 play(1,115,78,7);best_put(1,980);
 scr=SCR_SONGS;host_enc[ENC_SELECT]=-1;rd_controls(0,&changed);
 assert(CQ_SLOT==0&&!cq_weather()&&KF_SONG[0].hash==hash0&&KF_SONG[1].hash==hash1);
 assert(best_of(0)>=0&&best_of(1)>=0&&best_of(0)!=best_of(1));
 song_start(0,0);unsigned next=0;
 for(unsigned elapsed=0;elapsed<12u*44100u;elapsed+=256){
  unsigned f=kf_now;kg_ms=(uint32_t)((uint64_t)f*1000/44100);
  while(next<kg.n&&kg_note_frame(next)<f+256){unsigned t=kg_note_frame(next),key=kg.notes[next].lane;kg_press(key,t);kg_release(key,t+1);next++;}
  int32_t out[512]={0};app_audio(out,256);kg_update(kf_now);
 }
 assert(rd_spc_status(0)==0&&rd_spc_status(2)>0&&rd_spc_status(3)>0);
 puts("Dual-song: SELECT switches projects; original song, score identities and restart audio remain intact");
}
