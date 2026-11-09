#!/usr/bin/env python3
"""Check actual host key-light traces: auto must be dark, NEXT is steady and THEN uses a separate dim layer."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('log',type=Path);p.add_argument('--auto',action='store_true');a=p.parse_args()
rows=[list(map(int,line.split())) for line in a.log.read_text().splitlines()]
assert rows,'empty light trace'
counts=[bin(row[2]).count('1') for row in rows]
assert max(counts)<=(0 if a.auto else 1), 'unexpected simultaneous/automatic key hints'
if not a.auto:assert max(counts)==1,'no manual key hint observed'
print(f'{len(rows)} frames checked; max lit keys={max(counts)}')

glow=[row[-1] for row in rows]
assert all(not (row[2]&row[-1]) for row in rows),'bright and dim layers overlap'
assert max(bin(g).count('1') for g in glow)<=(0 if a.auto else 1),'too many dim keys'
if not a.auto:assert any(glow) and any(not g for g in glow),'missing blinking THEN hint'
print('separate dim layer and automatic accompaniment darkness passed')
