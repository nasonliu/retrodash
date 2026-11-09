import json,shutil,sys,tempfile,unittest,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from cq_bundle import build,source
ROOT=Path(__file__).resolve().parents[2]
class Project(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)/'project';shutil.copytree(ROOT/'projects/pixel-street',self.root)
 def tearDown(self):self.tmp.cleanup()
 def art(self,change):
  p=self.root/'art/art.json';x=json.loads(p.read_text());change(x);p.write_text(json.dumps(x))
 def test_complete_project_and_zip(self):
  data,report=build(self.root);self.assertEqual(data[:4],b'CQP2');self.assertEqual(report['melodyNotes'],32);self.assertEqual(report['backingNotes'],130)
  path=Path(self.tmp.name)/'demo.cqproj'
  with zipfile.ZipFile(path,'w') as z:
   for p in self.root.rglob('*'):
    if p.is_file():z.write(p,p.relative_to(self.root))
  with source(path) as root:self.assertEqual(build(root)[0],data)
 def test_frame_bounds(self):
  self.art(lambda a:a['frames'][0].update(rect=[0,0,999,40]))
  with self.assertRaisesRegex(ValueError,'invalid frame'):build(self.root)
 def test_missing_resource(self):
  (self.root/'art/hero-0.png').unlink()
  with self.assertRaisesRegex(ValueError,'missing'):build(self.root)
 def test_duplicate_id(self):
  self.art(lambda a:a['images'].append(a['images'][0]))
  with self.assertRaisesRegex(ValueError,'duplicate'):build(self.root)
 def test_missing_action(self):
  self.art(lambda a:a['actors'][0]['actions'].pop('attack'))
  with self.assertRaises((KeyError,ValueError)):build(self.root)
 def test_resource_order_is_not_hardcoded(self):
  self.art(lambda a:a['images'].reverse())
  _,report=build(self.root);self.assertEqual(report['images'],7)
 def test_zip_traversal(self):
  p=Path(self.tmp.name)/'bad.cqproj'
  with zipfile.ZipFile(p,'w') as z:z.writestr('../escape','x')
  with self.assertRaisesRegex(ValueError,'unsafe'):
   with source(p):pass
if __name__=='__main__':unittest.main()
