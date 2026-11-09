#!/usr/bin/env python3
"""Pack generated scrolling layers and trim transparent sprite margins without moving anchors."""
from pathlib import Path
from PIL import Image
import json
ROOT=Path(__file__).resolve().parents[1];dst=ROOT/'projects/peaceful-days';im=Image.open(ROOT/'assets/chrono-trigger-source/parallax-source.png').convert('RGBA')
# Split at the actual generated strip boundaries, then sample at the runtime dimensions.
far=im.crop((0,0,1536,367));base=Image.new('RGBA',far.size,(165,224,225,255));base.alpha_composite(far);left=(1536-474)//2;base.crop((left,0,left+474,367)).convert('RGB').resize((80,62),Image.Resampling.NEAREST).resize((240,62),Image.Resampling.NEAREST).save(dst/'art/far.png')
for file,box,size in [('village.png',(0,367,1536,736),(240,72)),('foreground.png',(0,896,1536,1024),(240,16))]:
 c=im.crop(box).resize(size,Image.Resampling.NEAREST);c.putalpha(c.getchannel('A').point(lambda v:255 if v>=220 else 0));c.save(dst/'art'/file)
afile=dst/'art/art.json';a=json.loads(afile.read_text())
a['images']=[i for i in a['images'] if i['id'] not in ('village','far','foreground')]+[{'id':'far','path':'art/far.png'},{'id':'village','path':'art/village.png'},{'id':'foreground','path':'art/foreground.png'}]
# Save original registered atlas so this operation is idempotent and fully reversible.
original=dst/'art/crono-untrimmed-source.png'
if not original.exists():Image.open(dst/'art/crono.png').save(original)
source=Image.open(original).convert('RGBA');atlas=Image.new('RGBA',(224,80));x=y=rowh=0
for f in a['frames']:
 if f['image']!='crono.sheet':continue
 # Source logical48px grid inferred from frame identity, not the previously trimmed rect.
 name=f['id'].split('.');index=int(name[-1])+(6 if name[1]=='attack' else 0);c=source.crop(((index%4)*48,(index//4)*48,(index%4+1)*48,(index//4+1)*48));b=c.getchannel('A').getbbox();left,top,right,bottom=b;right=max(right,20);bottom=max(bottom,43)
 crop=c.crop((left,top,right,bottom));w,h=crop.size
 if x+w>224:x=0;y+=rowh;rowh=0
 if y+h>80:raise ValueError('trimmed atlas overflow')
 atlas.alpha_composite(crop,(x,y));f['rect']=[x,y,w,h];f['pivot']=[20-left,43-top]
 # Verify rendering at the same foot anchor reconstructs exactly the original pixels.
 check=Image.new('RGBA',(48,48));check.alpha_composite(crop,(left,top));assert check.tobytes()==c.tobytes()
 x+=w;rowh=max(rowh,h)
atlas.crop((0,0,224,y+rowh)).save(dst/'art/crono.png');afile.write_text(json.dumps(a,indent=2)+'\n')
p=dst/'scene/scene.json';s=json.loads(p.read_text());s['parallax']=True;s['layers']=[{'id':'far','image':'far','y':84,'height':130,'scrollRatio':0.10,'tileX':True},{'id':'village','image':'village','y':124,'height':72,'scrollRatio':0.35,'tileX':True},{'id':'near','image':'foreground','y':182,'height':16,'scrollRatio':0.75,'tileX':True,'foreground':True},{'id':'floor','image':'ground','y':196,'scrollRatio':1,'tileX':True}];p.write_text(json.dumps(s,indent=2)+'\n')
print('Prepared3 independent scrolling layers and pixel-identical cropped sprites; atlas',Image.open(dst/'art/crono.png').size)
