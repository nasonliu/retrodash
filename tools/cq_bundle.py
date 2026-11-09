#!/usr/bin/env python3
"""Compile a v2 .cqproj directory/ZIP into a validated CQP2 song-and-art bundle."""
import argparse, contextlib, json, re, struct, tempfile, zipfile, zlib
from pathlib import Path
from PIL import Image
from mido import MidiFile
from cq_compile import compile_song
HEADER=160

@contextlib.contextmanager
def source(path):
    path=Path(path)
    if path.is_dir(): yield path.resolve(); return
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        with zipfile.ZipFile(path) as z:
            if sum(i.file_size for i in z.infolist())>16*1024*1024: raise ValueError('source exceeds 16 MiB')
            names=z.namelist()
            if len(set(names))!=len(names): raise ValueError('duplicate ZIP entry')
            for i in z.infolist():
                p=Path(i.filename)
                if p.is_absolute() or '..' in p.parts or (i.external_attr>>16)&0o170000==0o120000: raise ValueError('unsafe ZIP path')
            z.extractall(root)
        yield root

def build(root, song_data=None, compact=False):
    root=Path(root).resolve()
    def path(name):
        p=(root/name).resolve()
        if not p.is_relative_to(root) or not p.is_file(): raise ValueError(f'missing or unsafe file: {name}')
        return p
    def js(name):return json.loads(path(name).read_text())
    m=js('manifest.json')
    if song_data is None:
        if m.get('format')!='chipquest-project' or m.get('version')!=2: raise ValueError('expected source format version 2')
    elif m.get('format')!='retrodash-spc-project' or m.get('version')!=1:
        raise ValueError('expected an SPC source project')
    if not re.fullmatch(r'[A-Za-z0-9_.-]{1,48}',m['id']):raise ValueError('project ID must be 1-48 ASCII filename characters')
    chart=js(m['chart']);art=js(m['art']);scene=js(m['scene']);path(m['credits'])
    if song_data is None and (chart['keyMapping']!='fm1-fixed-midi-53-79' or chart['chordSelection']['default']!='lowest'): raise ValueError('unsupported chart mapping')
    if chart.get('noteAppearanceOverrides') or scene.get('events'): raise ValueError('scene events/individual skins are not implemented in CQP2.0')
    if song_data is None:
        roots={int(x['tick']):int(x['pitch']) for x in chart['chordSelection'].get('overrides',[])}
        song_data=compile_song(MidiFile(path(m['music'])),m['title'],'project.mid',chart['melodyTrack'],roots,chart)
    C,B,end,bpm,fpt,hash_,mapping=song_data
    def ids(items):
        result={v['id']:i for i,v in enumerate(items)}
        if len(result)!=len(items): raise ValueError('duplicate resource ID')
        return result
    images=art['images'];frames=art['frames'];clips=art['clips'];actors=art['actors']
    imap,fmap,cmap,amap=map(ids,[images,frames,clips,actors])
    if not 1<=len(images)<=32 or not 1<=len(frames)<=256 or not 1<=len(clips)<=64: raise ValueError('resource count exceeded')
    buf=bytearray(HEADER)
    def add(data):
        while len(buf)%4:buf.append(0)
        off=len(buf);buf.extend(data);return off
    extension='.spc' if m['format']=='retrodash-spc-project' else '.mid'
    filename_text=m['id'] if m['id'].endswith(extension) else m['id']+extension
    title=add(m['title'].encode('ascii')+b'\0');filename=add(filename_text.encode('ascii')+b'\0')
    co=add(b''.join(struct.pack('<HHBBbB',*v) for v in C));bo=add(b''.join(struct.pack('<HHBBBB',*v) for v in B))
    imgrows=[];shared_palettes={}
    for item in images:
        group=item.get('paletteGroup')
        if group and group not in shared_palettes:
            members=[Image.open(path(i['path'])).convert('RGB') for i in images if i.get('paletteGroup')==group]
            sample=Image.new('RGB',(max(i.width for i in members),sum(i.height for i in members)));y=0
            for member in members:sample.paste(member,(0,y));y+=member.height
            shared_palettes[group]=sample.quantize(colors=255 if item.get('encoding')=='indexed8' else 15,method=Image.Quantize.MEDIANCUT)
    for item in images:
        with Image.open(path(item['path'])) as raw:
            w,h=raw.size
            if not (1<=w<=240 and 1<=h<=240): raise ValueError(f"image dimensions: {item['id']}")
            rgba=raw.convert('RGBA');encoding=item.get('encoding','rgb565');transparent=any(a<128 for *_,a in rgba.getdata())
            def rgb565(r,g,b):
                v=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
                return 0xf81e if v==0xf81f else v
            if encoding in ('indexed4','indexed8'):
                colors=16 if encoding=='indexed4' else 256;quant=rgba.convert('RGB').quantize(palette=shared_palettes[item['paletteGroup']],dither=Image.Dither.NONE) if item.get('paletteGroup') else rgba.convert('RGB').quantize(colors=colors-1,method=Image.Quantize.MEDIANCUT);pal=quant.getpalette();entries=[0xf81f]+[rgb565(*pal[i*3:i*3+3]) for i in range(colors-1)];indices=[0 if px[3]<128 else index+1 for px,index in zip(rgba.getdata(),quant.getdata())]
                data=bytearray(b''.join(struct.pack('<H',v) for v in entries))
                if colors==16:
                    for i in range(0,len(indices),2):data.append(indices[i]|((indices[i+1] if i+1<len(indices) else 0)<<4))
                else:data.extend(indices)
                flags=int(transparent)|(2 if colors==16 else 4)
            elif encoding=='rgb565':
                data=bytearray()
                for r,g,b,a in rgba.getdata():data.extend(struct.pack('<H',0xf81f if a<128 else rgb565(r,g,b)))
                flags=int(transparent)
            else:raise ValueError('unsupported image encoding')
            if compact and flags&6:
                import lz4blk
                palette_bytes=32 if flags&2 else 512
                packed=lz4blk.compress(bytes(data[palette_bytes:]))
                if len(packed)<len(data)-palette_bytes:
                    data=data[:palette_bytes]+packed;flags|=8
        off=add(data);imgrows.append((w,h,off,len(data),flags))
    io=add(b''.join(struct.pack('<HHIII',*v) for v in imgrows))
    frows=[]
    for f in frames:
        image=imap[f['image']];x,y,w,h=f['rect'];px,py=f['pivot'];iw,ih=imgrows[image][:2]
        if min(x,y)<0 or min(w,h)<=0 or x+w>iw or y+h>ih or not (0<=px<=w and 0<=py<=h): raise ValueError(f"invalid frame: {f['id']}")
        frows.append((image,x,y,w,h,px,py))
    fo=add(b''.join(struct.pack('<I6H',*v) for v in frows))
    steps=[];crows=[]
    for c in clips:
        first=len(steps)
        for st in c['frames']:
            dur=st['durationMs']
            if not isinstance(dur,int) or not 1<=dur<=2000: raise ValueError('frame duration out of range')
            steps.append((fmap[st['frame']],dur))
        if not len(steps)>first: raise ValueError('empty animation')
        crows.append((first,len(steps)-first,int(c['loop'])))
    clo=add(b''.join(struct.pack('<IHH',*v) for v in crows));sto=add(b''.join(struct.pack('<HH',*v) for v in steps))
    hero=actors[amap[scene['heroActor']]]['actions'];skins={s['id']:s for s in scene['targetSkins']};enemy=actors[amap[skins[scene['defaultTargetSkin']]['actor']]]['actions']
    for actor in actors:
        for clip_id in actor['actions'].values():
            if clip_id not in cmap:raise ValueError(f'missing animation: {clip_id}')
    if len(steps)>4096:raise ValueError('too many animation steps')
    roles=[cmap[hero[k]] for k in ['run','attack','miss','hold','clear']]+[cmap[enemy['move']]]
    layers=scene['layers'];parallax=bool(scene.get('parallax'));layerrows=[];layeroff=0
    if parallax:
        if len(layers)!=4:raise ValueError('parallax requires3 backdrop layers and ground')
        pixels=0
        for i,l in enumerate(layers[:-1]):
            image=imap[l['image']];w,h=imgrows[image][:2];y=l['y'];dh=l['height'];speed=l['scrollRatio'];flags=int(bool(l.get('foreground')))
            if w!=240 or not 28<=y<214 or not 1<=dh<=186 or y+dh>214 or not 0<=speed<=1 or not l['tileX']:raise ValueError('invalid parallax layer')
            if i==0 and (y!=28 or dh!=186 or imgrows[image][4]&1 or flags):raise ValueError('far layer must cover the battle viewport opaquely')
            pixels+=w*h;layerrows.append((image,y,dh,round(speed*256),flags))
        if pixels>180000:raise ValueError('parallax source image budget exceeded')
        layeroff=add(b''.join(struct.pack('<IHHII',*v) for v in layerrows))
        bg=imap[layers[0]['image']];ground=imap[layers[-1]['image']];floor=layers[-1]['y']
        if floor!=196:raise ValueError('parallax ground must align to196')
    else:
        if len(layers)!=2 or layers[0]['tileX'] or not layers[1]['tileX'] or layers[0]['y']!=28 or layers[0]['scrollRatio']!=0 or layers[1]['scrollRatio']!=1:raise ValueError('legacy scene requires one240x128 backdrop and ground')
        bg=imap[layers[0]['image']];ground=imap[layers[1]['image']];floor=layers[1]['y']
        if imgrows[bg][:2]!=(240,128):raise ValueError('background must be240x128')
        if not 120<=floor<=156:raise ValueError('ground baseline outside stage')
    def color(key):
        value=scene['theme'][key]
        if not re.fullmatch(r'#[0-9a-fA-F]{6}',value):raise ValueError(f'invalid theme color: {key}')
        return int(value[1:],16)
    values=[2,0,0,title,filename,bpm,fpt,end,co,len(C),bo,len(B),io,len(images),fo,len(frames),clo,len(clips),sto,len(steps),*roles,bg,ground,*[color(k) for k in ['target','text','background','nextOutline','miss']],floor,hash_,imap[m['coverImage']],scene.get('approachMs',1800),layeroff,len(layerrows)]
    if not 1000<=scene.get('approachMs',1800)<=3000:raise ValueError('approachMs must be 1000..3000')
    buf[:4]=b'CQP2'
    for n,v in enumerate(values):struct.pack_into('<I',buf,4+n*4,v)
    enemy_clips=[cmap[actors[amap[skin['actor']]]['actions']['move']] for skin in scene['targetSkins']]
    weather=scene.get('weather')
    if weather not in (None,'blizzard'):raise ValueError('unsupported weather')
    if len(enemy_clips)>1 or weather:
        if not 1<=len(enemy_clips)<=16:raise ValueError('enemy skin count exceeded')
        add(struct.pack('<'+'I'*len(enemy_clips),*enemy_clips)+struct.pack('<II',int(weather=='blizzard'),len(enemy_clips))+b'RDX1')
    if len(buf)>180*1024:raise ValueError('runtime project exceeds 180 KiB demo budget')
    struct.pack_into('<I',buf,8,len(buf));struct.pack_into('<I',buf,12,zlib.crc32(buf[HEADER:])&0xffffffff)
    return bytes(buf),{'title':m['title'],'bpm':bpm/100,'approachMs':scene.get('approachMs',1800),'bytes':len(buf),'melodyNotes':len(C),'backingNotes':sum(not n[5] and n[2]!=0 for n in B),'assistNotes':sum(not n[5] and n[2]==0 for n in B),'backingEvents':len(B),'keyOctaveShift':chart.get('melodyKeyOctaveShift',0),'images':len(images),'frames':len(frames),'clips':len(clips),'keyMap':mapping,'imageEncoding':'RGB565 / indexed4 / indexed8; transparent palette index0','extraImageDecodeRAM':0,'parallaxLayers':len(layerrows),'sceneCacheBytes':89280}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('project',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--header',type=Path);a=p.parse_args()
    with source(a.project) as root:data,report=build(root)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(data)
    a.output.with_suffix('.report.json').write_text(json.dumps(report,indent=2)+'\n')
    if a.header:
        a.header.parent.mkdir(parents=True,exist_ok=True)
        a.header.write_text('/* Generated by cq_bundle.py; source project contains music and PNG assets. */\n#pragma once\nstatic const unsigned char CQ_DEMO_DATA[] __attribute__((aligned(4))) = {\n'+',\n'.join(' '+','.join(str(b) for b in data[i:i+32]) for i in range(0,len(data),32))+'\n};\n')
    print(json.dumps(report))
if __name__=='__main__':main()
