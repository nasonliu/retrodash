/* SPDX-License-Identifier: GPL-3.0-only
 * SPC700 is compiled offline; only the original S-DSP runs on the FM-1.
 * The audio callback owns all DSP state. Main-loop requests use 32-bit stores.
 */
#include "rd_spc.h"
#ifdef RD_SPC_FAST
#include "fast/SPC_DSP.h"
#else
#include "SPC_DSP.h"
#endif
#include "rd_spc_data.h"
#include <string.h>

#ifdef DINGHY_HOST
static unsigned char aram[65536];
#else
static unsigned char aram[65536] __attribute__((section(".pool")));
#endif
static SPC_DSP dsp;
static unsigned char regs[128];
static short pcm[80];
static unsigned pcm_at,pcm_count,clock_now,event_at,event_clock,event_reg,event_value,event_left;
static unsigned phase,step=32000,gate_end,manual_at;
static int a[2],b[2];
static volatile unsigned start_request,stop_request,pause_request,mix_request=30,start_frame,start_speed=100;
static volatile unsigned hit_write,hit_read;
static unsigned hit_queue[32];
static unsigned seen_start,seen_stop,active,ready,error_code,hit_count,suppressed,automatic;
static unsigned native_pairs,native_hash=2166136261u,last_hash,hash_second;

