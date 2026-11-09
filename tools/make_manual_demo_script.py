#!/usr/bin/env python3
"""Scripted physical-key input for reproducible host footage, not a live performance."""
from pathlib import Path
import json
from mido import MidiFile
from cq_tracks import extract
root=Path(__file__).resolve().parents[1];notes,_,_=extract(MidiFile(root/'projects/pixel-street/music/song.mid'),1,{})
events=[(300,'shot'),(600,'btn 12'),(1000,'shot'),(1300,'enc 0 1'),(1500,'btn 12'),(1800,'shot'),(2000,'btn 12'),(2200,'enc 0 -4'),(2400,'shot'),(2600,'btn 12'),(2800,'btn 8'),(3000,'enc 0 1'),(3200,'btn 12'),(3600,'shot'),(4000,'btn 8'),(4200,'enc 0 -2'),(4400,'btn 12'),(4800,'shot'),(5200,'btn 11'),(5300,'shot'),(5500,'btn 11'),(6000,'btn 12'),(6200,'shot'),(6500,'btn 12'),(6800,'shot'),(7000,'btn 8'),(10000,'btn 12')]
for note in notes:
 t=round(10165+note['tick']/480*60000/72)
 events += [(t,f"keys {1<<(note['pitch']-53)}"),(t+450,'keys 0')]
for t in [11000,12000,14000,16000,20000,35000,47500]:events.append((t,'shot'))
events.append((49000,'end'));(root/'tests/chipquest/project-scripted.txt').write_text('# Reproducible simulated keyboard input, not a human recording.\n'+'\n'.join(f'{t} {s}' for t,s in sorted(events))+'\n')
