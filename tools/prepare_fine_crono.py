#!/usr/bin/env python3
"""Re-sample original high-resolution art at native~90px stature, not an enlarged37px sprite."""
from pathlib import Path
from PIL import Image
import statistics,json,shutil
ROOT=Path(__file__).resolve().parents[1];dst=ROOT/'projects/peaceful-days';src=ROOT/'assets/chrono-trigger-source'
def cells(file,cols,rows):
 im=Image.open(src/file).convert('RGBA');w,h=im.size;out=[]
 for y in range(rows):
  for x in range(cols):
   c=im.crop((round(x*w/cols),round(y*h/rows),round((x+1)*w/cols),round((y+1)*h/rows))).resize((512,512),Image.Resampling.NEAREST);c.putalpha(c.getchannel('A').point(lambda a:255 if a>=225 else 0));out.append(c)
 return out
run=cells('run-enemy.png',4,2);attack=cells('attack.png',3,2);poses=[];report=[]
for name,group in [('run',run[:6]),('attack',attack)]:
 bounds=[c.getchannel('A').getbbox() for c in group];scale=90/statistics.median(b[3]-b[1] for b in bounds);foot=statistics.median(b[3] for b in bounds)
 for i,c in enumerate(group):
  hair=[x for y in range(20,280) for x in range(120,440) if(lambda p:p[3]>220 and p[0]>100 and p[0]>p[1]*1.5 and p[0]>p[2]*1.8)(c.getpixel((x,y)))];head=statistics.median(hair);left=head-48/scale;top=foot-105/scale;size=120/scale
  n=c.crop((round(left),round(top),round(left+size),round(top+size))).resize((120,120),Image.Resampling.NEAREST);b=n.getchannel('A').getbbox();l,t,r,bot=b;bot=max(bot,105);r=max(r,48);trim=n.crop((l,t,r,bot));poses.append({'id':f'crono.{name}.{i}','image':trim,'pivot':[48-l,105-t]});report.append({'frame':f'{name}.{i}','scale':scale,'pivot':[48,105],'bounds':b,'nativeStature':90})
# Fixed authored pose rows balance the wide slash with narrow running poses.
rows=[[9,0,1],[10,8,2],[3,4,5],[6,7,11]];sheets=[Image.new('RGBA',(240,220)),Image.new('RGBA',(240,220))];heights=[0,0];frames=[]
for row,indices in enumerate(rows):
 sheet=row//2;x=0;y=heights[sheet];rh=max(poses[i]['image'].height for i in indices)
 for i in indices:
  f=poses[i];w,h=f['image'].size
  if x+w>240:raise ValueError(f'row too wide: {row} {x+w}')
  sheets[sheet].alpha_composite(f['image'],(x,y));frames.append({'id':f['id'],'image':f'crono.fine.{sheet}','rect':[x,y,w,h],'pivot':f['pivot']});x+=w
 heights[sheet]+=rh
for i,s in enumerate(sheets):s.crop((0,0,240,heights[i])).save(dst/f'art/crono-fine-{i}.png')
enemy=Image.new('RGBA',(120,60))
for i,c in enumerate(run[6:]):
 b=c.getchannel('A').getbbox();cx=(b[0]+b[2])/2;n=c.crop((round(cx-150),b[3]-300,round(cx+150),b[3])).resize((60,60),Image.Resampling.NEAREST);enemy.alpha_composite(n,(i*60,0));frames.append({'id':f'imp.{i}','image':'imp.fine','rect':[i*60,0,60,60],'pivot':[30,60]})
enemy.save(dst/'art/imp-fine.png')
afile=dst/'art/art.json';a=json.loads(afile.read_text());a['images']=[i for i in a['images'] if i['id'] not in ('crono.sheet','imp.sheet','crono.fine.0','crono.fine.1','imp.fine')]+[{'id':f'crono.fine.{i}','path':f'art/crono-fine-{i}.png','encoding':'indexed4'} for i in range(2)]+[{'id':'imp.fine','path':'art/imp-fine.png','encoding':'indexed4'}]
for im in a['images']:
 if im['id'] in ('far','village','cover'):im['encoding']='indexed8'
 elif im['id'] in ('foreground','ground'):im['encoding']='indexed4'
a['frames']=frames;afile.write_text(json.dumps(a,indent=2)+'\n');(dst/'art/animation-registration.json').write_text(json.dumps(report,indent=2)+'\n')
source=src/'near-houses-source.png'
if not source.exists():shutil.copyfile('/Users/nason/.codex/generated_images/01a1bddb-ac3c-7cd1-b80b-52153dced1fa/exec-951d15ec-4988-4567-a92f-db14f5ef485e.png',source)
house=Image.open(source).convert('RGBA');w,h=house.size;side=min(w,h);left=(w-side)//2;house=house.crop((left,0,left+side,side)).resize((240,240),Image.Resampling.NEAREST).crop((0,48,240,216));house.putalpha(house.getchannel('A').point(lambda a:255 if a>=220 else 0));house.save(dst/'art/village.png')
p=dst/'scene/scene.json';s=json.loads(p.read_text());s['layers'][1]['y']=28;s['layers'][1]['height']=168;p.write_text(json.dumps(s,indent=2)+'\n')
print('Native90px Crono, native60px enemies, life-size cottage layer; atlases',heights)
