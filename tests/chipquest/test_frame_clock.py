"""Compile the actual native clock function with simulated FM-1 timer registers.

The usual DINGHY_HOST build uses a different clock branch and missed this bug.
"""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


class FrameClockTests(unittest.TestCase):
    def test_native_timer_wrap_keeps_frames_due(self):
        source=(ROOT/'firmware/src/retrodash.c').read_text()
        native=re.search(r'static uint32_t rd_now_us\(void\)\s*\{.*?\n\}',source,re.S)
        self.assertIsNotNone(native)
        harness='''#include <stdint.h>
#include <stdio.h>
static uint32_t fm1_ms, hardware_ticks;
static uint32_t fm1_micros(void){return hardware_ticks/24u;}
'''+native.group(0)+'''
int main(void){
 fm1_ms=178954u;hardware_ticks=0xffffffffu-24u*2000u;
 uint32_t deadline=rd_now_us()+16667u;
 fm1_ms+=20u;hardware_ticks+=24u*20000u;
 if((int32_t)(rd_now_us()-deadline)<0){fprintf(stderr,"display deadline stranded across 179-second hardware wrap\\n");return 1;}
 fm1_ms=4294967u;hardware_ticks=100u;
 deadline=rd_now_us()+16667u;fm1_ms+=20u;hardware_ticks+=480000u;
 if((int32_t)(rd_now_us()-deadline)<0)return 2;
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'clock.c').write_text(harness)
            subprocess.run(['cc','-O2',str(p/'clock.c'),'-o',str(p/'clock')],check=True,capture_output=True)
            result=subprocess.run([str(p/'clock')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)


if __name__=='__main__':unittest.main()
