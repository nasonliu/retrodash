/* SPDX-License-Identifier: GPL-3.0-only: RetroDash fixed HUD over per-song art. */
#pragma once
#include "rd_ui.h"
static void cq_pitch_name(uint32_t pitch,char *s){
 static const char *const names[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 const char *n=names[pitch%12];while(*n)*s++=*n++;*s++=(char)('0'+pitch/12-1);*s=0;
}
static int32_t cq_approach_pos(void){uint32_t ms=cq_get(148);if(!ms)ms=1800;ms=ms*100u/rd_scroll_speed;return(int32_t)((uint64_t)kg.song->bpm_x100*ms*KG_BEAT/6000000u);}
static int32_t cq_stage_x(uint32_t t,int32_t pos){int32_t lead=cq_approach_pos(),delta=(int32_t)t*256-pos;if(lead<1)lead=1;if(delta>lead*2)return 512;if(delta < -lead)return -128;return 88+delta*140/lead;}
static void cq_impact(uint32_t ms,uint32_t clock,int floor){
 static const int8_t dx[8]={-2,-1,1,2,2,1,-1,-2},dy[8]={-1,-2,-2,-1,1,2,2,1};
 for(unsigned f=0;f<kg.nfx&&f<8;f++){
  unsigned at=(kg.nfx-1-f)%8;uint32_t age=ms-kg.fx[at].ms;
  if(!kg.fx[at].big||age>=240u)continue;
  int d=age<80u?38+(int)age/2:78+(int)(age-80u)/5;
  rd_asset(RD_RING,RD_RING_W,RD_RING_H,88-d/2,floor-30-d/2,d,d);
  if(age<180u){
   int scale=256,xx=88+(int)age/7,yy=floor-(int)age/13;
   if(age<70u)cq_tint=0xffff;
   cq_draw_clip_scaled(cq_enemy_clip(kg.fx[at].note),clock,xx,yy,scale);cq_tint=0;
  }
  for(int i=0;i<8;i++){int x=88+dx[i]*(8+(int)age/14),y=floor-30+dy[i]*(8+(int)age/16),size=age<90?4:2;if(y>=28&&y<210)krect(x,y,size,size,age<70?RD_TEXT:RD_YELLOW);}
 }
}
/* Only recompose the background when a far/middle layer advances a native pixel. */
#ifdef RD_SPC
typedef uint16_t cq_scene_pixel_t;
#else
typedef uint8_t cq_scene_pixel_t;
#endif
static cq_scene_pixel_t cq_scene_cache[240*186] __attribute__((section(".pool"),aligned(4)));
static const uint8_t *cq_scene_owner;
static uint32_t cq_layer_offset[3];
static uint16_t cq_scene_palette[256];
static cq_scene_pixel_t cq_scene_color(uint8_t index){
#ifdef RD_SPC
 return cq_scene_palette[index];
#else
 return index;
#endif
}
static void cq_paint_layer(cq_scene_pixel_t *dst,const uint8_t *l,uint32_t off){
 const uint8_t *im=cq_image(cq_u32(l));cq_pixels_t pixels=cq_pixels(im);uint32_t h=cq_u16(l+6),ih=cq_u16(im+2),yy=cq_u16(l+4),n=240u-off;
 for(uint32_t y=0;y<h;y++){const uint8_t *row=pixels.indices+y*ih/h*240;cq_scene_pixel_t *out=dst+(yy+y-28)*240;
  if(!pixels.transparent){for(uint32_t x=0;x<n;x++)out[x]=cq_scene_color(row[x+off]);for(uint32_t x=n;x<240;x++)out[x]=cq_scene_color(row[x-n]);}
  else{for(uint32_t x=0;x<n;x++){uint8_t v=row[x+off];if(v)out[x]=cq_scene_color(v);}for(uint32_t x=n;x<240;x++){uint8_t v=row[x-n];if(v)out[x]=cq_scene_color(v);}}
 }
}
static void cq_foreground(const uint8_t *l,uint32_t off){
 const uint8_t *im=cq_image(cq_u32(l));cq_pixels_t p=cq_pixels(im);uint32_t h=cq_u16(l+6),ih=cq_u16(im+2),yy=cq_u16(l+4),n=240u-off;
 for(uint32_t y=0;y<h;y++){uint32_t row=y*ih/h*240;uint16_t *out=cv_px+(yy+y)*240;for(uint32_t x=0;x<n;x++){uint16_t v=cq_pixel(p,row+x+off);if(!p.transparent||v!=0xf81f)out[x]=swap16(v);}for(uint32_t x=n;x<240;x++){uint16_t v=cq_pixel(p,row+x-n);if(!p.transparent||v!=0xf81f)out[x]=swap16(v);}}
}
static void cq_scroll_layers(int32_t pos,int foreground){
 uint32_t count=cq_get(156),phase=pos>0?(uint32_t)pos/1024u:0;
 if(count){
  int dirty=cq_scene_owner!=cq_data;
  for(uint32_t i=0;i<count;i++){const uint8_t *l=cq_data+cq_get(152)+i*16;uint32_t off=phase*cq_u32(l+8)/256u%240u;
   if(!cq_u32(l+12)&&cq_layer_offset[i]!=off)dirty=1;
   cq_layer_offset[i]=off;
  }
  if(!foreground){
   if(dirty){if(cq_scene_owner!=cq_data){const uint8_t *first=cq_data+cq_get(152);cq_pixels_t p=cq_pixels(cq_image(cq_u32(first)));for(uint32_t q=0;q<256;q++)cq_scene_palette[q]=swap16(p.palette[q]);}for(uint32_t i=0;i<count;i++){const uint8_t *l=cq_data+cq_get(152)+i*16;if(!cq_u32(l+12))cq_paint_layer(cq_scene_cache,l,cq_layer_offset[i]);}cq_scene_owner=cq_data;}
#ifdef RD_SPC
   {const uint32_t *src=(const uint32_t *)cq_scene_cache;uint32_t *out=(uint32_t *)(cv_px+28*240);for(unsigned q=0;q<240*186/2;q++)out[q]=src[q];}
#else
   for(int y=0;y<186;y++){const uint8_t *row=cq_scene_cache+y*240;uint16_t *out=cv_px+(28+y)*240;for(int x=0;x<240;x++)out[x]=cq_scene_palette[row[x]];}
#endif
  }else for(uint32_t i=0;i<count;i++){const uint8_t *l=cq_data+cq_get(152)+i*16;if(cq_u32(l+12))cq_foreground(l,cq_layer_offset[i]);}
 }else if(!foreground)cq_blit_scaled(cq_get(108),0,28,60,36,119,92,240,186);
}
static void cq_snow(uint32_t clock){
 if(!cq_weather())return;
 /* Bounded native-pixel particles, no frame buffers or audio-clock changes. */
 for(unsigned i=0;i<32;i++){
  unsigned speed=18u+(i%4u)*9u,t=clock*speed/1000u;
  int y=28+(int)((i*47u+t)%180u),x=(int)((i*73u+720u-(t*2u)%240u)%240u);
  rgb_t c=i%3u?0xA6BBDD:RD_TEXT;krect(x,y,2,1,c);
  if(i%3u==0&&x<237&&y>29)krect(x+2,y-1,2,1,c);
 }
}
static void cq_stage(uint32_t ms,const uint8_t *lit,const char *right){
 static uint32_t last_t0,last_clock,clip_clock,last_actor,last_judge;
 int32_t i,next=-1,then=-1,first=-1;uint32_t actor=cq_get(84),age=ms-kg.judge_ms,ground=cq_get(112),floor=196,clock=0;char a[8],score[24];(void)lit;
 {int32_t elapsed=(int32_t)(kg_frame_at_pos(kg.vpos)-kf_seq.t0);if(elapsed>0)clock=(uint32_t)elapsed*10u/441u;}
 kfill_rows(0,28,RD_BG);cq_scroll_layers(kg.vpos,0);
 {const uint8_t *g=cq_image(ground);int w=(int)cq_u16(g),offset=(kg.vpos>0?kg.vpos/(KG_BEAT/12):0)%w;for(i=-offset;i<240;i+=w)cq_blit_scaled(ground,i,(int)floor,0,0,(int)cq_u16(g),(int)cq_u16(g+2),w,(int)cq_u16(g+2));}
 for(i=(int32_t)kg.first;i<(int32_t)kg.n;i++)if(kg.nst[i]==KN_PENDING){if(first<0){first=i;next=kg.notes[i].pitch;}else{then=kg.notes[i].pitch;break;}}
 if(kg.done)actor=cq_get(100);else if(kg.judge&&age<cq_clip_duration(cq_get(kg.judge<=KJ_GOOD?88:92)))actor=cq_get(kg.judge<=KJ_GOOD?88:92);else for(i=(int32_t)kg.first;i<(int32_t)kg.n;i++)if(kg.nst[i]==KN_HELD){actor=cq_get(96);break;}
 if(last_t0!=kf_seq.t0||last_actor!=actor||(actor!=cq_get(84)&&last_judge!=kg.judge_ms))clip_clock=0;else if(clock>=last_clock)clip_clock+=clock-last_clock;
 last_t0=kf_seq.t0;last_clock=clock;last_actor=actor;last_judge=kg.judge_ms;
 if(actor==cq_get(84)&&kg.vpos>=0)clip_clock=(uint32_t)(kg.vpos%KG_BEAT)*696u/KG_BEAT;
 cq_draw_clip_scaled(actor,clip_clock,46,(int)floor,256);
 for(i=(int32_t)kg.first;i<(int32_t)kg.n;i++){
  const kf_cnote_t *n=&kg.notes[i];int x=cq_stage_x(n->t,kg.vpos);if(x>252)break;if(kg.nst[i]!=KN_PENDING&&kg.nst[i]!=KN_HELD)continue;if(kg.nst[i]==KN_HELD)x=88;if(x<74)continue;
  if(n->flags&1u){int end=cq_stage_x(n->t+n->len,kg.vpos);if(end>239)end=239;if(end>x)krect(x,(int)floor-18,end-x,3,RD_YELLOW);}
  cq_draw_clip_scaled(cq_enemy_clip((uint32_t)i),clock,x,(int)floor,256);
  if(i==first){int width,xx;cq_pitch_name(n->pitch,a);width=cq_ui_width(1,a)+8;xx=x-width/2;if(xx+width>238)xx=238-width;krect(xx,(int)floor-78,width,16,RD_YELLOW);cq_ui_text(1,xx+4,(int)floor-76,a,RD_BG);}
 }
 cq_scroll_layers(kg.vpos,1);
 cq_snow(clock);
 cq_impact(ms,clock,(int)floor);
 {int pulse=kg.pos>=0&&kg.pos%KG_BEAT<KG_BEAT/8;int hit=kg.judge&&kg.judge<=KJ_GOOD&&age<90u;
  krect(hit?85:87,(int)floor-78,hit?6:2,80,hit?RD_TEXT:pulse?RD_MINT:RD_YELLOW);krect(83,(int)floor-78,10,3,hit?RD_TEXT:RD_YELLOW);
  if(pulse)krect(0,(int)floor+11,240,2,RD_MINT);
 }
 /* Fixed global HUD: song palette never changes the semantics of input hints. */
 krect(0,0,240,28,RD_BG);cq_ui_text(1,7,4,kg.song->title,RD_TEXT);knum(score,(uint32_t)kg.score,6);rd_right(1,233,4,score,RD_TEXT);
 krect(7,21,226,3,0x49325du);if(kg.song->end&&kg.pos>0){int p=kg.pos/(int32_t)(((uint32_t)kg.song->end*KG_BEAT+225u)/226u);if(p>226)p=226;krect(7,21,p,3,RD_YELLOW);}
 knum(score,(uint32_t)kg.combo,0);{int pop=256;int x=232-cq_ui_width(5,score)*pop/256;
 rd_text_pop(5,x+1,32,score,RD_BG,pop);rd_text_pop(5,x,31,score,RD_YELLOW,pop);}rd_outline(2,232-cq_ui_width(2,"COMBO"),72,"COMBO",RD_TEXT);
 if(right)cq_ui_text(0,7,30,right,RD_MINT);
 if(kg.judge&&age<500u){
  int miss=kg.judge>KJ_GOOD,pop=256;
  int dy=miss?(int)age/22:age<90u?-(int)(90u-age)/30:0,width=cq_ui_width(4,KJ_TEXT[kg.judge])*pop/256;
  rgb_t c=miss?RD_TEXT:kg.judge==KJ_PERFECT?RD_YELLOW:RD_MINT;
  rd_asset(RD_BURST,RD_BURST_W,RD_BURST_H,3,29+dy,RD_BURST_W,RD_BURST_H);
  rd_text_pop(4,78-width/2+1,43+dy,KJ_TEXT[kg.judge],RD_BG,pop);rd_text_pop(4,78-width/2,41+dy,KJ_TEXT[kg.judge],c,pop);
  if(miss&&age<150u){krect(0,28,3,184,RD_PINK);krect(237,28,3,184,RD_PINK);}
 }
 if(!kg.judge&&kg.n&&kg.pos<(int32_t)kg.notes[0].t*256){int beats=((int32_t)kg.notes[0].t*256-kg.pos+KG_BEAT-1)/KG_BEAT;if(beats>3)rd_outline(3,120-cq_ui_width(3,"READY")/2,41,"READY",RD_TEXT);else{rd_value(a,beats,"");rd_outline(5,120-cq_ui_width(5,a)/2,32,a,RD_YELLOW);}}
 krect(0,214,240,26,RD_BG);krect(0,214,240,2,RD_PINK);
 krect(5,218,114,19,RD_YELLOW);cq_ui_text(1,10,224,"NEXT",RD_BG);if(next>=0)cq_pitch_name((uint32_t)next,a);else kput(a,"--");cq_ui_text(3,48,220,a,RD_BG);
 krect(125,218,110,19,RD_PINK);krect(127,220,106,15,RD_BG);cq_ui_text(1,130,224,"THEN",RD_TEXT);if(then>=0)cq_pitch_name((uint32_t)then,a);else kput(a,"--");cq_ui_text(3,175,220,a,RD_PINK);
 if(kg.done){krect(29,93,182,28,RD_BG);rd_center(3,120,98,kg.full?"FULL COMBO":"CLEAR",RD_MINT);}
}
