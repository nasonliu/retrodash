#!/usr/bin/env python3
"""Prepare the selected ImageGen atlas at native FM-1 pixel dimensions."""
import json
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'projects/ff6-terra-snow'; out=p/'art'
src=Image.open(out/'source-atlas.png').convert('RGBA')

def keyed(im):
 im=im.copy();im.putdata([(0,0,0,0) if r>165 and b>140 and g<90 else (r,g,b,255) for r,g,b,a in im.getdata()]);return im
images=[];frames=[];clips=[]
def image(id,im,group=None,encoding='indexed4'):
 im.save(out/(id+'.png'));item=dict(id=id,path='art/'+id+'.png',encoding=encoding)
 if group:item['paletteGroup']=group
 images.append(item)
def sprite(id,crop,base,size,pivot):
 im=keyed(src.crop(crop)); bbox=im.getbbox()
 # All cells share one scale and fixed source baseline; trim only keyed margins.
 if bbox:
  trim=im.crop(bbox);trim=trim.resize((max(1,round(trim.width*base)),max(1,round(trim.height*base))),Image.Resampling.NEAREST)
  canvas=Image.new('RGBA',size);x=round(bbox[0]*base);y=size[1]-round((crop[3]-crop[1]-bbox[1])*base)
  canvas.alpha_composite(trim,(x,y))
 else:raise ValueError(id)
 if id.startswith('terra'):
  # Reserve greens and reds so a bronze-heavy atlas cannot erase Terra's identity.
  colors=['151522','403a31','76674b','b4a077','e7d2a0','333c43','717780','b4bbc4','1f4a37','3b8059','83b087','78333a','cf6261','d59e86','f4d2ac']
  rgb=[tuple(bytes.fromhex(c)) for c in colors];pal=Image.new('P',(1,1));pal.putpalette([v for c in (rgb+[rgb[0]]*(256-len(rgb))) for v in c])
  alpha=canvas.getchannel('A');canvas=canvas.convert('RGB').quantize(palette=pal,dither=Image.Dither.NONE).convert('RGBA');canvas.putalpha(alpha)
  canvas.putdata([(*rgb[0],0) if px[3]<128 else px for px in canvas.getdata()])
 image(id,canvas,'terra' if id.startswith('terra') else 'imperial')
 frames.append(dict(id=id,image=id,rect=[0,0,*size],pivot=pivot))
for i in range(6):sprite('terra.'+str(i),(i*256+16,8,(i+1)*256,236),.42,(112,98),[40,98])
for i in range(3):sprite('mech.'+str(i),(i*272+24,264,(i+1)*272-4,492),.35,(88,80),[44,80])
for i in range(3):sprite('soldier.'+str(i),(854+i*224,348,1030+i*224,492),.35,(62,52),[31,52])
def clip(id,ids,ms,loop=True):clips.append(dict(id=id,loop=loop,frames=[dict(frame=x,durationMs=ms)for x in ids]))
clip('terra.run',['terra.'+str(i)for i in [0,1,2,3,2,1]],116)
clip('terra.attack',['terra.'+str(i)for i in [0,4,5,4,0]],55,False)
clip('terra.miss',['terra.'+str(i)for i in [5,0,5,0]],70,False)
clip('terra.hold',['terra.4','terra.0'],160)
clip('terra.clear',['terra.0','terra.3'],220)
clip('mech.move',['mech.'+str(i)for i in [0,1,2,1]],145)
clip('soldier.move',['soldier.'+str(i)for i in [0,1,2,1]],145)
# Source panorama remains at native output size; opaque far fills every stage row.
far=src.crop((0,514,1536,766)).resize((240,112),Image.Resampling.NEAREST)
bg=Image.new('RGBA',(240,186),(17,29,58,255));bg.alpha_composite(far,(0,0))
road=src.crop((0,770,1536,1022)).resize((240,86),Image.Resampling.NEAREST)
bg.alpha_composite(road,(0,100));image('far',bg,'scene','indexed8')
# Distant skyline and nearby ridges have different scroll rates.
mid=Image.new('RGBA',(240,168));ridge=src.crop((0,846,1536,1022)).resize((240,46),Image.Resampling.NEAREST);mid.alpha_composite(ridge,(0,122));image('village',mid,'scene','indexed8')
near=src.crop((0,948,1536,1022)).resize((240,16),Image.Resampling.NEAREST);image('foreground',near)
ground=src.crop((0,964,1536,1022)).resize((240,18),Image.Resampling.NEAREST);image('ground',ground)
cover=bg.resize((240,240),Image.Resampling.NEAREST);hero=Image.open(out/'terra.0.png').convert('RGBA');cover.alpha_composite(hero,(52,130));image('cover',cover,encoding='indexed4')
art=dict(images=images,frames=frames,clips=clips,actors=[dict(id='terra-magitek',actions={a:'terra.'+a for a in ['run','attack','miss','hold','clear']}),dict(id='imperial-magitek',actions={'move':'mech.move'}),dict(id='imperial-soldier',actions={'move':'soldier.move'})])
(out/'art.json').write_text(json.dumps(art,indent=2)+'\n')
scene=dict(theme=dict(background='#111D3A',text='#FFF5DD',target='#FFEF19',nextOutline='#FF36A7',miss='#FF7C70'),heroActor='terra-magitek',layers=[dict(id='far',image='far',y=28,height=186,scrollRatio=.1,tileX=True),dict(id='snowfield',image='village',y=28,height=168,scrollRatio=.35,tileX=True),dict(id='near',image='foreground',y=182,height=16,scrollRatio=.75,tileX=True,foreground=True),dict(id='floor',image='ground',y=196,scrollRatio=1,tileX=True)],targetSkins=[dict(id='magitek',actor='imperial-magitek'),dict(id='infantry',actor='imperial-soldier')],defaultTargetSkin='magitek',events=[],approachMs=2600,parallax=True,weather='blizzard')
(p/'scene/scene.json').write_text(json.dumps(scene,indent=2)+'\n')
# Enlarged contact sheet is for inspecting consistent actor baselines only.
sheet=Image.new('RGBA',(112*6,160),(17,29,58,255))
for i in range(6):sheet.alpha_composite(Image.open(out/('terra.'+str(i)+'.png')),(112*i,0))
for i in range(3):
 sheet.alpha_composite(Image.open(out/('mech.'+str(i)+'.png')),(88*i,80))
 sheet.alpha_composite(Image.open(out/('soldier.'+str(i)+'.png')),(300+80*i,108))
sheet.save(ROOT/'build/ff6-reference/actors.png')
print('FF6 artwork prepared:',len(images),'images',len(frames),'frames')
