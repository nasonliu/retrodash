#!/usr/bin/env python3
"""Pack generated UI art as fixed RGB565 assets, independent of song projects."""
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'assets/retrodash-ui';im=Image.open(p/'source-atlas.png').convert('RGBA');w,h=im.size
cells=[im.crop((x*w//2,y*h//2,(x+1)*w//2,(y+1)*h//2)) for y in range(2) for x in range(2)]
bg=Image.new('RGBA',cells[0].size,(24,12,41,255));bg.alpha_composite(cells[0]);bg=bg.resize((240,240),Image.Resampling.NEAREST)
assets=[('BG_DATA',bg)]
for name,c,size in [('LOGO',cells[1],(216,52)),('BURST',cells[2],(146,53)),('RING',cells[3],(82,82))]:
 # Hard alpha eliminates semi-transparent generation halos on RGB565 hardware.
 a=c.getchannel('A').point(lambda v:255 if v>=220 else 0)
 if name=='LOGO':
  a=Image.new('L',c.size)
  a.putdata([255 if px[3]>=220 and ((min(px[:3])>215) or(px[0]>200 and px[1]>180 and px[2]<130)) else 0 for px in c.getdata()])
 c.putalpha(a);bbox=a.getbbox()
 if not bbox:raise ValueError(name+' missing')
 c=c.crop(bbox).resize(size,Image.Resampling.NEAREST);assets.append((name,c))
lines=['/* Generated from original ImageGen UI art; see assets/retrodash-ui. */','#pragma once']
for name,im in assets:
 im.save(p/(name.lower()+'.png'));data=[]
 for rr,gg,bb,aa in im.getdata():
  v=((rr>>3)<<11)|((gg>>2)<<5)|(bb>>3)
  if aa<220:v=0xf81f
  elif v==0xf81f:v=0xf81e
  data.append(v)
 lines += [f'#define RD_{name}_W {im.width}',f'#define RD_{name}_H {im.height}',f'static const uint16_t RD_{name}[{len(data)}] = {{'+','.join(hex(v) for v in data)+'};']
(ROOT/'firmware/gen/rd_assets.h').write_text('\n'.join(lines)+'\n')
print('UI atlas packed:',sum(im.width*im.height*2 for _,im in assets),'bytes')
