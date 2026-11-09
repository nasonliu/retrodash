import sys, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from build_spc_reference import make_chart
class ChartConfigTest(unittest.TestCase):
 def test_voice_changes_and_eighth_note_merge(self):
  config=dict(bpm=80,keyOctaveShift=0,melodyParts=[dict(voice=0,srcn=32),dict(voice=5,srcn=35)],pitchCalibration={'32':dict(dspPitch=2408,midiPitch=67),'35':dict(dspPitch=1807,midiPitch=55)})
  notes=[dict(clock=t,gateEndClock=t+400000,voice=v,srcn=s,pitch=p)for t,v,s,p in [(0,0,32,2408),(10240,0,32,2408),(1024000,1,32,2408),(2048000,5,35,1807)]]
  chart=make_chart(notes,config)
  self.assertEqual([t['voice']for t in chart['targets']],[0,5])
  self.assertEqual([t['keyMidiPitch']for t in chart['targets']],[67,55])
  self.assertEqual(len(chart['automaticMelodyFills']),1)
if __name__=='__main__':unittest.main()
