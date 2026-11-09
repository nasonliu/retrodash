#!/usr/bin/env python3
"""Read temporary RD_PROFILE_BOOT telemetry from an attached FM-1 over MIDI."""
import argparse,time,json,statistics
from pathlib import Path
import mido
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--seconds',type=int,default=16);a=p.parse_args()
names=mido.get_input_names();matches=[n for n in names if 'Felucca' in n];assert len(matches)==1,names
rows=[];row={};end=time.monotonic()+a.seconds
with mido.open_input(matches[0]) as port:
 while time.monotonic()<end:
  for msg in port.iter_pending():
   if msg.type=='control_change' and msg.channel==15 and 110<=msg.control<=116:
    row[{110:'fps',111:'averageDrawMs',112:'maxDrawMs',113:'maxFrameGapMs',114:'averagePaintMs',115:'averageTransferMs',116:'lcdTimeouts'}[msg.control]]=msg.value
    if len(row)==7:rows.append(row);print(json.dumps(row),flush=True);row={}
  time.sleep(.005)
assert rows,'No native frame telemetry received'
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(rows,indent=2)+'\n')
print('medianFPS',statistics.median(r['fps'] for r in rows),'minFPS',min(r['fps'] for r in rows),'worstGap',max(r['maxFrameGapMs'] for r in rows),flush=True)
