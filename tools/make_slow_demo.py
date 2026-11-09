#!/usr/bin/env python3
from pathlib import Path
import json
from mido import MidiFile,MidiTrack,MetaMessage,Message,bpm2tempo
root=Path(__file__).resolve().parents[1]/'projects/pixel-street'
m=MidiFile(ticks_per_beat=480);t=MidiTrack();m.tracks.append(t);t.append(MetaMessage('track_name',name='SLOW STREET',time=0));t.append(MetaMessage('set_tempo',tempo=bpm2tempo(72),time=0))
def add(name,ch,notes):
 t=MidiTrack();m.tracks.append(t);t.append(MetaMessage('track_name',name=name,time=0));ev=[]
 for beat,pitch,length,velocity in notes:
  ev += [(round(beat*480),0,Message('note_on',channel=ch,note=pitch,velocity=velocity)),(round((beat+length)*480),-1,Message('note_off',channel=ch,note=pitch,velocity=0))]
 last=0
 for at,_,msg in sorted(ev,key=lambda x:(x[0],x[1])):t.append(msg.copy(time=at-last));last=at
mel=[60,62,64,67,65,64,62,60,64,65,67,69,67,65,64,62,60,64,67,72,71,69,67,64,65,64,62,60,62,64,62,60]
add('MELODY',0,[(4+i,p,0.65,92) for i,p in enumerate(mel)])
add('BASS',1,[(i,48 if (i//4)%2==0 else 45,0.75,65) for i in range(40)])
add('HARMONY',2,[(b,p,3.5,45) for b in range(0,40,4) for p in ([60,64,67] if (b//4)%2==0 else [57,60,64])])
add('DRUMS',9,[(b,36 if b%4==0 else 38,0.15,60) for b in range(0,40,2)]+[(b,42,0.1,35) for b in range(40)])
m.save(root/'music/song.mid');p=root/'manifest.json';x=json.loads(p.read_text());x['title']='SLOW STREET';p.write_text(json.dumps(x,indent=2)+'\n')
p=root/'scene/scene.json';x=json.loads(p.read_text());x['approachMs']=1800;p.write_text(json.dumps(x,indent=2)+'\n')
print('SLOW STREET: 72 BPM, 32 notes, one melody note per beat')
