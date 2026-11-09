#define DINGHY_HOST
#define main cq_host_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
static uint32_t keys(uint64_t bits){return (uint32_t)(bits>>14);}
static uint32_t led(unsigned key){return 1u<<(KEY_LED(key)-14);}
int main(void){
 cq_init_project();kf_init();kg_start(CQ_SLOT,100,0,44100);scr=SCR_PLAY;paused=0;
 unsigned a=kg.notes[0].lane,b=kg.notes[1].lane;assert(a!=b);
 for(int beat=0;beat<4;beat++){
  kg.pos=beat*KG_BEAT;leds();assert(keys(host_leds)==led(a));assert(keys(host_glow)==led(b));
  kg.pos=beat*KG_BEAT+KG_BEAT/2;leds();assert(keys(host_leds)==led(a));assert(keys(host_glow)==0);
 }
 /* Two consecutive equal pitches must keep NEXT steady at maximum. */
 kf_cnote_t notes[2]={kg.notes[0],kg.notes[0]};kg.notes=notes;kg.n=2;kg.first=0;kg.pos=KG_BEAT;
 leds();assert(keys(host_leds)==led(a));assert(keys(host_glow)==0);
 kg.auto_on=1;leds();assert(!keys(host_leds)&&!keys(host_glow));kg.auto_on=0;
 paused=1;leds();assert(!keys(host_leds)&&!keys(host_glow));paused=0;
 kg.n=1;leds();assert(keys(host_leds)==led(a));assert(!keys(host_glow));
 kg.nst[0]=KN_HIT;leds();assert(!keys(host_leds)&&!keys(host_glow));
 assert(cq_clip_duration(cq_get(88))==300);
 puts("NEXT maximum steady, THEN half-beat dim pulse, repeated keys, AUTO, pause and final note passed");
 return 0;
}
