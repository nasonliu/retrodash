#define DINGHY_HOST
#define main rd_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
static void set32(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static void reseal(uint8_t *p,uint32_t n){uint32_t c=~0u;for(unsigned i=160;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=c>>1^((0u-(c&1u))&0xedb88320u);}set32(p+12,~c);}
int main(void){
 cq_init_project();assert(cq_ready);kf_init();kh_init();set_defaults();kg_start(CQ_SLOT,100,0,44100);assert(!strcmp(kg.song->title,"PEACEFUL DAYS"));assert(kg.n==174&&kg.song->nband==3256&&kg.song->bpm_x100==8600);
 uint32_t frame=kg_note_frame(0),key=kg.notes[0].lane;kg_ms=100;kg_update(frame);kg_press(key,frame);assert(kg.judge==KJ_PERFECT&&key_note[key]==78);kg_release(key,frame+100);
 kg_start(CQ_SLOT,100,0,44100);kg.auto_on=1;
 for(frame=0;frame<44100u*250u;frame+=256){kg_ms=frame/44;kf_seq_post(frame+4096);kg_update(frame);if(kg.done&&kg.pos>=((int32_t)kg.song->end+4)*KG_BEAT)break;}
 assert(kg.done&&kg.acc10==1000&&kg.max_combo==174&&kg.cnt[3]==0);for(unsigned k=0;k<27;k++)assert(!key_note[k]);
 uint8_t *bad=malloc(cq_size);kf_song_t song;assert(bad);memcpy(bad,cq_data,cq_size);bad[cq_get(36)+6]=5;reseal(bad,cq_size);assert(!cq_open(bad,cq_size,&song));memcpy(bad,cq_data,cq_size);bad[cq_get(44)+7]=6;reseal(bad,cq_size);assert(!cq_open(bad,cq_size,&song));free(bad);
 puts("Peaceful Days: actual78 sound on mapped F#3 key, 174-target full combo, all keys released, octave/patch validation passed");return 0;
}