static unsigned u32(const unsigned char *p){return unsigned(p[0])|unsigned(p[1])<<8|unsigned(p[2])<<16|unsigned(p[3])<<24;}
static bool unpack_ram(){
 const unsigned char *p=RD_SPC_RAM,*end=p+sizeof(RD_SPC_RAM);unsigned at=0;
 while(p<end){
  unsigned token=*p++,n=token>>4;
  if(n==15){unsigned x;do{if(p==end)return false;x=*p++;n+=x;}while(x==255);}
  if(n>unsigned(end-p)||n>65536-at)return false;
  memcpy(aram+at,p,n);p+=n;at+=n;if(p==end)break;
  if(end-p<2)return false;unsigned offset=p[0]|unsigned(p[1])<<8;p+=2;
  if(!offset||offset>at)return false;n=token&15;
  if(n==15){unsigned x;do{if(p==end)return false;x=*p++;n+=x;}while(x==255);}
  n+=4;if(n>65536-at)return false;
  while(n--){aram[at]=aram[at-offset];at++;}
 }
 return at==65536;
}
static void next_event(){
 if(!event_left){event_clock=0xffffffffu;return;}
 unsigned delta=0,shift=0;
 for(;;){
  if(event_at>=sizeof(RD_SPC_EVENTS)||shift>28){error_code=2;event_left=0;event_clock=0xffffffffu;return;}
  unsigned v=RD_SPC_EVENTS[event_at++];delta|=(v&127)<<shift;shift+=7;if(!(v&128))break;
 }
 if(event_at+2>sizeof(RD_SPC_EVENTS)){error_code=2;event_left=0;event_clock=0xffffffffu;return;}
 event_clock+=delta;event_reg=RD_SPC_EVENTS[event_at++];event_value=RD_SPC_EVENTS[event_at++];event_left--;
}
static unsigned scaled_volume(unsigned value,unsigned percent){
 int v=(signed char)value;v=v*int(percent)/100;return (unsigned char)(v>127?127:v< -128?-128:v);
}
static void write_volume(unsigned reg,unsigned value){
#ifdef RD_SPC_REFERENCE
 dsp.write(reg,value);
#else
 unsigned lead=(reg<2&&regs[4]==34);dsp.write(reg,scaled_volume(value,lead?140:mix_request));
#endif
}
static void refresh_mix(){
 for(unsigned v=0;v<8;v++)for(unsigned side=0;side<2;side++)write_volume(v*16+side,regs[v*16+side]);
}
static void apply_event(){
 unsigned reg=event_reg,value=event_value;regs[reg]=value;
 if((reg&15)<2){write_volume(reg,value);return;}
#ifndef RD_SPC_REFERENCE
 if(reg==0x4c&&(value&1)&&regs[4]==34){
  while(manual_at<sizeof(RD_SPC_TARGETS)/sizeof(*RD_SPC_TARGETS)&&RD_SPC_TARGETS[manual_at].clock<event_clock)manual_at++;
  if(manual_at<sizeof(RD_SPC_TARGETS)/sizeof(*RD_SPC_TARGETS)&&RD_SPC_TARGETS[manual_at].clock==event_clock){value&=~1u;suppressed++;}
  else {gate_end=0;automatic++;dsp.write(0x5c,regs[0x5c]);}
 }
 if(reg==0x5c&&gate_end&&clock_now<gate_end)value&=~1u;
#endif
 dsp.write(reg,value);
 if(reg==4) {write_volume(0,regs[0]);write_volume(1,regs[1]);}
}
static void refill(){
 unsigned end=clock_now+1024;dsp.set_output(pcm,80);
 while(event_clock<end){
  if(event_clock<clock_now){error_code=3;active=0;break;}
  if(event_clock>clock_now)dsp.run(event_clock-clock_now);
  clock_now=event_clock;apply_event();next_event();
 }
 if(gate_end&&gate_end<=end){
  if(gate_end>clock_now){dsp.run(gate_end-clock_now);clock_now=gate_end;}
  dsp.write(0x5c,regs[0x5c]|1);gate_end=0;
 }
 if(end>clock_now)dsp.run(end-clock_now);
 clock_now=end;pcm_count=dsp.sample_count()/2;pcm_at=0;
}
static void pair(int *p){if(pcm_at>=pcm_count)refill();p[0]=pcm[pcm_at*2];p[1]=pcm[pcm_at*2+1];pcm_at++;
#if defined(RD_PROFILE_BOOT) || defined(DINGHY_HOST)
 native_hash=(native_hash^(unsigned short)p[0])*16777619u;
 native_hash=(native_hash^(unsigned short)p[1])*16777619u;
 if(++native_pairs%32000u==0){last_hash=native_hash;hash_second=native_pairs/32000u;native_hash=2166136261u;}
#endif
}
static void reset(){
 error_code=0;
 if(!unpack_ram()){error_code=1;active=0;return;}
 dsp.init(aram);dsp.load(RD_SPC_REGS);memcpy(regs,RD_SPC_REGS,128);
 event_at=16;event_clock=0;event_left=u32(RD_SPC_EVENTS+12);next_event();
 clock_now=pcm_at=pcm_count=phase=gate_end=manual_at=0;
 suppressed=automatic=hit_count=ready=0;hit_read=hit_write;
 native_pairs=last_hash=hash_second=0;native_hash=2166136261u;
 refresh_mix();
}
extern "C" void rd_spc_init(){reset();active=0;}
extern "C" void rd_spc_start(uint32_t frame,uint32_t speed){start_frame=frame;start_speed=speed?speed:100;pause_request=0;start_request++;}
extern "C" void rd_spc_stop(){stop_request++;}
extern "C" void rd_spc_pause(uint32_t paused){pause_request=paused;}
extern "C" void rd_spc_mix(uint32_t backing){mix_request=backing>100?100:backing;}
extern "C" void rd_spc_hit(uint32_t index){
 unsigned w=hit_write;if(w-hit_read>=32||index>=sizeof(RD_SPC_TARGETS)/sizeof(*RD_SPC_TARGETS))return;
 hit_queue[w&31]=index;hit_write=w+1;
}
static void apply_hits(){
 while(hit_read!=hit_write){
  const rd_spc_target &t=RD_SPC_TARGETS[hit_queue[hit_read&31]];hit_read++;
#ifndef RD_SPC_REFERENCE
  dsp.write(0x5c,regs[0x5c]&~1u);
  for(unsigned r=0;r<8;r++){if(r<2)write_volume(r,t.regs[r]);else dsp.write(r,t.regs[r]);}
  dsp.write(0x4c,1);gate_end=clock_now+t.duration;hit_count++;
#else
  (void)t;
#endif
 }
}
extern "C" void rd_spc_render(int32_t *out,uint32_t frames,uint32_t now){
 if(seen_stop!=stop_request){seen_stop=stop_request;active=0;}
 if(seen_start!=start_request){seen_start=start_request;reset();step=320*start_speed;active=!error_code;}
 static unsigned last_mix=0xffffffffu;
 if(last_mix!=mix_request){last_mix=mix_request;refresh_mix();}
 if(!active||pause_request)return;
 apply_hits();
 for(unsigned i=0;i<frames;i++){
  if((int32_t)(now+i-start_frame)<0)continue;
  if(clock_now>=151u*1024000u){active=0;break;}
  if(!ready){pair(a);pair(b);ready=1;}
  int frac=int(phase*32768u/44100u);
  out[2*i]+=a[0]+((b[0]-a[0])*frac>>15);
  out[2*i+1]+=a[1]+((b[1]-a[1])*frac>>15);
  phase+=step;
  if(phase>=44100){phase-=44100;a[0]=b[0];a[1]=b[1];pair(b);}
 }
}
extern "C" uint32_t rd_spc_status(uint32_t field){
 return field==0?error_code:field==1?clock_now:field==2?hit_count:field==3?suppressed:field==4?automatic:field==6?last_hash:field==7?hash_second:active;
}
