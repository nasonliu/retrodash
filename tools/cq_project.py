#!/usr/bin/env python3
"""Create and compile a ChipQuest MIDI project (GPL-3.0-or-later)."""
import argparse
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAX_MIDI = 256 * 1024

def read_project(path):
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        if sorted(names) != ['manifest.json', 'music.mid']:
            raise ValueError('project must contain exactly manifest.json and music.mid')
        if any(z.getinfo(n).file_size > MAX_MIDI for n in names):
            raise ValueError('project entry too large')
        manifest = json.loads(z.read('manifest.json'))
        if manifest.get('format') != 'chipquest-project' or manifest.get('version') != 1:
            raise ValueError('unsupported project format')
        data = z.read('music.mid')
        if len(data) < 14 or data[:4] != b'MThd':
            raise ValueError('music.mid is not a standard MIDI file')
        return manifest, data

def main():
    p = argparse.ArgumentParser(description=__doc__)
    sp = p.add_subparsers(dest='cmd', required=True)
    a = sp.add_parser('pack', help='pack a standard MIDI file')
    a.add_argument('midi', type=Path); a.add_argument('project', type=Path)
    a.add_argument('--title', required=True); a.add_argument('--author', default='')
    a = sp.add_parser('compile', help='add a project to this firmware source tree')
    a.add_argument('project', type=Path); a.add_argument('--name', required=True, help='safe MIDI filename for songs/')
    a.add_argument('--melody-track', required=True, type=int, help='zero-based physical MIDI track')
    a.add_argument('--roots', type=Path, help='JSON object mapping chord start tick to chosen pitch')
    a = sp.add_parser('inspect', help='read a project manifest')
    a.add_argument('project', type=Path)
    args = p.parse_args()
    if args.cmd == 'pack':
        data = args.midi.read_bytes()
        if len(data) > MAX_MIDI or len(data) < 14 or data[:4] != b'MThd': raise ValueError('invalid or oversized MIDI')
        manifest = {'format': 'chipquest-project', 'version': 1, 'title': args.title, 'author': args.author, 'music': 'music.mid'}
        with zipfile.ZipFile(args.project, 'w', compression=zipfile.ZIP_DEFLATED) as z:
            z.writestr('manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2))
            z.writestr('music.mid', data)
        print(f'Created {args.project}')
    else:
        manifest, data = read_project(args.project)
        if args.cmd == 'inspect':
            print(json.dumps(manifest, ensure_ascii=False, indent=2)); return
        name = args.name
        if not re.fullmatch(r'[A-Za-z0-9_-]+\.mid', name, flags=re.I): raise ValueError('name must be a simple .mid filename')
        target = ROOT / 'songs' / name
        if target.exists(): raise FileExistsError(f'{target} already exists')
        target.write_bytes(data)
        header = ROOT / 'firmware/gen/kf_songs.h'
        prior = header.read_bytes()
        try:
            subprocess.run(['node', str(ROOT / 'tools' / 'mid2vavehero.mjs')], cwd=ROOT, check=True)
            command = [sys.executable, str(ROOT / 'tools' / 'cq_compile.py'), str(target),
                       '--melody-track', str(args.melody_track), '--title', 'USER SONG']
            if args.roots: command += ['--roots', str(args.roots.resolve())]
            subprocess.run(command, cwd=ROOT, check=True)
        except Exception:
            target.unlink(missing_ok=True)
            header.write_bytes(prior)
            raise
        print('Project compiled into firmware/gen/kf_songs.h; build the firmware to use it.')

if __name__ == '__main__':
    main()
