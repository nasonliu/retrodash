import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cq_compile import compile_song
from test_tracks import fixture

class Compile(unittest.TestCase):
    def test_physical_track_and_black_pitch_survive(self):
        C,B,end,bpm,fpt,h,mapping = compile_song(fixture(), 'TEST', 'test.mid', 1, {})
        self.assertEqual([(n[2],n[3]) for n in C], [(60,7),(61,8)])
        self.assertEqual([(n[2],n[3]) for n in B], [(1,48)])
        self.assertEqual(mapping,{60:7,61:8})
    def test_manual_chord_choice(self):
        C,*_ = compile_song(fixture(), 'TEST', 'test.mid', 1, {0:64})
        self.assertEqual(C[0][2],64)
if __name__=='__main__': unittest.main()
