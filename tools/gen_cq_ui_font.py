#!/usr/bin/env python3
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];SIZES=[9,12,14,20,28,44,72]
lines=['/* Barlow Condensed / SIL OFL1.1. Native-size2-bit antialiased glyphs. */','#pragma once']
for index,size in enumerate(SIZES):
 font=ImageFont.truetype(str(ROOT/f"assets/BarlowCondensed-{'SemiBold' if index<2 else 'BoldItalic'}.ttf"),size);top=font.getbbox('A')[1];widths=[];offsets=[];data=[]
 allowed=set(chr(c) for c in range(32,128)) if index<4 else set(chr(c) for c in range(32,96)) if index==4 else set(' 0123456789%.-+PLAY') if index==5 else set(' SABCD')
 for code in range(32,128):
  ch=chr(code);width=max(1,round(font.getlength(ch)));widths.append(width);offsets.append(len(data))
  if ch not in allowed:continue
  im=Image.new('L',(width,size));ImageDraw.Draw(im).text((0,-top),ch,font=font,fill=255);values=[min(3,(v+42)//85) for v in im.getdata()]
  for i in range(0,len(values),4):data.append(sum(v<<(2*j) for j,v in enumerate(values[i:i+4])))
 lines.append(f'static const uint8_t CQ_U{index}_W[96] = '+'{'+','.join(map(str,widths))+'};')
 lines.append(f'static const uint16_t CQ_U{index}_O[96] = '+'{'+','.join(map(str,offsets))+'};')
 lines.append(f'static const uint8_t CQ_U{index}_B[{len(data)}] = '+'{'+','.join(map(str,data))+'};')
lines+=['typedef struct {uint8_t h;const uint8_t *width;const uint16_t *offset;const uint8_t *bits;} cq_ui_font_t;',f'static const cq_ui_font_t CQ_UI_FONTS[{len(SIZES)}] = {{']
for i,size in enumerate(SIZES):lines.append(f' {{{size},CQ_U{i}_W,CQ_U{i}_O,CQ_U{i}_B}},')
lines+=['};'];(ROOT/'firmware/gen/cq_ui_font.h').write_text('\n'.join(lines)+'\n')
