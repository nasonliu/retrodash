import json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'projects/pixel-street'
class Animation(unittest.TestCase):
 def test_uniform_body_scale_and_anchor(self):
  rows=json.loads((ROOT/'art/animation-registration.json').read_text())
  self.assertEqual(len(rows),12);self.assertEqual({r['scale'] for r in rows},{0.125});self.assertEqual({tuple(r['pivot']) for r in rows},{(30,43)})
  heights=[r['visibleBounds'][3]-r['visibleBounds'][1] for r in rows]
  self.assertLessEqual(max(heights)-min(heights),1)
 def test_four_running_poses_and_six_attack_poses(self):
  art=json.loads((ROOT/'art/art.json').read_text());clips={c['id']:c for c in art['clips']}
  self.assertEqual(len({f['frame'] for f in clips['courier.run']['frames']}),4)
  self.assertEqual(len(clips['courier.attack']['frames']),6)
if __name__=='__main__':unittest.main()
