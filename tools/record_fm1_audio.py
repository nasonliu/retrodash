#!/usr/bin/env python3
"""Capture FM-1 USB output through CoreAudio/PortAudio (optional test dependencies).

Requires numpy and sounddevice in the test venv; neither is a firmware dependency.
"""
import argparse,json,wave
from pathlib import Path
import sounddevice as sd
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--seconds',type=int,default=25);a=p.parse_args()
devices=sd.query_devices();ids=[i for i,d in enumerate(devices) if d['name']=='Felucca' and d['max_input_channels']>=2]
assert len(ids)==1,ids
sd.default.latency='high'
audio=sd.rec(a.seconds*44100,samplerate=44100,channels=2,dtype='int16',device=ids[0],blocking=True)
status=str(sd.get_status())
with wave.open(str(a.output),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100);w.writeframes(audio.tobytes())
report=dict(device='Felucca',sampleRate=44100,frames=len(audio),seconds=len(audio)/44100,portAudioStatus=status)
a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
