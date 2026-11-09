#define DINGHY_HOST
#define main rd_unused_main
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
static void scalar(uint32_t image,int x,int y,int sx,int sy,int sw,int sh,int dw,int dh){
 const uint8_t *im=cq_image(image),*p=cq_data+cq_u32(im+4);uint8_t *decoded=0;
 if(cq_u32(im+12)&8u){unsigned palette=cq_u32(im+12)&2u?32u:512u,pixels=cq_u16(im)*cq_u16(im+2),bytes=cq_u32(im+12)&2u?(pixels+1u)/2u:pixels;
  decoded=malloc(palette+bytes);assert(decoded);memcpy(decoded,p,palette);assert(rd_unpack(p+palette,cq_u32(im+8)-palette,decoded+palette,bytes));p=decoded;
 }
 int iw=(int)cq_u16(im);uint32_t tr=cq_u32(im+12)&1u;
 for(int b=0;b<dh;b++)for(int a=0;a<dw;a++){int xx=x+a,yy=y+b;if(xx<0||xx>=240||yy<0||yy>=240)continue;uint32_t pixel=(sy+b*sh/dh)*iw+sx+a*sw/dw,codec=cq_u32(im+12)&6u;uint32_t index=codec==2?(p[32+pixel/2]>>((pixel%2)*4))&15u:codec==4?p[512+pixel]:0;uint16_t v=(uint16_t)cq_u16(p+(codec?index:pixel)*2);if(!tr||v!=0xf81f)cv_px[yy*240+xx]=swap16(cq_tint?cq_tint:v);}
 free(decoded);
}
int main(void){
 cq_init_project();assert(cq_ready);uint16_t *expected=malloc(sizeof cv_px);assert(expected);
 for(unsigned i=0;i<50;i++){
  unsigned image=i%2?cq_get(56)-1:cq_get(108);const uint8_t *im=cq_image(image);int sw=i%2?48:(int)cq_u16(im),sh=i%2?48:(int)cq_u16(im+2),x=(int)(i*37%320)-50,y=(int)(i*31%320)-50,dw=20+(int)(i*23%300),dh=20+(int)(i*19%300);
  cq_tint=i%3?0:0xffff;
  if(i%5==0){dw=sw;dh=sh;}
  for(unsigned p=0;p<240*240;p++)cv_px[p]=(uint16_t)(p*17);
  scalar(image,x,y,0,0,sw,sh,dw,dh);memcpy(expected,cv_px,sizeof cv_px);
  for(unsigned p=0;p<240*240;p++)cv_px[p]=(uint16_t)(p*17);
  if(i%5==0){unsigned iw=cq_u16(im),ih=cq_u16(im+2);uint16_t *raw=malloc(iw*ih*2);assert(raw);cq_pixels_t src=cq_pixels(im);for(unsigned j=0;j<iw*ih;j++)raw[j]=cq_pixel(src,j);cq_pixels_t view={raw,0,0,0,src.transparent};cq_blit_pixels_scaled(view,(int)iw,x,y,0,0,sw,sh,dw,dh);free(raw);}
  else cq_blit_scaled(image,x,y,0,0,sw,sh,dw,dh);
  assert(!memcmp(expected,cv_px,sizeof cv_px));
 }
 cq_tint=0;free(expected);puts("50 scaled opaque/transparent/tinted/clipped cases match the scalar renderer pixel-for-pixel");return 0;
}
