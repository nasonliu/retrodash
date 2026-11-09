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
static const rd_spc_song_data *song=&RD_SPC_SONGS[0];
static volatile unsigned selected_song;
static rd_block_cache_t event_cache __attribute__((section(".pool")));
static bool rd_spc_is_lead(unsigned voice,unsigned srcn){for(unsigned i=0;i<song->part_count;i++)if(song->parts[i*2]==voice&&song->parts[i*2+1]==srcn)return true;return false;}
static unsigned event_byte(unsigned at){return rd_block_byte(song->events,&event_cache,at);}
static unsigned char regs[128];
static short pcm[80];
static unsigned pcm_at,pcm_count,clock_now,event_at,event_clock,event_reg,event_value,event_left;
static unsigned phase,step=32000,gate_end[8],manual_at;
static int a[2],b[2];
static volatile unsigned start_request,stop_request,pause_request,mix_request=30,start_frame,start_speed=100;
static volatile unsigned hit_write,hit_read;
static unsigned hit_queue[32];
static unsigned seen_start,seen_stop,active,ready,error_code,hit_count,suppressed,automatic;
static unsigned native_pairs,native_hash=2166136261u,last_hash,hash_second;

static unsigned u32(const unsigned char *p){return unsigned(p[0])|unsigned(p[1])<<8|unsigned(p[2])<<16|unsigned(p[3])<<24;}
static bool unpack_ram(){return rd_unpack(song->ram,song->ram_size,aram,65536);}

static void next_event(){
 if(!event_left){event_clock=0xffffffffu;return;}
 unsigned delta=0,shift=0;
 for(;;){
  if(event_at>=song->events->size||shift>28){error_code=2;event_left=0;event_clock=0xffffffffu;return;}
  unsigned v=event_byte(event_at++);delta|=(v&127)<<shift;shift+=7;if(!(v&128))break;
 }
 if(event_at+2>song->events->size){error_code=2;event_left=0;event_clock=0xffffffffu;return;}
 event_clock+=delta;event_reg=event_byte(event_at++);event_value=event_byte(event_at++);event_left--;
}
static unsigned scaled_volume(unsigned value,unsigned percent){
 int v=(signed char)value;v=v*int(percent)/100;return (unsigned char)(v>127?127:v< -128?-128:v);
}
static void write_volume(unsigned reg,unsigned value){
#ifdef RD_SPC_REFERENCE
 dsp.write(reg,value);
#else
 unsigned voice=reg>>4;unsigned lead=rd_spc_is_lead(voice,regs[voice*16+4]);dsp.write(reg,scaled_volume(value,lead?140:mix_request));
#endif
}
static void refresh_mix(){
 for(unsigned v=0;v<8;v++)for(unsigned side=0;side<2;side++)write_volume(v*16+side,regs[v*16+side]);
}
static void apply_event(){
 unsigned reg=event_reg,value=event_value;regs[reg]=value;
 if((reg&15)<2){write_volume(reg,value);return;}
#ifndef RD_SPC_REFERENCE
 if(reg==0x4c){
  while(manual_at<song->target_count&&song->targets[manual_at].clock<event_clock)manual_at++;
  for(unsigned voice=0;voice<8;voice++)if((value&(1u<<voice))&&rd_spc_is_lead(voice,regs[voice*16+4])){
   if(manual_at<song->target_count&&song->targets[manual_at].clock==event_clock&&song->targets[manual_at].voice==voice){value&=~(1u<<voice);suppressed++;}
   else {gate_end[voice]=0;automatic++;dsp.write(0x5c,regs[0x5c]);}
  }
 }
 if(reg==0x5c)for(unsigned voice=0;voice<8;voice++)if(gate_end[voice]&&clock_now<gate_end[voice])value&=~(1u<<voice);
#endif
 dsp.write(reg,value);
 if((reg&15u)==4) {unsigned base=reg&0x70u;write_volume(base,regs[base]);write_volume(base+1,regs[base+1]);}
}
static void refill(){
 unsigned end=clock_now+1024;dsp.set_output(pcm,80);
 while(event_clock<end){
  if(event_clock<clock_now){error_code=3;active=0;break;}
  if(event_clock>clock_now)dsp.run(event_clock-clock_now);
  clock_now=event_clock;apply_event();next_event();
 }
 for(unsigned voice=0;voice<8;voice++)if(gate_end[voice]&&gate_end[voice]<=end){
  if(gate_end[voice]>clock_now){dsp.run(gate_end[voice]-clock_now);clock_now=gate_end[voice];}
  dsp.write(0x5c,regs[0x5c]|(1u<<voice));gate_end[voice]=0;
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
 song=&RD_SPC_SONGS[selected_song<RD_SPC_NSONG?selected_song:0];event_cache.owner=0;error_code=0;
 if(!unpack_ram()){error_code=1;active=0;return;}
 dsp.init(aram);dsp.load(song->regs);memcpy(regs,song->regs,128);
 event_at=16;event_clock=0;event_left=event_byte(12)|(event_byte(13)<<8)|(event_byte(14)<<16)|(event_byte(15)<<24);next_event();
 clock_now=pcm_at=pcm_count=phase=manual_at=0;memset(gate_end,0,sizeof(gate_end));
 suppressed=automatic=hit_count=ready=0;hit_read=hit_write;
 native_pairs=last_hash=hash_second=0;native_hash=2166136261u;
 refresh_mix();
}
extern "C" void rd_spc_select(uint32_t index){if(index<RD_SPC_NSONG)selected_song=index;}
extern "C" void rd_spc_init(){reset();active=0;}
extern "C" void rd_spc_start(uint32_t frame,uint32_t speed){start_frame=frame;start_speed=speed?speed:100;pause_request=0;start_request++;}
extern "C" void rd_spc_stop(){stop_request++;}
extern "C" void rd_spc_pause(uint32_t paused){pause_request=paused;}
extern "C" void rd_spc_mix(uint32_t backing){mix_request=backing>100?100:backing;}
extern "C" void rd_spc_hit(uint32_t index){
 unsigned w=hit_write;if(w-hit_read>=32||index>=song->target_count)return;
 hit_queue[w&31]=index;hit_write=w+1;
}
static void apply_hits(){
 while(hit_read!=hit_write){
  const rd_spc_target &t=song->targets[hit_queue[hit_read&31]];hit_read++;
#ifndef RD_SPC_REFERENCE
  unsigned mask=1u<<t.voice,base=t.voice*16;
  dsp.write(0x5c,regs[0x5c]&~mask);
  for(unsigned r=0;r<8;r++)dsp.write(base+r,r<2?scaled_volume(t.regs[r],140):t.regs[r]);
  dsp.write(0x4c,mask);gate_end[t.voice]=clock_now+t.duration;hit_count++;
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
  if(clock_now>=song->duration){active=0;break;}
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
