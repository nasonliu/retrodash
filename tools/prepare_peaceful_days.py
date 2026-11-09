#!/usr/bin/env python3
"""Register generated Chrono fan-art using one scale per animation batch, fixed foot anchor."""
from pathlib import Path
from PIL import Image
import json,statistics,shutil
ROOT=Path(__file__).resolve().parents[1];src=ROOT/'assets/chrono-trigger-source';dst=ROOT/'projects/peaceful-days'
files={'run-enemy.png':'exec-a9cd42e5-f9c7-4fc9-8c38-d59e2e419223.png','attack.png':'exec-5338f787-fe06-47c5-afea-cdb4992fc1a4.png','village.png':'exec-57372887-5803-4a20-848a-c8f5c86483a5.png','cover.png':'exec-11e22535-8b30-4b09-a9c8-20192223a3ce.png'}
gen=Path('/Users/nason/.codex/generated_images/01a1bddb-ac3c-7cd1-b80b-52153dced1fa')
for name,source in files.items():
 if not (src/name).exists():shutil.copyfile(gen/source,src/name)
def split(file,cols,rows):
 im=Image.open(src/file).convert('RGBA');w,h=im.size
 out=[]
 for y in range(rows):
  for x in range(cols):
   c=im.crop((round(x*w/cols),round(y*h/rows),round((x+1)*w/cols),round((y+1)*h/rows))).resize((512,512),Image.Resampling.NEAREST)
   c.putalpha(c.getchannel('A').point(lambda a:255 if a>=225 else 0));out.append(c)
 return out
run=split('run-enemy.png',4,2);attack=split('attack.png',3,2);sheet=Image.new('RGBA',(192,144));frames=[];report=[]
for name,cells in [('run',run[:6]),('attack',attack)]:
 bounds=[c.getchannel('A').getbbox() for c in cells];height=statistics.median(b[3]-b[1] for b in bounds);scale=37/height;foot=statistics.median(b[3] for b in bounds)
 for i,(c,bounds_) in enumerate(zip(cells,bounds)):
  # Red hair is the head landmark; weapon/scarf bounds never determine horizontal anchor.
  hair=[(x,y) for y in range(20,280) for x in range(120,440) if (lambda p:p[3]>220 and p[0]>100 and p[0]>p[1]*1.5 and p[0]>p[2]*1.8)(c.getpixel((x,y)))]
  if not hair:raise ValueError('missing red hair landmark')
  head=statistics.median(x for x,y in hair)
  left=head-20/scale;top=foot-43/scale;size=48/scale
  native=c.crop((round(left),round(top),round(left+size),round(top+size))).resize((48,48),Image.Resampling.NEAREST)
  n=len(frames);sheet.alpha_composite(native,((n%4)*48,(n//4)*48));frames.append({'id':f'crono.{name}.{i}','image':'crono.sheet','rect':[(n%4)*48,(n//4)*48,48,48],'pivot':[20,43]})
  report.append({'frame':f'{name}.{i}','scale':scale,'headLandmark':head,'footBaseline':foot,'pivot':[20,43],'bounds':native.getchannel('A').getbbox()})
sheet.save(dst/'art/crono.png')
enemy=Image.new('RGBA',(48,24))
for i,c in enumerate(run[6:]):
 b=c.getchannel('A').getbbox();cx=(b[0]+b[2])/2;size=300
 c=c.crop((round(cx-size/2),round(b[3]-size),round(cx+size/2),b[3])).resize((24,24),Image.Resampling.NEAREST)
 enemy.alpha_composite(c,(i*24,0));frames.append({'id':f'imp.{i}','image':'imp.sheet','rect':[i*24,0,24,24],'pivot':[12,24]})
enemy.save(dst/'art/imps.png')
bg=Image.open(src/'village.png').convert('RGB');bg.resize((240,128),Image.Resampling.NEAREST).save(dst/'art/background.png')
# A native four-to-one ground tile sampled from the generated path.
w,h=bg.size;bg.crop((w//3,h-80,w//3+320,h)).resize((48,12),Image.Resampling.NEAREST).save(dst/'art/ground.png')
Image.open(src/'cover.png').convert('RGB').resize((112,112),Image.Resampling.NEAREST).save(dst/'art/cover.png')
clips=[{'id':'crono.run','loop':True,'frames':[{'frame':f'crono.run.{i}','durationMs':58} for i in range(6)]},{'id':'crono.attack','loop':False,'frames':[{'frame':f'crono.attack.{i}','durationMs':50} for i in range(6)]},{'id':'crono.miss','loop':False,'frames':[{'frame':'crono.attack.0','durationMs':170}]},{'id':'crono.hold','loop':True,'frames':[{'frame':'crono.attack.1','durationMs':160},{'frame':'crono.attack.2','durationMs':160}]},{'id':'crono.clear','loop':True,'frames':[{'frame':'crono.run.0','durationMs':200}]},{'id':'imp.move','loop':True,'frames':[{'frame':f'imp.{i}','durationMs':150} for i in range(2)]}]
a={'images':[{'id':'crono.sheet','path':'art/crono.png'},{'id':'imp.sheet','path':'art/imps.png'},{'id':'village','path':'art/background.png'},{'id':'ground','path':'art/ground.png'},{'id':'cover','path':'art/cover.png'}],'frames':frames,'clips':clips,'actors':[{'id':'crono','actions':{k:'crono.'+k for k in ['run','attack','miss','hold','clear']}},{'id':'blue-imp','actions':{'move':'imp.move'}}]}
(dst/'art/art.json').write_text(json.dumps(a,indent=2)+'\n');(dst/'art/animation-registration.json').write_text(json.dumps(report,indent=2)+'\n')
scene={'theme':{'background':'#180C29','text':'#FFF5DD','target':'#FFEF19','nextOutline':'#FF36A7','miss':'#FF7C70'},'heroActor':'crono','layers':[{'id':'far','image':'village','y':28,'scrollRatio':0,'tileX':False},{'id':'floor','image':'ground','y':144,'scrollRatio':1,'tileX':True}],'targetSkins':[{'id':'imp','actor':'blue-imp'}],'defaultTargetSkin':'imp','events':[],'approachMs':2400}
(dst/'scene/scene.json').write_text(json.dumps(scene,indent=2)+'\n')
print('Registered native Crono atlas',sheet.size,'6 run + 6 attack; anchors (20,43); scales',sorted({r['scale'] for r in report}))
for r in report:print(r['frame'],r['bounds'])
