#!/usr/bin/env python3
"""Build an offline-verified SPC source project, not a firmware-loadable CQP.

Requires an unmodified checkout of blarggs-audio-libraries/snes_spc and c++.
DSP timing is in 1,024,000 Hz clocks; audio remains 32 kHz stereo.
"""
import argparse
import collections
import hashlib
import json
import math
import struct
import subprocess
import wave
from pathlib import Path

TOOLS = Path(__file__).resolve().parent


def save_json(p, value):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


def wav(p, pcm, channels=2):
    with wave.open(str(p), 'wb') as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(32000)
        w.writeframes(pcm)


def brr_blocks(ram, start):
    out = bytearray()
    for n in range(65536 // 9):
        a = (start + n * 9) & 65535
        block = bytes(ram[(a + i) & 65535] for i in range(9))
        out.extend(block)
        if block[0] & 1:
            return bytes(out)
    raise ValueError('BRR sample has no end block')


def decode_brr(data):
    # Integer filter arithmetic follows snes_spc/SPC_DSP.cpp (Shay Green,
    # LGPL-2.1-or-later). This WAV is an inspection aid, not a full DSP voice.
    out, p1, p2 = [], 0, 0
    for a in range(0, len(data), 9):
        h = data[a]
        for b in data[a + 1:a + 9]:
            for n in (b >> 4, b & 15):
                n = n - 16 if n & 8 else n
                s = (n << (h >> 4)) >> 1
                if h >> 4 >= 13:
                    s = -2048 if n < 0 else 0
                half = p2 >> 1
                f = h & 12
                if f >= 8:
                    s += p1 - half
                    if f == 8:
                        s += (half >> 4) + ((p1 * -3) >> 6)
                    else:
                        s += ((p1 * -13) >> 7) + ((half * 3) >> 4)
                elif f:
                    s += (p1 >> 1) + ((-p1) >> 5)
                s = max(-32768, min(32767, s))
                s = ((s * 2 + 32768) & 65535) - 32768
                out.append(s)
                p2, p1 = p1, s
    return struct.pack('<' + 'h' * len(out), *out)


def compact(events, initial):
    regs, result = list(initial), []
    for t, a, v in events:
        # KON writes can retrigger a voice even when the byte is unchanged.
        if regs[a] != v or (a == 0x4c and v):
            result.append((t, a, v))
        regs[a] = v
    return result


def pack_delta(events):
    out, previous = bytearray(b'RDSD\x01\x00\x00\x00'), 0
    out.extend(struct.pack('<II', 1024000, len(events)))
    for t, a, v in events:
        delta = t - previous
        if delta < 0:
            raise ValueError('unordered DSP writes')
        while delta >= 128:
            out.append((delta & 127) | 128)
            delta >>= 7
        out.extend((delta, a, v))
        previous = t
    return bytes(out)


def notes_from_events(events, initial, end):
    regs, active, notes = list(initial), {}, []
    def stop(ch, t):
        if ch in active:
            n = active.pop(ch)
            n['gateEndClock'] = t
            notes.append(n)
    for t, a, v in events:
        regs[a] = v
        if a == 0x5c:
            for ch in range(8):
                if v >> ch & 1:
                    stop(ch, t)
        if a == 0x4c:
            for ch in range(8):
                if not (v >> ch & 1):
                    continue
                stop(ch, t)
                r = regs[ch * 16:ch * 16 + 8]
                active[ch] = dict(clock=t, voice=ch, srcn=r[4],
                    pitch=r[2] | ((r[3] & 63) << 8), volumeLR=r[:2],
                    adsr=r[5:7], gain=r[7])
    for ch in list(active):
        stop(ch, end)
    return sorted(notes, key=lambda n: (n['clock'], n['voice']))


DEFAULT_CONFIG = dict(title='PEACEFUL DAYS', id='retrodash.chrono-trigger.peaceful-days.spc',
    bpm=87.27, keyOctaveShift=-2,
    melodyParts=[dict(voice=0,srcn=34)],
    pitchCalibration={'33':dict(dspPitch=4549,midiPitch=78),
                      '34':dict(dspPitch=3430,midiPitch=78)},
    pitchCalibrationStatus='aligned to supplied MIDI')


def make_chart(notes, config):
    chart, assist, previous = [], [], -10**10
    bpm, minimum_beats = config['bpm'], 0.5
    calibration = config['pitchCalibration']
    parts = config['melodyParts']
    shift = config.get('keyOctaveShift', 0)
    for index, n in enumerate(notes):
        seconds = n['clock'] / 1024000
        if not any(n['voice']==part['voice'] and n['srcn']==part['srcn']
                   and part.get('startSeconds',0)<=seconds<part.get('endSeconds',601)
                   for part in parts):
            continue
        cal = calibration[str(n['srcn'])]
        pitch = cal['midiPitch'] + round(12*math.log2(n['pitch']/cal['dspPitch']))
        physical = pitch + shift * 12
        if not 53 <= physical <= 79:
            raise ValueError('melody pitch outside physical FM-1 keys')
        target = dict(noteIndex=index,voice=n['voice'],clock=n['clock'],gateEndClock=n['gateEndClock'],
            srcn=n['srcn'],dspPitch=n['pitch'],soundingMidiPitch=pitch,
            keyMidiPitch=physical,keyIndex=physical-53)
        bucket = round(seconds*bpm/60/minimum_beats)
        if bucket > previous:
            chart.append(target)
            previous=bucket
        else:
            assist.append(target)
    if not chart or len(chart)>400:
        raise ValueError('manual chart must contain 1..400 targets')
    return dict(format='retrodash-spc-chart',version=1,clockHz=1024000,
        manualVoice=parts[0]['voice'],manualSrcn=parts[0]['srcn'],melodyParts=parts,bpm=bpm,
        minimumTapBeats=minimum_beats,minimumNoteValue='1/8',mergeGrid='eighth-note',
        keyOctaveShift=shift,nextThenOnly=True,autoAccompanimentLights=False,
        pitchCalibration=calibration,
        pitchCalibrationStatus=config.get('pitchCalibrationStatus','sample loop fundamental'),
        targets=chart,automaticMelodyFills=assist)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('spc', type=Path)
    p.add_argument('--emulator-dir', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--work', type=Path, required=True)
    p.add_argument('--config',type=Path,help='song-specific melody parts and pitch calibration JSON')
    a = p.parse_args()
    config = json.loads(a.config.read_text()) if a.config else DEFAULT_CONFIG
    data = a.spc.read_bytes()
    if len(data) < 0x10200 or not data.startswith(b'SNES-SPC700 Sound File Data v0.30'):
        raise ValueError('expected an SPC v0.30 snapshot')
    seconds = int(data[0xa9:0xac].rstrip(b'\0 '))
    if not 1 <= seconds <= 600:
        raise ValueError('unsupported duration')
    src = a.emulator_dir.resolve() / 'snes_spc'
    work, out = a.work.resolve(), a.output.resolve()
    work.mkdir(parents=True, exist_ok=True)
    for d in ('music', 'audio/samples', 'chart', 'verification'):
        (out / d).mkdir(parents=True, exist_ok=True)
    def run(cmd):
        subprocess.run([str(x) for x in cmd], check=True)
    common = ['c++', '-O3', '-DNDEBUG', '-std=c++11', '-I' + str(src)]
    run(common + ['-include', TOOLS/'spc_trace_hook.h', TOOLS/'spc_reference.cpp'] +
        [src/x for x in ('SNES_SPC.cpp','SNES_SPC_misc.cpp','SNES_SPC_state.cpp','SPC_DSP.cpp')] +
        ['-o', work/'render'])
    run(common + [TOOLS/'spc_dsp_replay.cpp',src/'SPC_DSP.cpp','-o',work/'replay'])
    run([work/'render',a.spc.resolve(),seconds,work/'reference.pcm',work/'registers.bin'])
    events = list(struct.iter_unpack('<IBB',(work/'registers.bin').read_bytes()))
    ram, initial = data[256:65792], data[0x10100:0x10180]
    changed = compact(events, initial)
    changed_bytes = b''.join(struct.pack('<IBB',*e) for e in changed)
    (out/'audio/events-clock.bin').write_bytes(changed_bytes)
    packed = pack_delta(changed)
    (out/'audio/events.sde').write_bytes(packed)
    run([work/'replay',a.spc.resolve(),out/'audio/events-clock.bin',seconds,work/'replay.pcm'])
    reference, replay = (work/'reference.pcm').read_bytes(), (work/'replay.pcm').read_bytes()
    # SNES_SPC::reset_buf prepends four stereo frames, unlike the bare DSP.
    equal = reference[16:] == replay[:-16]
    if not equal:
        raise ValueError('DSP-only replay differs from full SPC emulation')
    (out/'music/source.spc').write_bytes(data)
    (out/'audio/aram.bin').write_bytes(ram)
    (out/'audio/dsp-registers.bin').write_bytes(initial)
    wav(out/'verification/reference.wav',reference)
    notes = notes_from_events(events,initial,seconds*1024000)
    save_json(out/'audio/voices.json',notes)
    sample_meta = []
    end_ram = (work/'registers.bin.end.spc').read_bytes()[256:65792]
    for srcn in sorted({n['srcn'] for n in notes}):
        start, loop = struct.unpack_from('<HH',ram,initial[0x5d]*256+srcn*4)
        brr = brr_blocks(ram,start)
        if any(ram[(start+i)&65535] != end_ram[(start+i)&65535] for i in range(len(brr))):
            raise ValueError('sample changed in ARAM during playback; static bank is insufficient')
        name = f'srcn-{srcn:02x}'
        (out/f'audio/samples/{name}.brr').write_bytes(brr)
        pcm = decode_brr(brr)
        wav(out/f'audio/samples/{name}.wav',pcm,1)
        sample_meta.append(dict(srcn=srcn,startAddress=start,loopAddress=loop,
            brrBytes=len(brr),decodedSamples=len(pcm)//2,loopEnabled=bool(brr[-9]&2),
            loopSampleOffset=((loop-start)&65535)//9*16,
            brr=f'samples/{name}.brr',preview=f'samples/{name}.wav'))
    save_json(out/'audio/samples.json',sample_meta)
    chart_data=make_chart(notes,config)
    chart=chart_data['targets'];assist=chart_data['automaticMelodyFills']
    save_json(out/'chart/chart.json',chart_data)
    revision = subprocess.check_output(['git','-C',str(a.emulator_dir),'rev-parse','HEAD'],text=True).strip()
    metadata = dict(format='retrodash-spc-project',version=1,title=config['title'],
        id=config['id'],
        runtimeStatus='offline-verified; not supported by current CQP firmware loader',
        source='music/source.spc',audio=dict(engine='sfc-dsp-event-replay',clockHz=1024000,
        sampleRate=32000,channels=2,aram='audio/aram.bin',registers='audio/dsp-registers.bin',
        events='audio/events.sde',samples='audio/samples.json',notes='audio/voices.json'),
        chart='chart/chart.json',art='art/art.json',scene='scene/scene.json',coverImage='cover',
        credits='licenses/credits.txt',durationSeconds=seconds,
        sourceTags=dict(title=data[0x2e:0x4e].split(b'\0')[0].decode(),
            game=data[0x4e:0x6e].split(b'\0')[0].decode(),
            artist=data[0xb1:0xd1].split(b'\0')[0].decode(),fadeMilliseconds=int(data[0xac:0xb1].rstrip(b'\0 ') or b'0')),
        emulator=dict(repository='https://github.com/blarggs-audio-libraries/snes_spc',revision=revision))
    save_json(out/'manifest.json',metadata)
    counts=collections.Counter(n['voice'] for n in notes)
    report=dict(sourceSha256=hashlib.sha256(data).hexdigest(),durationSeconds=seconds,
        rawRegisterWrites=len(events),changedRegisterWrites=len(changed),
        clockEventBytes=len(changed_bytes),deltaEventBytes=len(packed),
        brrBytes=sum(x['brrBytes'] for x in sample_meta),
        noteOnCount=len(notes),voiceNoteCounts=dict(counts),manualTargets=len(chart),
        automaticMelodyFills=len(assist),
        comparison=dict(equal=equal,comparedStereoFrames=(len(reference)-16)//4,
            skippedFullEmulatorLeadingFrames=4,
            description='Changed-write DSP-only replay vs full SPC emulation, exact PCM comparison'),
        validationScope='offline conversion and PCM comparison only; hardware validation is separate')
    save_json(out/'verification/report.json',report)
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()
