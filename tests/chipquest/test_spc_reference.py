import importlib.util
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('spc_reference', ROOT/'tools/build_spc_reference.py')
spc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(spc)


class SPCReferenceTests(unittest.TestCase):
    def test_compact_preserves_retrigger(self):
        events = [(0,2,10),(1,2,10),(2,0x4c,1),(3,0x4c,1),(4,0x4c,0),(5,0x4c,0)]
        self.assertEqual(spc.compact(events,bytes(128)),[events[i] for i in (0,2,3,4)])

    def test_delta_roundtrip(self):
        events = [(0,1,2),(127,3,4),(255,5,6),(1024000,0x4c,255)]
        data = spc.pack_delta(events)
        self.assertEqual(data[:8],b'RDSD\x01\0\0\0')
        self.assertEqual(struct.unpack_from('<II',data,8),(1024000,len(events)))
        i,clock,result=16,0,[]
        while i<len(data):
            delta,shift=0,0
            while True:
                b=data[i];i+=1;delta|=(b&127)<<shift;shift+=7
                if not b&128:break
            clock+=delta;result.append((clock,data[i],data[i+1]));i+=2
        self.assertEqual(result,events)

    def test_brr_block_and_signed_nibbles(self):
        data=bytes([1])+bytes([0x78])*8
        pcm=struct.unpack('<16h',spc.decode_brr(data))
        self.assertEqual(pcm,(6,-8)*8)
        ram=bytearray(65536)
        for i,b in enumerate(data):ram[(65532+i)&65535]=b
        self.assertEqual(spc.brr_blocks(ram,65532),data)

    def test_voice_retrigger_and_keyoff(self):
        initial=bytearray(128);initial[4]=34
        notes=spc.notes_from_events([(10,0x4c,1),(20,0x4c,1),(30,0x5c,1)],initial,100)
        self.assertEqual([(n['clock'],n['gateEndClock']) for n in notes],[(10,20),(20,30)])


if __name__=='__main__':unittest.main()
