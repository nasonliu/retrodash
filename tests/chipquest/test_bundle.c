#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KF_POOL
#include "kf_song.h"
static uint16_t cv_px[240*240];
static uint16_t swap16(uint16_t v){return (uint16_t)(v<<8|v>>8);}
#include "cq_bundle.h"
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static void reseal(uint8_t *p,uint32_t n){uint32_t c=~0u,i,j;for(i=160;i<n;i++){c^=p[i];for(j=0;j<8;j++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}put32(p+12,~c);}
int main(void){
    kf_song_t s;uint8_t *bad=malloc(sizeof CQ_DEMO_DATA);uint32_t off;
    assert(bad);assert(cq_open(CQ_DEMO_DATA,sizeof CQ_DEMO_DATA,&s));
    assert(!strcmp(s.title,"SLOW STREET")&&s.nchart==32&&s.nband==130&&s.chart[0].pitch==60&&s.chart[0].lane==7);
    assert((const uint8_t *)s.chart>=CQ_DEMO_DATA&&(const uint8_t *)(s.band+s.nband)<=CQ_DEMO_DATA+sizeof CQ_DEMO_DATA);
    assert(!cq_open(CQ_DEMO_DATA,sizeof CQ_DEMO_DATA-1,&s));
    memcpy(bad,CQ_DEMO_DATA,sizeof CQ_DEMO_DATA);bad[sizeof CQ_DEMO_DATA-1]^=1;assert(!cq_open(bad,sizeof CQ_DEMO_DATA,&s));
    memcpy(bad,CQ_DEMO_DATA,sizeof CQ_DEMO_DATA);put32(bad+52,0xfffffff0u);assert(!cq_open(bad,sizeof CQ_DEMO_DATA,&s));
    memcpy(bad,CQ_DEMO_DATA,sizeof CQ_DEMO_DATA);put32(bad+56,0xffffffffu);assert(!cq_open(bad,sizeof CQ_DEMO_DATA,&s));
    memcpy(bad,CQ_DEMO_DATA,sizeof CQ_DEMO_DATA);put32(bad+84,10000);assert(!cq_open(bad,sizeof CQ_DEMO_DATA,&s));
    memcpy(bad,CQ_DEMO_DATA,sizeof CQ_DEMO_DATA);off=cq_u32(bad+60);bad[off+8]=255;bad[off+9]=255;reseal(bad,sizeof CQ_DEMO_DATA);assert(!cq_open(bad,sizeof CQ_DEMO_DATA,&s));
    cq_init_project();assert(cq_ready);off=cq_get(84);cq_draw_clip(off,0,32,144);cq_draw_clip(off,20000,-4,5);
    free(bad);puts("bundle loader: music pointers, art references, CRC, truncation, invalid offsets and clipped drawing passed");return 0;
}
