/* SPDX-License-Identifier: GPL-3.0-only
 * Read-only, bounded CQP2 view: the same bundle contains notes, accompaniment and artwork. */
#pragma once
#include <stddef.h>
#ifdef RD_SPC
#include "cq_spc_demo.h"
#else
#include "cq_demo.h"
#endif
#define CQ_SLOT (KF_NSONG - 1u)
#define CQ_LIMIT (180u * 1024u)
static const uint8_t *cq_data;
static uint32_t cq_size;
static int cq_ready;
static uint32_t cq_u32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t cq_u16(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8; }
static int cq_range(uint32_t off,uint32_t count,uint32_t stride,uint32_t size) { return off>=160u && !(off&1u) && off<=size && count <= (size-off)/stride; }
static int cq_string(const uint8_t *p,uint32_t off,uint32_t size) { uint32_t i; if(off<160u || off>=size)return 0;for(i=0;i<64u && i<size-off;i++)if(!p[off+i])return 1;return 0; }
static int cq_open(const uint8_t *p,uint32_t size,kf_song_t *out) {
    uint32_t i,j,crc=~0u,ni,nf,nc,ns,io,fo,clo,sto,co,bo,nn,nb;
    kf_song_t song={0};
    _Static_assert(sizeof(kf_cnote_t)==8 && sizeof(kf_bev_t)==8,"CQP2 note ABI");
    if(size<160u || size>CQ_LIMIT || ((uintptr_t)p&3u) || p[0]!='C'||p[1]!='Q'||p[2]!='P'||p[3]!='2'||cq_u32(p+4)!=2u||cq_u32(p+8)!=size)return 0;
    for(i=160;i<size;i++){crc^=p[i];for(j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    if((~crc)!=cq_u32(p+12) || !cq_string(p,cq_u32(p+16),size)||!cq_string(p,cq_u32(p+20),size))return 0;
    co=cq_u32(p+36);nn=cq_u32(p+40);bo=cq_u32(p+44);nb=cq_u32(p+48);
    io=cq_u32(p+52);ni=cq_u32(p+56);fo=cq_u32(p+60);nf=cq_u32(p+64);clo=cq_u32(p+68);nc=cq_u32(p+72);sto=cq_u32(p+76);ns=cq_u32(p+80);
    if(!nn||nn>400||nb>65535||!ni||ni>32||!nf||nf>256||!nc||nc>64||!ns||ns>4096)return 0;
    if(!cq_range(co,nn,8,size)||!cq_range(bo,nb,8,size)||!cq_range(io,ni,16,size)||!cq_range(fo,nf,16,size)||!cq_range(clo,nc,8,size)||!cq_range(sto,ns,4,size))return 0;
    if(!cq_u32(p+24)||!cq_u32(p+28)||cq_u32(p+32)<8||cq_u32(p+32)>65535)return 0;
    for(i=0;i<nn;i++){const uint8_t *r=p+co+i*8; int oct=(int8_t)r[6],sounding=(int)r[4]+12*oct;
        if(!cq_u16(r+2)||r[4]>127||r[5]>=27||r[4]!=r[5]+53u||oct < -4||oct>4||sounding<0||sounding>127||r[7]>1||(i&&cq_u16(r)<cq_u16(r-8)))return 0;
        for(j=0;j<i;j++){const uint8_t *prev=p+co+j*8;if(prev[5]==r[5]&&prev[6]!=r[6])return 0;}}
    for(i=0;i<nb;i++){const uint8_t *r=p+bo+i*8;if(!cq_u16(r+2)||r[4]>6||r[5]>127||r[6]>127||r[7]>(r[4]==0?5:4)||(i&&cq_u16(r)<cq_u16(r-8)))return 0;}
    for(i=0;i<ni;i++){const uint8_t *r=p+io+i*16;uint32_t w=cq_u16(r),h=cq_u16(r+2),off=cq_u32(r+4);uint32_t flags=cq_u32(r+12),codec=flags&6u,bytes=codec==2?32u+(w*h+1u)/2u:codec==4?512u+w*h:w*h*2u;
     if(!w||w>240||!h||h>240||flags>5||codec==6||cq_u32(r+8)!=bytes||!cq_range(off,bytes,1,size))return 0;}
    for(i=0;i<nf;i++){const uint8_t *r=p+fo+i*16;uint32_t image=cq_u32(r),x=cq_u16(r+4),y=cq_u16(r+6),w=cq_u16(r+8),h=cq_u16(r+10);const uint8_t *im;if(image>=ni||!w||!h)return 0;im=p+io+image*16;if(x+w>cq_u16(im)||y+h>cq_u16(im+2)||cq_u16(r+12)>w||cq_u16(r+14)>h)return 0;}
    for(i=0;i<nc;i++){const uint8_t *r=p+clo+i*8;uint32_t first=cq_u32(r),n=cq_u16(r+4);if(!n||first>ns||n>ns-first||cq_u16(r+6)>1)return 0;}
    for(i=0;i<ns;i++){const uint8_t *r=p+sto+i*4;if(cq_u16(r)>=nf||!cq_u16(r+2)||cq_u16(r+2)>2000)return 0;}
    for(i=84;i<=104;i+=4)if(cq_u32(p+i)>=nc)return 0;
    if(cq_u32(p+108)>=ni||cq_u32(p+112)>=ni||cq_u32(p+144)>=ni)return 0;
    {uint32_t nl=cq_u32(p+156),lo=cq_u32(p+152),pixels=0;
     if(nl){
      if(nl!=3||!cq_range(lo,nl,16,size)||cq_u32(p+136)!=196)return 0;
      for(i=0;i<nl;i++){const uint8_t *l=p+lo+i*16;uint32_t image=cq_u32(l),y=cq_u16(l+4),h=cq_u16(l+6);const uint8_t *im;
       if(image>=ni||y<28||!h||h>186||y+h>214||cq_u32(l+8)>256||cq_u32(l+12)>1)return 0;
       im=p+io+image*16;if(cq_u16(im)!=240)return 0;if(!cq_u32(l+12)){
        const uint8_t *first=p+io+cq_u32(p+lo)*16;if((cq_u32(im+12)&6u)!=4u)return 0;
        for(uint32_t q=0;q<512;q++)if(p[cq_u32(im+4)+q]!=p[cq_u32(first+4)+q])return 0;
       }pixels+=240*cq_u16(im+2);
       if(!i&&(y!=28||h!=186||(cq_u32(im+12)&1u)||cq_u32(l+12)))return 0;
      }
      if(pixels>180000)return 0;
     }else{const uint8_t *bg=p+io+cq_u32(p+108)*16;if(cq_u16(bg)!=240||cq_u16(bg+2)!=128||cq_u32(p+136)<120||cq_u32(p+136)>156)return 0;}
    }
    if(cq_u32(p+148) && (cq_u32(p+148)<1000||cq_u32(p+148)>3000))return 0;
    song.title=(const char *)p+cq_u32(p+16);song.file=(const char *)p+cq_u32(p+20);song.bpm_x100=cq_u32(p+24);song.fpt_q16=cq_u32(p+28);song.end=(uint16_t)cq_u32(p+32);song.transpose=127; song.level=0;song.hash=(uint16_t)(cq_u32(p+140)&0x3fffu);
    song.chart=(const kf_cnote_t *)(p+co);song.nchart=(uint16_t)nn;song.band=(const kf_bev_t *)(p+bo);song.nband=(uint16_t)nb;
    *out=song;return 1;
}
static void cq_init_project(void) {
    const uint8_t *p=CQ_DEMO_DATA;uint32_t n=sizeof(CQ_DEMO_DATA);
#ifdef DINGHY_HOST
    static uint32_t external[CQ_LIMIT/4];
    const char *file=getenv("CQ_PROJECT");
    if(file && *file){FILE *f=fopen(file,"rb");size_t count;if(!f){fprintf(stderr,"cannot open CQ_PROJECT\n");exit(2);}count=fread(external,1,sizeof(external),f);if(fgetc(f)!=EOF){fclose(f);exit(2);}fclose(f);p=(const uint8_t *)external;n=(uint32_t)count;}
#endif
    cq_ready=cq_open(p,n,&KF_SONG[CQ_SLOT]);
    if(cq_ready){cq_data=p;cq_size=n;}
#ifdef DINGHY_HOST
    if(!cq_ready){fprintf(stderr,"CQP2 rejected\n");exit(2);}
#endif
}
static uint32_t cq_get(uint32_t at){return cq_u32(cq_data+at);}
static const uint8_t *cq_image(uint32_t i){return cq_data+cq_get(52)+i*16;}
typedef struct {const uint16_t *pixels,*palette;const uint8_t *indices;uint32_t codec,transparent;} cq_pixels_t;
static cq_pixels_t cq_pixels(const uint8_t *im){
 const uint8_t *p=cq_data+cq_u32(im+4);uint32_t flags=cq_u32(im+12);cq_pixels_t v={(const uint16_t *)p,(const uint16_t *)p,p+(flags&2u?32:512),flags&6u,flags&1u};return v;
}
static inline uint16_t cq_pixel(cq_pixels_t v,uint32_t i){
 if(v.codec==2u){uint32_t index=v.indices[i>>1]>>((i&1u)*4u);return v.palette[index&15u];}
 if(v.codec==4u)return v.palette[v.indices[i]];
 return v.pixels[i];
}
static void cq_blit(uint32_t image,int x,int y,int sx,int sy,int w,int h) {
    const uint8_t *r=cq_image(image);uint32_t iw=cq_u16(r);cq_pixels_t pixels=cq_pixels(r);uint32_t transparent=pixels.transparent;int a,b;
    for(b=0;b<h;b++){int yy=y+b;if(yy<0||yy>=240)continue;for(a=0;a<w;a++){int xx=x+a;uint32_t v;if(xx<0||xx>=240)continue;v=cq_pixel(pixels,(sy+b)*(int)iw+sx+a);if(!transparent||v!=0xf81fu)cv_px[yy*240+xx]=swap16((uint16_t)v);}}
}
static void cq_draw_image(uint32_t image,int x,int y){const uint8_t *r=cq_image(image);cq_blit(image,x,y,0,0,(int)cq_u16(r),(int)cq_u16(r+2));}
static uint16_t cq_tint; /* temporary silhouette flash, drawing thread only */
static void cq_blit_pixels_scaled(cq_pixels_t pixels,int iw,int x,int y,int sx,int sy,int sw,int sh,int dw,int dh){
 int a,b,left=x<0?-x:0,right=dw,top=y<0?-y:0,bottom=dh;uint16_t xm[240];
 uint32_t transparent=pixels.transparent;uint16_t tint=cq_tint;
 if(dw<=0||dh<=0)return;
 if(x+right>240)right=240-x;
 if(y+bottom>240)bottom=240-y;
 if(left>=right||top>=bottom)return;
 if(!pixels.codec&&dw==sw&&dh==sh){
  for(b=top;b<bottom;b++){
   const uint16_t *row=pixels.pixels+(sy+b)*iw+sx+left;
   uint16_t *out=cv_px+(y+b)*240+x+left;
   if(!transparent&&!tint){for(a=0;a<right-left;a++)out[a]=swap16(row[a]);}
   else for(a=0;a<right-left;a++){uint16_t v=row[a];if(!transparent||v!=0xf81f)out[a]=swap16(tint?tint:v);}
  }
  return;
 }
 for(a=left;a<right;a++)xm[a-left]=(uint16_t)(sx+a*sw/dw);
 for(b=top;b<bottom;b++){
  uint32_t row=(uint32_t)(sy+b*sh/dh)*iw;uint16_t *out=cv_px+(y+b)*240+x+left;
  if(!transparent&&!tint){for(a=0;a<right-left;a++)out[a]=swap16(cq_pixel(pixels,row+xm[a]));}
  else for(a=0;a<right-left;a++){uint16_t v=cq_pixel(pixels,row+xm[a]);if(!transparent||v!=0xf81f)out[a]=swap16(tint?tint:v);}
 }
}
static void cq_blit_scaled(uint32_t image,int x,int y,int sx,int sy,int sw,int sh,int dw,int dh){const uint8_t *im=cq_image(image);cq_blit_pixels_scaled(cq_pixels(im),(int)cq_u16(im),x,y,sx,sy,sw,sh,dw,dh);}
static uint16_t cq_hero_cache[12000] __attribute__((section(".pool")));
static uint16_t cq_enemy_cache[7200] __attribute__((section(".pool")));
static const uint8_t *cq_sprite_owner;
static uint32_t cq_cached_frame=~0u,cq_cached_enemy=~0u;
static uint32_t cq_clip_duration(uint32_t clip) {
    const uint8_t *r=cq_data+cq_get(68)+clip*8,*st=cq_data+cq_get(76)+cq_u32(r)*4;uint32_t i,total=0;
    for(i=0;i<cq_u16(r+4);i++)total+=cq_u16(st+i*4+2);
    return total;
}
static void cq_draw_clip_scaled(uint32_t clip,uint32_t ms,int x,int y,int scale) {
    const uint8_t *r=cq_data+cq_get(68)+clip*8,*st=cq_data+cq_get(76)+cq_u32(r)*4,*f;uint32_t n=cq_u16(r+4),i,total=0,frame=0;
    for(i=0;i<n;i++)total+=cq_u16(st+i*4+2);
    if(cq_u16(r+6))ms%=total;else if(ms>=total)ms=total-1;
    for(i=0;i<n;i++){uint32_t d=cq_u16(st+i*4+2);if(ms<d){frame=cq_u16(st+i*4);break;}ms-=d;}
    f=cq_data+cq_get(60)+frame*16;
    uint32_t image=cq_u32(f);const uint8_t *im=cq_image(image);cq_pixels_t pixels=cq_pixels(im);int iw=(int)cq_u16(im),sx=(int)cq_u16(f+4),sy=(int)cq_u16(f+6),w=(int)cq_u16(f+8),h=(int)cq_u16(f+10);
    if(cq_sprite_owner!=cq_data){cq_sprite_owner=cq_data;cq_cached_frame=cq_cached_enemy=~0u;}
    if(clip==cq_get(104)&&cq_u16(im)*cq_u16(im+2)<=7200u){
     if(cq_cached_enemy!=image){for(uint32_t q=0;q<cq_u16(im)*cq_u16(im+2);q++)cq_enemy_cache[q]=cq_pixel(pixels,q);cq_cached_enemy=image;}
     pixels.pixels=cq_enemy_cache;pixels.codec=0;
    }else if(w*h<=12000){
     if(cq_cached_frame!=frame){for(int b=0;b<h;b++)for(int a=0;a<w;a++)cq_hero_cache[b*w+a]=cq_pixel(pixels,(sy+b)*iw+sx+a);cq_cached_frame=frame;}
     pixels.pixels=cq_hero_cache;pixels.codec=0;iw=w;sx=sy=0;
    }
    cq_blit_pixels_scaled(pixels,iw,x-(int)cq_u16(f+12)*scale/256,y-(int)cq_u16(f+14)*scale/256,sx,sy,w,h,w*scale/256,h*scale/256);
}

static void cq_draw_clip(uint32_t clip,uint32_t ms,int x,int y){cq_draw_clip_scaled(clip,ms,x,y,256);}
