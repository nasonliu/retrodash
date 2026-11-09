#!/usr/bin/env python3
"""Embed selected SPC songs, retaining independent audio, charts and artwork."""
import argparse,json,struct,math,hashlib
from pathlib import Path
import lz4blk
from cq_bundle import build
from pack_ui import array,blocks
ROOT=Path(__file__).resolve().parents[1]

def dsp_ram(p):
    original=(p/'audio/aram.bin').read_bytes();regs=(p/'audio/dsp-registers.bin').read_bytes()
    ram=bytearray(65536);values={k:{regs[k]}for k in (0x5d,0x6d,0x7d)}
    for _,k,v in struct.iter_unpack('<IBB',(p/'audio/events-clock.bin').read_bytes()):
        if k in values:values[k].add(v)
    def keep(start,n):
        for i in range(n):at=(start+i)&65535;ram[at]=original[at]
    for directory in values[0x5d]:keep(directory*256,1024)
    for sample in json.loads((p/'audio/samples.json').read_text()):keep(sample['startAddress'],sample['brrBytes'])
    for esa in values[0x6d]:
        for edl in values[0x7d]:keep(esa*256,max(4,(edl&15)*2048))
    return bytes(ram)

def generate(projects,compact=False):
    audio='#pragma once\n#include "rd_pack.h"\nstruct rd_spc_target {unsigned clock,duration;unsigned char regs[8],voice;};\n'
    audio+='struct rd_spc_song_data {const unsigned char *ram,*regs;unsigned ram_size,duration;const rd_blocks_t *events;const rd_spc_target *targets;unsigned target_count;const unsigned char *parts;unsigned part_count;};\n'
    art='#pragma once\n';audio_rows=[];art_rows=[]
    out=ROOT/'build/spc-device';out.mkdir(parents=True,exist_ok=True)
    for i,project in enumerate(projects):
        p=project.resolve();chart=json.loads((p/'chart/chart.json').read_text());notes=json.loads((p/'audio/voices.json').read_text());m=json.loads((p/'manifest.json').read_text())
        ram=dsp_ram(p) if compact else (p/'audio/aram.bin').read_bytes();packed=lz4blk.compress(ram);assert lz4blk.decompress(packed)==ram
        prefix=f'RD_SPC_{i}';audio+=array(prefix+'_RAM',packed)+array(prefix+'_REGS',(p/'audio/dsp-registers.bin').read_bytes())
        audio+=blocks(prefix+'_EVENTS',(p/'audio/events.sde').read_bytes())
        parts=chart.get('melodyParts',[dict(voice=chart['manualVoice'],srcn=chart['manualSrcn'])]);audio+=array(prefix+'_PARTS',bytes(v for part in parts for v in (part['voice'],part['srcn'])))
        audio+='static const rd_spc_target '+prefix+'_TARGETS[]={\n'
        for t in chart['targets']:
            n=notes[t['noteIndex']];r=n['volumeLR']+[n['pitch']&255,n['pitch']>>8,n['srcn']]+n['adsr']+[n['gain']]
            audio+='{%du,%du,{%s},%du},\n'%(n['clock'],max(32,n['gateEndClock']-n['clock']),','.join(map(str,r)),n['voice'])
        audio+='};\n';audio_rows.append('{%s_RAM,%s_REGS,%du,%du,%s_EVENTS,%s_TARGETS,%du,%s_PARTS,%du}'%(prefix,prefix,len(packed),m['durationSeconds']*1024000,prefix,prefix,len(chart['targets']),prefix,len(parts)))
        bpm=round(chart.get('bpm',87.27)*100);fpt=round(44100*60/(bpm/100*96)*65536)
        tm=lambda c:round(c*96*bpm/(6000*1024000))
        C=[(tm(t['clock']),max(1,tm(t['gateEndClock'])-tm(t['clock'])),t['keyMidiPitch'],t['keyIndex'],2,0)for t in chart['targets']]
        hash_=int.from_bytes(hashlib.sha256(b''.join(struct.pack('<HHBBbB',*r)for r in C)).digest()[:2],'little')&0x3fff
        song=(C,[],math.ceil(m['durationSeconds']*bpm/6000),bpm,fpt,hash_,{t['keyMidiPitch']:t['keyIndex']for t in chart['targets']})
        data,report=build(p,song,compact=compact);name=f'CQ_PROJECT_DATA_{i}';art+=array(name,data);art_rows.append('{'+name+','+str(len(data))+'u}')
        (out/(p.name+'.cqp')).write_bytes(data);(out/(p.name+'.report.json')).write_text(json.dumps(report,indent=2)+'\n')
        print(p.name,'ARAM',len(packed),'CQP',len(data),'targets',len(C))
    audio+=f'#define RD_SPC_NSONG {len(projects)}u\nstatic const rd_spc_song_data RD_SPC_SONGS[]={{'+','.join(audio_rows)+'};\n'
    art+='#define CQ_DEMO_DATA CQ_PROJECT_DATA_0\nstatic const struct {const unsigned char *bytes;unsigned size;} CQ_PROJECTS[]={'+','.join(art_rows)+'};\n'
    (ROOT/'firmware/gen/rd_spc_data.h').write_text(audio);(ROOT/'firmware/gen/cq_spc_demo.h').write_text(art)
    types=(ROOT/'firmware/gen/kf_songs.h').read_text().split('static const kf_cnote_t KF_S0_C')[0]
    (ROOT/'firmware/gen/kf_spc_song.h').write_text(types+f'\n#define KF_NSONG {len(projects)}u\nstatic kf_song_t KF_SONG[KF_NSONG];\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('projects',type=Path,nargs='*');parser.add_argument('--compact',action='store_true');a=parser.parse_args()
    projects=a.projects or [ROOT/'projects/peaceful-days-spc',ROOT/'projects/ff6-terra-snow']
    generate(projects,a.compact or len(projects)>1)
