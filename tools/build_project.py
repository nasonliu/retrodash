#!/usr/bin/env python3
"""Build a complete v2 song project into a CQP and optionally FM-1 firmware."""
import argparse,json,subprocess,sys,zipfile
from pathlib import Path
from cq_bundle import source,build
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('project',type=Path);p.add_argument('--firmware',action='store_true');a=p.parse_args();project=a.project.resolve();out=ROOT/'build';out.mkdir(exist_ok=True)
 # Validate everything before changing the firmware-generated headers.
 with source(project) as src:
  data,report=build(src);m=json.loads((src/'manifest.json').read_text());chart=json.loads((src/m['chart']).read_text())
  subprocess.run(['node',str(ROOT/'tools/mid2vavehero.mjs')],cwd=ROOT,check=True)
  roots={str(x['tick']):x['pitch'] for x in chart['chordSelection'].get('overrides',[])}
  rootsfile=out/'project-roots.json';rootsfile.write_text(json.dumps(roots))
  subprocess.run([sys.executable,str(ROOT/'tools/cq_compile.py'),str(src/m['music']),'--title',m['title'],'--melody-track',str(chart['melodyTrack']),'--roots',str(rootsfile),'--chart-config',str(src/m['chart'])],cwd=ROOT,check=True)
  header=ROOT/'firmware/gen/kf_songs.h';header.write_text(header.read_text().replace('static const kf_song_t KF_SONG','static kf_song_t KF_SONG'))
  # Source bundle includes PNG, MIDI and author-editable JSON, never toolchain files.
  with zipfile.ZipFile(out/'demo.cqproj','w',zipfile.ZIP_DEFLATED) as z:
   for file in sorted(src.rglob('*')):
    if file.is_file():z.write(file,file.relative_to(src))
 subprocess.run([sys.executable,str(ROOT/'tools/cq_bundle.py'),str(project),'--output',str(out/'demo.cqp'),'--header',str(ROOT/'firmware/gen/cq_demo.h')],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/gen_cq_ui_font.py')],cwd=ROOT,check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/gen_retrodash_assets.py')],cwd=ROOT,check=True)
 if a.firmware:subprocess.run([sys.executable,str(ROOT/'tools/build.py')],cwd=ROOT,check=True)
 print('Project source: build/demo.cqproj; runtime: build/demo.cqp')
if __name__=='__main__':main()
