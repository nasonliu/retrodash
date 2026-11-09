#!/usr/bin/env python3
"""Register animation with ONE scale and a fixed head/ground coordinate system."""
from pathlib import Path
from PIL import Image
import json
ROOT=Path(__file__).resolve().parents[1]
project=ROOT/'projects/pixel-street'
im=Image.open(ROOT/'assets/chipquest-source/courier-six-frames.png').convert('RGBA')
sheets=[Image.new('RGBA',(180,50)),Image.new('RGBA',(180,50))]
report=[]
for i in range(6):
 c=im.crop(((i%3)*512,(i//3)*512,(i%3+1)*512,(i//3+1)*512))
 # Registration landmark: the cream cap top, not the full pose/weapon bounds.
 points=[x for y in range(95,155) for x in range(160,400) if (lambda p:p[3]>180 and p[0]>205 and p[1]>185 and p[2]>150)(c.getpixel((x,y)))]
 if not points:raise ValueError('missing head registration landmark')
 center=round(sum(points)/len(points));left=center-240
 # All poses use exactly 0.1 scale, same vertical crop and same output pivot.
 c=c.crop((left,80,left+480,480)).resize((60,50),Image.Resampling.NEAREST)
 alpha=c.getchannel('A').point(lambda v:255 if v>=180 else 0);c.putalpha(alpha)
 sheets[i//3].alpha_composite(c,((i%3)*60,0))
 report.append({'frame':i,'scale':0.125,'sourceHeadX':center,'pivot':[30,43],'visibleBounds':alpha.getbbox()})
for i,sheet in enumerate(sheets):sheet.save(project/f'art/hero-{i}.png')
# Six consecutive attack poses share the run animation's scale and pivot.
attack=Image.open(ROOT/'assets/chipquest-source/courier-attack-six-frames.png').convert('RGBA')
attack_sheet=Image.new('RGBA',(180,100))
for i in range(6):
 c=attack.crop(((i%3)*512,(i//3)*512,(i%3+1)*512,(i//3+1)*512))
 points=[x for y in range(95,155) for x in range(160,400) if (lambda p:p[3]>180 and p[0]>205 and p[1]>185 and p[2]>150)(c.getpixel((x,y)))]
 if not points:raise ValueError('missing attack head registration landmark')
 center=round(sum(points)/len(points))
 c=c.crop((center-240,80,center+240,480)).resize((60,50),Image.Resampling.NEAREST)
 alpha=c.getchannel('A').point(lambda v:255 if v>=180 else 0);c.putalpha(alpha)
 attack_sheet.alpha_composite(c,((i%3)*60,(i//3)*50))
 report.append({'frame':f'attack.{i}','scale':0.125,'sourceHeadX':center,'pivot':[30,43],'visibleBounds':alpha.getbbox()})
attack_sheet.save(project/'art/hero-attack.png')
e=Image.open(ROOT/'assets/chipquest-source/speaker-two-frames.png').convert('RGBA');out=Image.new('RGBA',(48,24))
for i in range(2):
 c=e.crop((i*887,0,(i+1)*887,887));bbox=c.getchannel('A').point(lambda v:255 if v>=180 else 0).getbbox();center=(bbox[0]+bbox[2])//2
 c=c.crop((center-225,250,center+225,700)).resize((24,24),Image.Resampling.NEAREST);c.putalpha(c.getchannel('A').point(lambda v:255 if v>=180 else 0));out.alpha_composite(c,(i*24,0))
out.save(project/'art/enemies.png')
afile=project/'art/art.json';a=json.loads(afile.read_text());a['images']=[x for x in a['images'] if not x['id'].startswith('hero.sheet')]+[{'id':f'hero.sheet.{i}','path':f'art/hero-{i}.png'} for i in range(2)]+[{'id':'hero.sheet.attack','path':'art/hero-attack.png'}]
a['frames']=[x for x in a['frames'] if not x['id'].startswith('hero.')]+[{'id':f'hero.{i}','image':f'hero.sheet.{i//3}','rect':[(i%3)*60,0,60,50],'pivot':[30,43]} for i in range(6)]+[{'id':f'hero.attack.{i}','image':'hero.sheet.attack','rect':[(i%3)*60,(i//3)*50,60,50],'pivot':[30,43]} for i in range(6)]
for clip in a['clips']:
 if clip['id']=='courier.run':clip['frames']=[{'frame':f'hero.{i}','durationMs':95} for i in range(4)]
 elif clip['id']=='courier.attack':clip['frames']=[{'frame':f'hero.attack.{i}','durationMs':50} for i in range(6)]
 elif clip['id']=='courier.miss':clip['frames']=[{'frame':'hero.5','durationMs':140}]
 elif clip['id']=='courier.hold':clip['frames']=[{'frame':'hero.4','durationMs':100}]
 else:
  for st in clip['frames']:
   if st['frame'].startswith('hero.'):st['frame']='hero.0'
afile.write_text(json.dumps(a,indent=2)+'\n');(project/'art/animation-registration.json').write_text(json.dumps(report,indent=2)+'\n')
print('twelve poses registered with shared 0.125 scale and pivot (30,43)')
