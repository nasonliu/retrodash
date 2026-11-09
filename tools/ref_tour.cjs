// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joakim (joakimskurk)
// The reference's engine (reference/vavehero.html) playing the engine tour of tests/kf/kf_render.c: every patch
// alone, every kit, rendered offline in Chromium. Its levels are the fasit for the firmware's engine
// (tests/kf/parity.py). Also the compressor's static curve (--comp), to fit kf_comp_gain.
//   node tools/ref_tour.cjs [outdir]      -> outdir/ref-tour.json, ref-tour.wav (default build/audio)
//   node tools/ref_tour.cjs --comp        the DynamicsCompressor's gain for a sine at -40..+6 dBFS
//   node tools/ref_tour.cjs --songs [S]   each built-in song as renderSong plays it (S seconds, default 30): ref-songN.json
const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

const URL = 'file://' + path.resolve(__dirname, '..', 'reference', 'vavehero.html');
const args = process.argv.slice(2);
const OUT = path.resolve(args.find((a, i) => !a.startsWith('--') && args[i - 1] !== '--songs') || path.join(__dirname, '..', 'build', 'audio'));
fs.mkdirSync(OUT, { recursive: true });

(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto(URL);
  await page.waitForFunction(() => window.KF && window.KF.makeEngine);
  if (args.includes('--songs')) {                     /* each built-in song as renderSong plays it: its levels every 4 bars */
    const secs = +(args[args.indexOf('--songs') + 1] || 30);
    const res = await page.evaluate(async (secs) => {
      const out = [];
      for (let i = 0; i < 4; i++) {
        const song = KF.SONGS[i], r = await KF.renderSong(i, secs), bin = atob(r.wav), n = (bin.length - 44) / 4, bar = 4 * 60 / song.bpm, rows = [];
        const v = new DataView(new ArrayBuffer(bin.length)); for (let k = 0; k < bin.length; k++) v.setUint8(k, bin.charCodeAt(k));
        for (let t0 = .05, k = 0; t0 + 4 * bar <= secs + 1e-6; t0 += 4 * bar, k++) {
          const a = Math.round(t0 * 44100), b = Math.min(n, Math.round((t0 + 4 * bar) * 44100));
          let sum = 0, dsum = 0, peak = 0, pl = 0, pr = 0;
          for (let j = a; j < b; j++) { const l = v.getInt16(44 + 4 * j, true), rr = v.getInt16(46 + 4 * j, true); sum += l * l + rr * rr; dsum += (l - pl) ** 2 + (rr - pr) ** 2; pl = l; pr = rr; peak = Math.max(peak, Math.abs(l), Math.abs(rr)) }
          sum /= 2 * (b - a) * 32768 * 32768; dsum /= 2 * (b - a) * 32768 * 32768;
          rows.push({ name: `bars ${4 * k + 1}-${4 * k + 4}`, rms_db: +(10 * Math.log10(sum + 1e-12)).toFixed(2), peak: +(peak / 32768).toFixed(4), bright: +Math.sqrt(dsum / (sum + 1e-12)).toFixed(4) });
        }
        out.push({ title: song.title, rows, wav: r.wav });
      }
      return out;
    }, secs);
    res.forEach((s, i) => {
      fs.writeFileSync(path.join(OUT, `ref-song${i}.json`), JSON.stringify(s.rows, null, 1) + '\n');
      fs.writeFileSync(path.join(OUT, `ref-song${i}.wav`), Buffer.from(s.wav, 'base64'));
      console.log(s.title, s.rows.map(r => r.rms_db.toFixed(1)).join(' '));
    });
    await browser.close();
    return;
  }
  if (args.includes('--comp')) {
    const rows = await page.evaluate(async () => {
      const out = [];
      for (let db = -40; db <= 6; db += 2) {
        const sr = 44100, ctx = new OfflineAudioContext(1, sr * 2, sr), o = ctx.createOscillator(), g = ctx.createGain(), c = ctx.createDynamicsCompressor();
        c.threshold.value = -10; c.knee.value = 6; c.ratio.value = 3; c.attack.value = .004; c.release.value = .2;
        o.frequency.value = 220; g.gain.value = Math.pow(10, db / 20); o.connect(g); g.connect(c); c.connect(ctx.destination); o.start();
        const b = await ctx.startRendering(), d = b.getChannelData(0);
        let pk = 0; for (let i = sr; i < 2 * sr; i++) pk = Math.max(pk, Math.abs(d[i]));
        out.push([db, 20 * Math.log10(pk)]);
      }
      return out;
    });
    for (const [i, o] of rows) console.log(`in ${String(i).padStart(4)} dB  out ${o.toFixed(2)} dB  gain ${(o - i).toFixed(2)} dB`);
    await browser.close();
    return;
  }
  const res = await page.evaluate(async () => {
    const KF = window.KF, sr = 44100, PATCH_S = 3.0, KIT_S = 4.0;
    const parts = ['PLAY', 'BASS', 'PAD', 'ARP', 'LEAD', 'KEYS'], BASE = { PLAY: 72, BASS: 36, PAD: 60, ARP: 72, LEAD: 72, KEYS: 60 };
    const HITS = [36, 38, 39, 37, 42, 46, 49, 41, 45, 47, 48, 50];
    const patches = []; for (const part of parts) for (const name of KF.PATCHSET[part]) patches.push({ part, name });
    const kits = KF.PATCHSET.DRUMS;
    const secs = .1 + patches.length * PATCH_S + kits.length * KIT_S + .5;
    const ctx = new OfflineAudioContext(2, Math.ceil(sr * secs), sr), E = KF.makeEngine(ctx);
    E.player.gain.value = .5 + 60 / 100; E.back.gain.value = 1.5 - 60 / 100; E.setTempo(112); E.setGrit(70);
    let s = .1;
    for (const { part, name } of patches) {
      [0, 4, 7, 12].forEach((iv, j) => E.note(name, s + j * .4, BASE[part] + iv, 100, { dur: .35 }));
      s += PATCH_S;
    }
    for (const kit of kits) { HITS.forEach((n, j) => E.drum(n, s + j * .2, 100, kit)); s += KIT_S; }
    const buf = await ctx.startRendering(), L = buf.getChannelData(0), R = buf.getChannelData(1);
    const q = x => Math.max(-32767, Math.min(32767, Math.round(x * 32767)));
    const stats = (name, t0, t1) => {
      const a = Math.round(t0 * sr), b = Math.min(L.length, Math.round(t1 * sr));
      let sum = 0, dsum = 0, peak = 0, pl = 0, pr = 0;
      for (let i = a; i < b; i++) {
        const l = q(L[i]), r = q(R[i]);
        sum += l * l + r * r; dsum += (l - pl) ** 2 + (r - pr) ** 2; pl = l; pr = r;
        peak = Math.max(peak, Math.abs(l) / 32768, Math.abs(r) / 32768);
      }
      sum /= 2 * (b - a) * 32768 * 32768; dsum /= 2 * (b - a) * 32768 * 32768;
      return { name, rms_db: +(10 * Math.log10(sum + 1e-12)).toFixed(2), peak: +peak.toFixed(4), bright: +Math.sqrt(dsum / (sum + 1e-12)).toFixed(4) };
    };
    const rows = []; s = .1;
    for (const { name } of patches) { rows.push(stats(name, s, s + PATCH_S)); s += PATCH_S; }
    for (const kit of kits) { rows.push(stats(kit, s, s + KIT_S)); s += KIT_S; }
    const pcm = new Int16Array(L.length * 2);
    for (let i = 0; i < L.length; i++) { pcm[2 * i] = q(L[i]); pcm[2 * i + 1] = q(R[i]); }
    let bin = ''; const u8 = new Uint8Array(pcm.buffer);
    for (let i = 0; i < u8.length; i += 0x8000) bin += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
    return { rows, pcm: btoa(bin), frames: L.length };
  });
  fs.writeFileSync(path.join(OUT, 'ref-tour.json'), JSON.stringify(res.rows, null, 1) + '\n');
  const data = Buffer.from(res.pcm, 'base64'), h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + data.length, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20);
  h.writeUInt16LE(2, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(44100 * 4, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
  h.write('data', 36); h.writeUInt32LE(data.length, 40);
  fs.writeFileSync(path.join(OUT, 'ref-tour.wav'), Buffer.concat([h, data]));
  for (const r of res.rows) console.log(`  ${r.name.padEnd(12)} rms ${r.rms_db.toFixed(2).padStart(6)} dB  peak ${r.peak.toFixed(3)}  bright ${r.bright.toFixed(3)}`);
  console.log('json   ->', path.join(OUT, 'ref-tour.json'));
  await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
