#!/usr/bin/env python3
"""Exercise the real FM-1 MIDI key input with AUTO disabled in a diagnostic build.

The device judges these incoming keys and reports its actual hit/fill counters.
This does not pretend to physically press the keyboard's switches.
"""
import argparse,json,time
from collections import deque
from pathlib import Path
import mido,rtmidi

p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--seconds',type=int,default=155);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
targets=json.loads((root/'projects/peaceful-days-spc/chart/chart.json').read_text())['targets']
events=[]
for n in targets:
 t=n['clock']/1024000+.008
 events.extend([(t,True,n['keyMidiPitch']),(t+.10,False,n['keyMidiPitch'])])
events.sort()
inp=rtmidi.MidiIn();inp.ignore_types(sysex=False,timing=False,active_sense=True)
name=next(n for n in inp.get_ports() if 'Felucca' in n);inp.open_port(inp.get_ports().index(name))
out=mido.open_output(next(n for n in mido.get_output_names() if 'Felucca' in n))
offsets=deque(maxlen=5);proofs=[];sent=[];index=0;end=time.monotonic()+a.seconds
try:
 while time.monotonic()<end:
  while True:
   item=inp.get_message()
   if item is None:break
   b=item[0]
   if len(b) in (28,33) and b[:4]==[0xf0,0x7d,0x52,0x44]:
    data=b[1:-1];clock=sum(v<<(i*7) for i,v in enumerate(data[10:15]));offsets.append(time.monotonic()-clock/1024000)
    proof=dict(clock=clock,second=clock/1024000,counts=[data[15+2*i]|data[16+2*i]<<7 for i in range(4)],maxCombo=data[23]|data[24]<<7,auto=data[25])
    proofs.append(proof)
    if int(proof['second'])%10==0:print(json.dumps(proof),flush=True)
  if offsets:
   current=time.monotonic()-min(offsets)
   while index<len(events) and events[index][0]<=current:
    t,on,pitch=events[index]
    if current-t>.12:raise RuntimeError('missed key-test deadline; do not inject stale notes')
    out.send(mido.Message('note_on' if on else 'note_off',channel=0,note=pitch,velocity=104 if on else 0))
    sent.append(dict(second=current,on=on,pitch=pitch,latenessMs=round((current-t)*1000,2)));index+=1
  if proofs and proofs[-1]['second']>=151:break
  time.sleep(.001)
finally:
 out.send(mido.Message('control_change',channel=0,control=123,value=0));out.close();inp.close_port()
result=dict(sentNoteOns=sum(e['on'] for e in sent),sentNoteOffs=sum(not e['on'] for e in sent),events=sent,deviceProofs=proofs)
a.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('events','deviceProofs')}),proofs[-1] if proofs else 'NO DEVICE PROOF',flush=True)
assert proofs and proofs[-1]['auto']==0,'AUTO must be disabled'
assert result['sentNoteOns']==len(targets) and proofs[-1]['maxCombo']==len(targets) and proofs[-1]['counts'][3]==0,'real device key-input check failed'
