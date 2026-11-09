import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from cq_bundle import build


class SPCProjectTests(unittest.TestCase):
    def test_compiler_uses_selected_projects_own_art(self):
        with tempfile.TemporaryDirectory() as tmp:
            project=Path(tmp)/'song'
            shutil.copytree(ROOT/'projects/peaceful-days-spc',project,
                            ignore=shutil.ignore_patterns('verification','vendor','tools','audio','music'))
            song=([(0,48,60,7,0,0)],[],8,6000,30105600,42,{60:7})
            before,report=build(project,song)
            self.assertEqual(report['melodyNotes'],1)
            file=project/'art/far.png'
            image=Image.open(file).convert('RGBA');image.putpixel((0,0),(255,0,0,255));image.save(file)
            after,_=build(project,song)
            self.assertNotEqual(before,after)
            manifest=json.loads((project/'manifest.json').read_text())
            self.assertEqual(manifest['format'],'retrodash-spc-project')


if __name__=='__main__':unittest.main()

class SceneExtensionsTests(unittest.TestCase):
    def test_two_enemy_types_and_blizzard_are_packed(self):
        import struct
        with tempfile.TemporaryDirectory() as tmp:
            project=Path(tmp)/'song'
            shutil.copytree(ROOT/'projects/peaceful-days-spc',project,
                            ignore=shutil.ignore_patterns('verification','vendor','tools','audio','music'))
            scene_path=project/'scene/scene.json'
            scene=json.loads(scene_path.read_text())
            scene['targetSkins'].append(dict(id='second',actor='blue-imp'))
            scene['weather']='blizzard'
            scene_path.write_text(json.dumps(scene))
            data,_=build(project,([(0,48,60,7,0,0)],[],8,6000,30105600,42,{60:7}))
            self.assertEqual(data[-4:],b'RDX1')
            self.assertEqual(struct.unpack_from('<II',data,len(data)-12),(1,2))
