import importlib.util
import unittest
from pathlib import Path
from mido import MidiFile, MidiTrack, Message, MetaMessage

p = Path(__file__).resolve().parents[2] / 'tools' / 'cq_tracks.py'
spec = importlib.util.spec_from_file_location('cq_tracks', p)
cq = importlib.util.module_from_spec(spec); spec.loader.exec_module(cq)

def fixture():
    mid = MidiFile(ticks_per_beat=480)
    t = MidiTrack(); mid.tracks.append(t)
    t.append(MetaMessage('set_tempo', tempo=500000, time=0))
    t = MidiTrack(); mid.tracks.append(t)
    for n in (60, 64, 67): t.append(Message('note_on', channel=0, note=n, velocity=90, time=0))
    for n in (60, 64, 67): t.append(Message('note_off', channel=0, note=n, time=480 if n == 60 else 0))
    t.append(Message('note_on', channel=0, note=61, velocity=80, time=0)); t.append(Message('note_off', channel=0, note=61, time=240))
    t = MidiTrack(); mid.tracks.append(t)
    t.append(Message('note_on', channel=0, note=48, velocity=70, time=0)); t.append(Message('note_off', channel=0, note=48, time=960))
    return mid

class Tracks(unittest.TestCase):
    def test_same_channel_separate_tracks_and_lowest_chord_note(self):
        chart, band, _ = cq.extract(fixture(), 1, {})
        self.assertEqual([(n['tick'], n['pitch']) for n in chart], [(0, 60), (480, 61)])
        self.assertEqual([(n['track'], n['pitch']) for n in band], [(2, 48)])
    def test_root_override_must_be_in_chord(self):
        chart, _, _ = cq.extract(fixture(), 1, {0: 64})
        self.assertEqual(chart[0]['pitch'], 64)
        with self.assertRaises(ValueError): cq.extract(fixture(), 1, {0: 66})
    def test_pitch_has_stable_key_including_black_notes(self):
        chart, _, _ = cq.extract(fixture(), 1, {})
        mapped, pitch_map = cq.assign_keys(chart)
        self.assertEqual([n['key'] for n in mapped], [7, 8])
        self.assertEqual(pitch_map, {60: 7, 61: 8})
        again, _ = cq.assign_keys(chart + [dict(chart[0], tick=1200)])
        self.assertEqual(again[-1]['key'], 7)

    def test_too_many_distinct_pitches_rejected(self):
        with self.assertRaises(ValueError): cq.assign_keys([{'pitch': p} for p in range(28)])

    def test_missing_track_rejected(self):
        with self.assertRaises(ValueError): cq.extract(fixture(), 5, {})

if __name__ == '__main__': unittest.main()
