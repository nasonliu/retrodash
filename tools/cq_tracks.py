#!/usr/bin/env python3
"""Inspect MIDI by physical track and extract a monophonic player chart.

Requires mido. Onsets at the same tick form a chord: choose its lowest pitch,
except for explicitly supplied tick-to-pitch overrides. Other tracks retain all
notes and their original track, channel, start, duration and velocity.
"""
import argparse
import json
from collections import defaultdict, deque
from pathlib import Path
from mido import MidiFile

def extract(mid, melody_track, overrides):
    if mid.ticks_per_beat <= 0:
        raise ValueError('SMPTE MIDI timing is not supported')
    if mid.type == 2:
        raise ValueError('MIDI type 2 has asynchronous tracks and is not supported')
    if not (0 <= melody_track < len(mid.tracks)):
        raise ValueError('melody track index out of range')
    tracks, tempos = [], []
    for index, track in enumerate(mid.tracks):
        tick, pending, notes = 0, defaultdict(deque), []
        for msg in track:
            tick += msg.time
            if msg.type == 'set_tempo': tempos.append({'tick': tick, 'tempo': msg.tempo})
            if msg.type not in ('note_on', 'note_off'): continue
            key = (msg.channel, msg.note)
            if msg.type == 'note_on' and msg.velocity:
                pending[key].append((tick, msg.velocity))
            elif pending[key]:
                start, velocity = pending[key].popleft()
                if tick <= start: raise ValueError(f'track {index}: zero-length note')
                notes.append({'track': index, 'channel': msg.channel, 'pitch': msg.note,
                              'tick': start, 'duration': tick - start, 'velocity': velocity})
            else:
                raise ValueError(f'track {index}: unmatched note off at tick {tick}')
        if any(pending.values()): raise ValueError(f'track {index}: unclosed notes')
        tracks.append(sorted(notes, key=lambda n: (n['tick'], n['pitch'])))
    melody = tracks[melody_track]
    if not melody: raise ValueError('melody track has no notes')
    groups = defaultdict(list)
    for n in melody: groups[n['tick']].append(n)
    invalid = set(overrides) - set(groups)
    if invalid: raise ValueError(f'override tick has no chord: {min(invalid)}')
    chart = []
    for tick, group in sorted(groups.items()):
        pitch = overrides.get(tick, min(n['pitch'] for n in group))
        candidates = [n for n in group if n['pitch'] == pitch]
        if not candidates: raise ValueError(f'override at tick {tick} is not in the chord')
        chart.append(max(candidates, key=lambda n: n['duration']))
    band = [note for i, notes in enumerate(tracks) if i != melody_track for note in notes]
    return chart, sorted(band, key=lambda n: (n['tick'], n['track'], n['pitch'])), tempos

def assign_keys(chart):
    pitches = sorted({n['pitch'] for n in chart})
    if pitches and (pitches[0] < 53 or pitches[-1] > 79):
        raise ValueError(f'melody range {pitches[0]}..{pitches[-1]} exceeds FM-1 keys 53..79 (F3..G5); transpose the MIDI')
    mapping = {pitch: pitch - 53 for pitch in pitches}
    return [dict(n, key=mapping[n['pitch']]) for n in chart], mapping

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('midi', type=Path)
    parser.add_argument('--melody-track', type=int, required=True, help='zero-based MIDI track index')
    parser.add_argument('--roots', type=Path, help='JSON object mapping onset ticks to pitches')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    overrides = {int(k): int(v) for k, v in json.loads(args.roots.read_text()).items()} if args.roots else {}
    mid = MidiFile(args.midi)
    chart, band, tempos = extract(mid, args.melody_track, overrides)
    chart, key_map = assign_keys(chart)
    result = {'key_map': key_map, 'format': 'chipquest-tracks', 'version': 1, 'ticks_per_beat': mid.ticks_per_beat,
              'melody_track': args.melody_track, 'chart': chart, 'accompaniment': band, 'tempos': tempos}
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(f'{len(chart)} playable notes, {len(band)} accompaniment notes; saved {args.output}')

if __name__ == '__main__': main()
