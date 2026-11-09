import json, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'projects/ff6-terra-snow'
class FF6ProjectTests(unittest.TestCase):
 def test_melody_switches_to_lower_brass_and_retains_mapping(self):
  c=json.loads((ROOT/'chart/chart.json').read_text());n=json.loads((ROOT/'audio/voices.json').read_text())
  self.assertEqual(len(c['targets']),78);self.assertEqual(len(c['automaticMelodyFills']),7)
  self.assertEqual({t['voice']for t in c['targets']},{0,4})
  for t in c['targets']:
   self.assertEqual(t['keyMidiPitch'],t['keyIndex']+53)
   self.assertTrue(53<=t['keyMidiPitch']<=79)
   if t['voice']==4:
    upper=[x for x in n if x['voice']==5 and x['srcn']==35 and x['clock']==t['clock']]
    self.assertTrue(upper);self.assertLessEqual(t['dspPitch'],upper[0]['pitch'])
 def test_song_duration_and_pcm_validation(self):
  m=json.loads((ROOT/'manifest.json').read_text());r=json.loads((ROOT/'verification/report.json').read_text())
  self.assertEqual(m['durationSeconds'],115);self.assertTrue(r['comparison']['equal'])
if __name__=='__main__':unittest.main()
