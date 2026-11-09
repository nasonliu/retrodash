import sys,json,unittest,hashlib
from pathlib import Path
from mido import MidiFile
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from cq_compile import compile_song
from cq_tracks import extract
from cq_bundle import build
class PeacefulDays(unittest.TestCase):
 def setUp(self):
  self.root=ROOT/'projects/peaceful-days';self.options=json.loads((self.root/'chart/chart.json').read_text());self.mid=MidiFile(self.root/'music/song.mid')
 def test_all_notes_original_pitches_and_source_preserved(self):
  src=json.loads((self.root/'music/source-info.json').read_text());self.assertEqual(hashlib.sha256((self.root/'music/song.mid').read_bytes()).hexdigest(),src['sha256'])
  chart,band,_=extract(self.mid,3,{})
  C,B,_,bpm,_,_,mapping=compile_song(self.mid,'PEACEFUL DAYS','song.mid',3,{},self.options)
  self.assertEqual((len(C),len(band),bpm),(174,3169,8600));self.assertEqual(sorted([(c[0],c[2]+12*c[4]) for c in C]+[(b[0],b[3]) for b in B if b[2]==0 and not b[5]]),sorted((round(n['tick']*96/self.mid.ticks_per_beat),n['pitch']) for n in chart));self.assertTrue(all(C[i][0]-C[i-1][0]>=96 for i in range(1,len(C))));self.assertEqual(sum(b[2]==0 and not b[5] for b in B),81);self.assertTrue(all(0<=c[3]<27 and c[2]==53+c[3] for c in C))
  self.assertEqual(sorted((n['pitch'],n['velocity']) for n in band),sorted((n[3],n[4]) for n in B if not n[5] and n[2]!=0));self.assertEqual(len([n for n in B if n[5]]),6)
 def test_routing_and_shift_bounds(self):
  for key,value in [('melodyKeyOctaveShift',-5),('backingParts',{'1':1}),('patches',{'0':99})]:
   opts=dict(self.options);opts[key]=value
   with self.assertRaises(ValueError):compile_song(self.mid,'PEACEFUL DAYS','song.mid',3,{},opts)
 def test_complete_song_and_registered_animations(self):
  data,r=build(self.root);self.assertEqual((r['melodyNotes'],r['backingNotes'],r['frames']),(174,3169,14));self.assertLessEqual(len(data),180*1024)
  art=json.loads((self.root/'art/art.json').read_text());clips={c['id']:c for c in art['clips']};self.assertEqual(len(clips['crono.run']['frames']),6);self.assertEqual(len(clips['crono.attack']['frames']),6)
  rows=json.loads((self.root/'art/animation-registration.json').read_text());self.assertEqual({tuple(r['pivot']) for r in rows},{(48,105)})
  heights=[r['bounds'][3]-r['bounds'][1] for r in rows];self.assertLessEqual(max(heights)-min(heights),5)
if __name__=='__main__':unittest.main()
