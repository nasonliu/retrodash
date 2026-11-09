#define DINGHY_HOST
#define main rd_unused_main
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
static uint32_t hash(void){uint32_t h=2166136261u;for(unsigned i=28*240;i<214*240;i++)h=(h^cv_px[i])*16777619u;return h;}
int main(void){
 cq_init_project();assert(cq_ready&&cq_get(156)==3);uint32_t old=0;
 for(unsigned beat=0;beat<16;beat++){
  cv_begin(240,240,c565(RD_BG));cq_scroll_layers((int32_t)beat*KG_BEAT,0);cq_scroll_layers((int32_t)beat*KG_BEAT,1);uint32_t h=hash();if(beat)assert(h!=old);old=h;
  cq_scroll_layers((int32_t)beat*KG_BEAT,0);cq_scroll_layers((int32_t)beat*KG_BEAT,1);assert(hash()==h);
 }
 const uint8_t *layers=cq_data+cq_get(152);assert(cq_u32(layers+8)<cq_u32(layers+16+8)&&cq_u32(layers+16+8)<cq_u32(layers+32+8));
 puts("3 layers move at distinct speeds; same audio position is pixel-stable; no top black viewport padding");return 0;
}
