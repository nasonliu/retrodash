import json,re,struct,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import lz4blk
from cq_bundle import build
class PackedResourcesTests(unittest.TestCase):
 def test_each_song_image_decodes_to_original_bytes(self):
  for name in ['peaceful-days-spc','ff6-terra-snow']:
   root=ROOT/'projects'/name
   song=([(0,48,60,7,0,0)],[],8,6000,30105600,42,{60:7})
   raw,_=build(root,song);packed,_=build(root,song,compact=True)
   self.assertLess(len(packed),len(raw))
   io,ni=struct.unpack_from('<II',raw,52);po=struct.unpack_from('<I',packed,52)[0]
   for i in range(ni):
    w,h,o,n,f=struct.unpack_from('<HHIII',raw,io+i*16)
    pw,ph,p,pn,pf=struct.unpack_from('<HHIII',packed,po+i*16)
    self.assertEqual((w,h,f),(pw,ph,pf&7));data=packed[p:p+pn]
    if pf&8:
     palette=32 if pf&2 else 512;data=data[:palette]+lz4blk.decompress(data[palette:])
    self.assertEqual(data,raw[o:o+n])
 def test_ui_packed_blocks_are_pixel_identical(self):
  raw=(ROOT/'firmware/gen/rd_assets.h').read_text();packed=(ROOT/'firmware/gen/rd_assets_packed.h').read_text()
  for name,items in re.findall(r'static const uint16_t (\w+)\[\d+\] = \{([^}]+)',raw):
   expected=b''.join(struct.pack('<H',int(x,0))for x in items.split(','))
   data=bytes(map(int,re.search(name+'_BYTES.*?=\\{([^}]+)',packed).group(1).split(',')))
   offsets=list(map(int,re.search(name+'_OFF.*?=\\{([^}]+)',packed).group(1).split(',')))
   decoded=b''.join(lz4blk.decompress(data[a:b])for a,b in zip(offsets,offsets[1:]))
   self.assertEqual(decoded,expected)
if __name__=='__main__':unittest.main()
