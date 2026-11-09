#define DINGHY_HOST
#ifndef RD_SPC
#define RD_SPC
#endif
#define main rd_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
int main(void){
 cq_init_project();assert(cq_ready&&cq_weather()==1);
 assert(cq_enemy_clip(0)!=cq_enemy_clip(1)&&cq_enemy_clip(2)==cq_enemy_clip(0));
 uint32_t size=cq_size;uint8_t *copy=malloc(size);assert(copy);memcpy(copy,cq_data,size);
 /* Check the footer after recomputing its CRC, so this exercises its bounds. */
 copy[size-8]=17;
 uint32_t crc=~0u;for(uint32_t i=160;i<size;i++){crc^=copy[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
 crc=~crc;memcpy(copy+12,&crc,4);kf_song_t song;assert(!cq_open(copy,size,&song));free(copy);
 cv_w=cv_h=240;memset(cv_px,0,240*240*2);cq_snow(0);
 uint16_t *before=malloc(240*240*2);memcpy(before,cv_px,240*240*2);memset(cv_px,0,240*240*2);cq_snow(1000);assert(memcmp(before,cv_px,240*240*2));
 for(unsigned i=0;i<28*240;i++)assert(!cv_px[i]);for(unsigned i=214*240;i<240*240;i++)assert(!cv_px[i]);
 puts("FF6: alternating mech/infantry, bounded scene footer, animated snow within viewport");
 free(before);return 0;
}
