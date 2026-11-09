#!/usr/bin/env python3
"""Pack the existing UI pixels losslessly; keep the editable/original header."""
import re,struct
from pathlib import Path
import lz4blk
ROOT=Path(__file__).resolve().parents[1]
def array(name,data):
 return 'static const unsigned char '+name+'[] __attribute__((aligned(4)))={'+','.join(map(str,data))+'};\n'
def blocks(name,data):
 parts=[lz4blk.compress(data[i:i+4096])for i in range(0,len(data),4096)];off=[0]
 for i,part in enumerate(parts):assert lz4blk.decompress(part)==data[i*4096:(i+1)*4096]
 for x in parts:off.append(off[-1]+len(x))
 return array(name+'_BYTES',b''.join(parts))+'static const uint32_t '+name+'_OFF[]={'+','.join(map(str,off))+'};\n'+'static const rd_blocks_t '+name+'[1]={{'+name+'_BYTES,'+name+'_OFF,'+str(len(data))+'u}};\n'
if __name__=='__main__':
 src=(ROOT/'firmware/gen/rd_assets.h').read_text();out='#pragma once\n#include "rd_pack.h"\n'
 out+='\n'.join(re.findall(r'^#define .*',src,re.M))+'\n'
 for name,items in re.findall(r'static const uint16_t (\w+)\[\d+\] = \{([^}]+)',src):
  raw=b''.join(struct.pack('<H',int(x,0))for x in items.split(','));out+=blocks(name,raw)
 (ROOT/'firmware/gen/rd_assets_packed.h').write_text(out)
 print('UI pixels packed losslessly')
