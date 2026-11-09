#!/usr/bin/env python3
"""Record temporary SPC diagnostic CC telemetry; values are from the FM-1."""
import argparse,json,time,statistics
from pathlib import Path
import mido,rtmidi
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--seconds',type=int,default=30);a=p.parse_args()
fields=['fps','averageDrawMs','maxDrawMs','maxFrameGapMs','averagePaintMs','averageTransferMs','lcdTimeouts',
        'averageAudio100us','maxAudio100us','audioDeadlineMisses','spcError','manualHitsModulo128',
        'suppressedModulo128','automaticFillsModulo128','spcSecondsModulo128','usbUnderrunsModulo128','usbMissedModulo128']
ports=[n for n in mido.get_input_names() if 'Felucca' in n];assert len(ports)==1,ports
rows=[];hashes=[];row={};started=time.monotonic();end=started+a.seconds
port=rtmidi.MidiIn();port.ignore_types(sysex=False,timing=False,active_sense=True);port.open_port(port.get_ports().index(ports[0]))
try:
 while time.monotonic()<end:
  while True:
   item=port.get_message()
   if item is None:break
   msg=mido.Message.from_bytes(item[0])
   if msg.type=='sysex' and len(msg.data) in (10,26,31) and list(msg.data[:3])==[0x7d,0x52,0x44]:
    proof=dict(second=msg.data[3]|msg.data[4]<<7,hash=sum(v<<(i*7) for i,v in enumerate(msg.data[5:10])),hostElapsed=round(time.monotonic()-started,3))
    if len(msg.data)==31:proof['hardwareTicks']=sum(v<<(i*7) for i,v in enumerate(msg.data[26:31]))
    hashes.append(proof)
   if msg.type=='control_change' and msg.channel==15 and 110<=msg.control<=126:
    if msg.control==110:row={}
    row[fields[msg.control-110]]=msg.value
    if len(row)==len(fields):row['hostElapsed']=round(time.monotonic()-started,3);rows.append(row);print(json.dumps(row),flush=True);row={}
  time.sleep(.005)
finally:port.close_port()
assert rows,'No SPC telemetry'
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(rows,indent=2)+'\n')
a.output.with_suffix('.hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
print('medianFPS',statistics.median(r['fps'] for r in rows),'minFPS',min(r['fps'] for r in rows),
      'maxAudioUs',max(r['maxAudio100us'] for r in rows)*100,'lateBlocks',sum(r['audioDeadlineMisses'] for r in rows),flush=True)
