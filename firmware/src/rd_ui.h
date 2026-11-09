/* SPDX-License-Identifier: GPL-3.0-only: RetroDash universal UI, independent of song art. */
#pragma once
#include "cq_ui_font.h"
#include "rd_assets.h"
#define RD_BG 0x180c29u
#define RD_YELLOW 0xffef19u
#define RD_PINK 0xff36a7u
#define RD_MINT 0x62ffd0u
#define RD_TEXT 0xfff5ddu
#define RD_DIM 0xa591b8u
static int cq_ui_width(unsigned font,const char *s){int w=0;while(*s){unsigned c=(unsigned char)*s++;if(c>=32&&c<128)w+=CQ_UI_FONTS[font].width[c-32];}return w;}
static void cq_ui_text(unsigned font,int x,int y,const char *s,rgb_t color){
 const cq_ui_font_t *f=&CQ_UI_FONTS[font];uint16_t fg=c565(color),packed=swap16(fg);kcol_t col=kcol(fg);
 while(*s){unsigned c=(unsigned char)*s++,a,b;if(c<32||c>=128)continue;c-=32;unsigned w=f->width[c];const uint8_t *pixels=f->bits+f->offset[c];
  for(b=0;b<f->h;b++){int yy=y+(int)b;if((unsigned)yy>=240u)continue;uint16_t *out=cv_px+yy*240;
   for(a=0;a<w;a++){unsigned i=b*w+a,v=(pixels[i>>2]>>((i&3u)*2u))&3u;int xx=x+(int)a;if(v&&(unsigned)xx<240u)out[xx]=v==3?packed:kblend_k(out[xx],&col,v==1?21:42);}}
  x+=(int)w;
 }
}
static void rd_center(unsigned f,int x,int y,const char *s,rgb_t c){cq_ui_text(f,x-cq_ui_width(f,s)/2,y,s,c);}
static void rd_right(unsigned f,int x,int y,const char *s,rgb_t c){cq_ui_text(f,x-cq_ui_width(f,s),y,s,c);}
static void rd_asset(const uint16_t *p,int w,int h,int x,int y,int dw,int dh){
 int a,b,left=x<0?-x:0,right=dw,top=y<0?-y:0,bottom=dh;uint16_t xm[240];
 if(dw<=0||dh<=0)return;
 if(x+right>240)right=240-x;
 if(y+bottom>240)bottom=240-y;
 if(left>=right||top>=bottom)return;
 for(a=left;a<right;a++)xm[a-left]=(uint16_t)(a*w/dw);
 for(b=top;b<bottom;b++){const uint16_t *row=p+b*h/dh*w;uint16_t *out=cv_px+(y+b)*240+x+left;for(a=0;a<right-left;a++){uint16_t v=row[xm[a]];if(v!=0xf81f)out[a]=swap16(v);}}
}
static void rd_background(void){rd_asset(RD_BG_DATA,240,240,0,0,240,240);}
/* Flat panels are controls and separators; branded ring/burst/logo remain real raster assets. */
static void rd_button(int x,int y,int w,int h,const char *label,int selected){
 int i;rgb_t c=selected?RD_YELLOW:RD_DIM;
 for(i=0;i<h;i++){int d=i<h/2?i:h-1-i,cut=5-d/3;if(cut<0)cut=0;krect(x+cut,y+i,w-2*cut,1,c);if(!selected&&i>1&&i<h-2)krect(x+cut+2,y+i,w-2*cut-4,1,RD_BG);}
 rd_center(3,x+w/2,y+(h-20)/2+1,label,selected?RD_BG:RD_TEXT);
}
static void rd_header(const char *title){krect(0,0,240,29,RD_BG);cq_ui_text(3,8,6,title,RD_TEXT);krect(8,27,224,1,RD_YELLOW);}
static void rd_footer(const char *text){krect(0,222,240,18,RD_BG);krect(8,222,224,1,0x4b2b60u);rd_center(0,120,228,text,RD_TEXT);}
static void rd_value(char *s,int value,const char *suffix){char *p=s;if(value<0){*p++='-';value=-value;}p=knum(p,(uint32_t)value,0);kput(p,suffix);}

static void rd_grade(int x,int y,const char *s){rd_center(6,x,y,s,RD_YELLOW);}

static void rd_outline(unsigned f,int x,int y,const char *s,rgb_t color){
 cq_ui_text(f,x-1,y,s,RD_BG);cq_ui_text(f,x+1,y,s,RD_BG);cq_ui_text(f,x,y-1,s,RD_BG);cq_ui_text(f,x,y+1,s,RD_BG);cq_ui_text(f,x,y,s,color);
}

static void rd_text_pop(unsigned font,int x,int y,const char *s,rgb_t color,int scale){(void)scale;cq_ui_text(font,x,y,s,color);}
