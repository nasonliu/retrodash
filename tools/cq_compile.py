#!/usr/bin/env python3
"""Append one track-based ChipQuest song to a freshly generated kf_songs.h."""
import argparse, hashlib, json, re
from pathlib import Path
from mido import MidiFile
from cq_tracks import extract, assign_keys

ROOT = Path(__file__).resolve().parents[1]

def compile_song(mid, title, filename, melody, overrides, options=None):
    chart, band, tempos = extract(mid, melody, overrides)
    options = options or {}
    gap = options.get('minimumTapBeats', 0)
    if not isinstance(gap,(int,float)) or not 0<=gap<=4: raise ValueError('minimumTapBeats must be 0..4')
    kept,assist=[],[]
    for note in chart:
        if not kept or note['tick']-kept[-1]['tick']>=round(gap*mid.ticks_per_beat):kept.append(note)
        else:assist.append(note)
    chart=kept
    options = options or {}
    shift = options.get('melodyKeyOctaveShift', 0)
    if not isinstance(shift, int) or not -4 <= shift <= 4: raise ValueError('melodyKeyOctaveShift must be -4..4')
    # Key pitch is independent of sounding pitch; original MIDI remains untouched.
    chart, mapping = assign_keys([dict(n, pitch=n['pitch']+12*shift) for n in chart])
    if len(chart) > 400: raise ValueError('more than 400 melody notes; split the song')
    if len({t['tempo'] for t in tempos}) > 1: raise ValueError('tempo changes are not yet supported')
    if tempos and min(t['tick'] for t in tempos) > 0 and tempos[0]['tempo'] != 500000:
        raise ValueError('tempo change after playback starts is not yet supported')
    tempo = tempos[0]['tempo'] if tempos else 500000
    if any(n['channel'] == 9 for n in band) and any(any(n['channel'] != 9 and n['track'] == d['track'] for n in band) for d in band if d['channel'] == 9):
        raise ValueError('a track mixes drum and melodic channels')
    table = (ROOT / 'firmware/gen/kf_tables.h').read_text()
    match = re.search(r'KF_GM\[128\] = \{([^}]+)\}', table)
    if not match: raise ValueError('cannot read the firmware drum map')
    gm = [int(v.strip()) for v in match.group(1).split(',')]
    unsupported = sorted({n['pitch'] for n in band if n['channel'] == 9 and gm[n['pitch']] == 255})
    if unsupported: raise ValueError(f'unsupported drum pitches: {unsupported}; choose supported kit notes')
    tracks = sorted({n['track'] for n in band if n['channel'] != 9})
    routing = options.get('backingParts')
    if routing is None:
        if len(tracks) > 5: raise ValueError('more than five melodic backing tracks; specify backingParts routing')
        parts = {track: i + 1 for i, track in enumerate(tracks)}
    else:
        parts = {int(track): part for track,part in routing.items()}
        if set(parts) != set(tracks) or any(not isinstance(p,int) or not 1<=p<=5 for p in parts.values()):
            raise ValueError('backingParts must route every melodic backing track to part 1..5')
    def tm(t):
        n = (t * 96 + mid.ticks_per_beat // 2) // mid.ticks_per_beat
        if not 0 <= n <= 65535: raise ValueError('song exceeds T96 time limit')
        return n
    C = [(tm(n['tick']), tm(n['duration']), n['pitch'], n['key'], -shift, 1 if tm(n['duration']) >= 144 else 0) for n in chart]
    B = [(tm(n['tick']), tm(n['duration']), 6 if n['channel']==9 else parts[n['track']], n['pitch'], n['velocity'], 0) for n in band]
    B += [(tm(n['tick']),tm(n['duration']),0,n['pitch'],n['velocity'],0) for n in assist]
    if any(row[1] == 0 for row in C + B): raise ValueError('a note is too short for the 1/96-beat grid')
    patch_events = []
    counts = [5,4,4,4,4,4,4]
    for part,patch in options.get('patches',{}).items():
        part=int(part)
        if not 0<=part<=6 or not isinstance(patch,int) or not 0<=patch<counts[part]: raise ValueError('invalid part patch')
        patch_events.append((0,1,part,0,0,patch+1))
    B = sorted(patch_events + B, key=lambda n:(n[0],0 if n[5] else 1,n[2],n[3]))
    if len(B) > 65535: raise ValueError('too many backing notes')
    last = max([t+l for t,l,*_ in C] + [t+l for t,l,*_ in B])
    end = max(8, ((last + 383)//384)*4)
    if end > 65535: raise ValueError('song too long')
    bpm100 = round(6000000000/tempo); fpt = round(44100*tempo/1000000/96*65536)
    h=int.from_bytes(hashlib.sha256(json.dumps([C,B]).encode()).digest()[:2],'big') & 0x3fff
    if not re.fullmatch(r'[A-Za-z0-9 _.-]{1,24}', title): raise ValueError('title must use 1-24 ASCII letters, digits or spaces')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', filename): raise ValueError('filename must be simple ASCII')
    return C,B,end,bpm100,fpt,h,mapping

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('midi',type=Path);p.add_argument('--melody-track',type=int,required=True);p.add_argument('--title',required=True)
    p.add_argument('--chart-config',type=Path);p.add_argument('--roots',type=Path);p.add_argument('--output',type=Path,default=ROOT/'firmware/gen/kf_songs.h')
    a=p.parse_args();overrides={int(k):int(v) for k,v in json.loads(a.roots.read_text()).items()} if a.roots else {}
    C,B,end,bpm,fpt,h,mapping=compile_song(MidiFile(a.midi),a.title,a.midi.name,a.melody_track,overrides,json.loads(a.chart_config.read_text()) if a.chart_config else None)
    text=a.output.read_text();m=re.search(r'#define KF_NSONG (\d+)u',text)
    if not m: raise ValueError('base header missing KF_NSONG')
    i=int(m.group(1)); tag=f'KF_S{i}'
    fmt=lambda row:'{' + ', '.join(map(str,row)) + '}'
    block=(f'static const kf_cnote_t {tag}_C[{len(C)}] = {{\n'+',\n'.join('    '+fmt(x) for x in C)+'\n};\n'
           +f'static const kf_bev_t {tag}_B[{max(1,len(B))}] = {{\n'+(',\n'.join('    '+fmt(x) for x in B) if B else '    {0, 0, 0, 0, 0, 0}')+'\n};\n'
           +f'static const kf_oct_t {tag}_O[1] = {{{{0, 0}}}};\nstatic const kf_phrase_t {tag}_P[1] = {{{{0, 0}}}};\n')
    text=text[:m.start()]+block+text[m.start():]
    text=text.replace(f'#define KF_NSONG {i}u',f'#define KF_NSONG {i+1}u',1)
    start=text.index('static const kf_song_t KF_SONG[KF_NSONG] = {');endpos=text.index('\n};',start)
    row=f'    {{"{a.title}", "{a.midi.name}", {bpm}u, {fpt}u, {end}, 127, 0, 0, {h}, {tag}_C, {len(C)}, {tag}_B, {len(B)}, {tag}_O, 0, 0, {tag}_P}},'
    text=text[:endpos]+'\n'+row+text[endpos:];a.output.write_text(text)
    print(f'Added {a.title}: {len(C)} melody notes, {len(B)} backing notes, {len(mapping)} fixed keys; times rounded to 1/96 beat')

if __name__=='__main__': main()
