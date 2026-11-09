// Plays reference/vavehero.html in Chromium: both songs on autoplay, a match against the bot, and two windows over LINK.
// node tests/reference.cjs [outdir]     (Playwright with Chromium; the screens land in outdir, default build/test)
const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

const URL = 'file://' + path.resolve(__dirname, '..', 'reference', 'vavehero.html');
const OUT = path.resolve(process.argv[2] || path.join(__dirname, '..', 'build', 'test'));
fs.mkdirSync(OUT, { recursive: true });
let failures = 0;
const check = (ok, what) => { console.log((ok ? 'ok   ' : 'FAIL ') + what); if (!ok) failures++; };
const sleep = ms => new Promise(r => setTimeout(r, ms));
/* a test song as a DAW might write it: format 0, running status, note-on velocity 0 as note-off */
function smf0(title, bpm, events, ppq = 96, mark = null) {   /* mark: a marker meta event's text */
  events.sort((x, y) => x[0] - y[0] || (x[1][2] === 0 ? -1 : 1) - (y[1][2] === 0 ? -1 : 1));
  const vlq = n => { const o = [n & 127]; while ((n >>= 7)) o.unshift((n & 127) | 128); return o; };
  const us = Math.round(60e6 / bpm), name = [...Buffer.from(title)];
  const body = [0, 0xFF, 0x51, 3, (us >> 16) & 255, (us >> 8) & 255, us & 255, 0, 0xFF, 0x03, name.length, ...name];
  if (mark) body.push(0, 0xFF, 0x06, Buffer.from(mark).length, ...Buffer.from(mark));
  let last = 0, run = 0;
  for (const [tick, bytes] of events) {
    body.push(...vlq(tick - last)); last = tick;
    if (bytes[0] === run) body.push(...bytes.slice(1)); else { body.push(...bytes); run = bytes[0]; }
  }
  body.push(0, 0xFF, 0x2F, 0);
  const n = body.length;
  return Buffer.from([...Buffer.from('MThd'), 0, 0, 0, 6, 0, 0, 0, 1, ppq >> 8, ppq & 255, ...Buffer.from('MTrk'), (n >>> 24) & 255, (n >> 16) & 255, (n >> 8) & 255, n & 255, ...body]);
}
function emDrive(mark) {                   /* E minor: needs a transpose, an octave climb, a chromatic note, a big chord */
  const ppq = 96, ev = [], note = (ch, beat, pitch, len, vel = 100) => {
    ev.push([Math.round(beat * ppq), [0x90 | ch, pitch, vel]], [Math.round((beat + len) * ppq), [0x90 | ch, pitch, 0]]);
  };
  const up = [64, 66, 67, 69, 71, 72, 74, 76];
  up.forEach((p, i) => note(0, 8 + i, p, .9));                        /* bars 2-3: E4 .. E5 */
  up.forEach((p, i) => note(0, 16 + i, p + 12, .9));                  /* bars 4-5: E5 .. E6 */
  up.slice().reverse().forEach((p, i) => note(0, 24 + i, p, .9));     /* bars 6-7: back down */
  note(0, 32, 70, 1);                                                  /* A#4: chromatic, lands on a black key */
  [64, 67, 71, 76].forEach(p => note(0, 36, p, 1));                   /* a four-note chord */
  note(0, 40, 64, 2); note(0, 44, 67, 1); note(0, 45, 71, 1);         /* a hold, and two more */
  for (let b = 0; b < 48; b += .5) note(1, b, 40, .4, 96);             /* BASS on channel 2 */
  for (let b = 0; b < 48; b++) note(9, b, b % 2 ? 38 : 36, .1, 110);  /* drums on channel 10 */
  note(6, 4, 60, 1);                                                   /* channel 7: no instrument */
  note(15, 4, 60, 1);                                                  /* channel 16: LINK's */
  note(1, 0, 1, .1); note(1, 16, 0, .1);                              /* keyswitches on channel 2: FM BASS, then PULSE */
  note(1, 8, 7, .1);                                                   /* ...and one with no patch (BASS has three) */
  note(0, 16, 11, 8);                                                  /* the OP note, held over bars 5-6 */
  return smf0('Em Drive', 100, ev, ppq, mark);
}

async function open(ctx) {
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  await page.goto(URL);
  await page.waitForFunction(() => !!window.KF);
  return { page, errors };
}
async function screen(page, name) {   // the 240 x 240 screen, as drawn
  const url = await page.evaluate(() => document.getElementById('scr').toDataURL('image/png'));
  fs.writeFileSync(path.join(OUT, name + '.png'), Buffer.from(url.split(',')[1], 'base64'));
}
async function waitBeat(page, b, timeout = 90000) {
  await page.waitForFunction(b => window.KF.play && window.KF.play.beat >= b, b, { timeout, polling: 20 });
}
async function waitResults(page, timeout = 90000) {
  await page.waitForFunction(() => window.KF.G.scr === 'results', null, { timeout, polling: 100 });
}
async function choose(page, song) {                       /* a song's level is its own (D12): no level to choose */
  await page.evaluate(s => { window.KF.G.sel = s; }, song);
}

(async () => {
  const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required'] });
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 1000 }, deviceScaleFactor: 1 });

  /* 1. one player, autoplay: every note must be PERFECT, both OCT shifts hit, OP BOOST used */
  const a = await open(ctx);
  await a.page.waitForTimeout(300);
  await screen(a.page, '01-splash');
  const art = await a.page.evaluate(() => { const d = document.getElementById('scr').getContext('2d').getImageData(120, 80, 1, 1).data; return [d[0], d[1], d[2]]; });
  check(art[0] > 200 && art[1] > 120, 'the splash shows its art: the sun at (120, 80) is ' + art.join(','));
  const autoWas = await a.page.evaluate(() => window.KF.G.auto);
  await a.page.keyboard.press('Space');
  await a.page.waitForTimeout(1500);
  const dm = await a.page.evaluate(() => ({ scr: window.KF.G.scr, demo: !!(window.KF.play && window.KF.play.demo), beat: window.KF.play ? window.KF.play.beat : -1, title: window.KF.play ? window.KF.play.song.title : '' }));
  check(dm.scr === 'play' && dm.demo && dm.title === 'ENGINE TOUR' && dm.beat >= 160 && dm.beat < 166, 'FX on the splash: the DEMO, ENGINE TOUR from bar 41 on autoplay: ' + JSON.stringify(dm));
  await a.page.keyboard.press('Enter');
  const back = await a.page.evaluate(() => ({ scr: window.KF.G.scr, play: !!window.KF.play, auto: window.KF.G.auto }));
  check(back.scr === 'splash' && !back.play && back.auto === autoWas, 'PLAY ends the DEMO: back on the splash, autoplay as it was');
  await a.page.keyboard.press('Enter');
  check(await a.page.evaluate(() => window.KF.G.scr) === 'songs', 'PLAY on the splash opens SONGS');
  await screen(a.page, '02-songs');
  const charts = await a.page.evaluate(() => window.KF.SONGS.map(s => ({ title: s.title, level: window.KF.LEVELS[s.level], notes: window.KF.buildChart(s).notes.length, melody: s.melody.length })));
  check(charts.map(c => c.title + ' ' + c.level).join() === 'FIRST LIGHT EASY,NEON CARRIER MED,ENGINE TOUR HARD,CHROME HORIZON MED' && charts.every(c => c.notes > 10 && c.notes === c.melody),
    'D12: one chart a song, at its own level, the play track as written: ' + JSON.stringify(charts));
  const rates = await a.page.evaluate(() => window.KF.SONGS.map(s => ({ t: s.title, r: +s.rate.rating.toFixed(2), l: s.rate.level, own: s.level })));
  check(rates.every(x => x.l === x.own), 'D13: the built-in songs have no LEVEL marker, their levels are counted from their notes over time: ' + JSON.stringify(rates));
  const unit = await a.page.evaluate(() => [[2, 1], [1, 1], [.25, .2]].map(([step, len]) =>   /* 120 BPM, 32 beats: 1, 2, 8 notes a second */
    window.KF.noteRate(Array.from({ length: 32 / step }, (_, i) => ({ t: 4 + i * step, len, pitch: 60 })), 120)).map(r => [+r.avg.toFixed(2), +r.peak.toFixed(2), r.level]));
  check(JSON.stringify(unit) === '[[1.03,1,0],[2,2,1],[8.01,8,3]]', 'D13: a note every second beat at 120 BPM is EASY, one a beat MED, four a beat EXP: ' + JSON.stringify(unit));
  const blacks = await a.page.evaluate(() => window.KF.SONGS.flatMap(s => window.KF.buildChart(s).notes).filter(n => [1, 3, 6, 8, 10].includes(((n.pitch % 12) + 12) % 12)).length);
  check(blacks === 0, 'D1: no black keys in any chart');

  await a.page.evaluate(() => window.KF.setAuto(true));
  await choose(a.page, 0); await a.page.keyboard.press('Enter');
  await waitBeat(a.page, 2); await screen(a.page, '03-first-light-intro');
  await waitResults(a.page);
  let L = await a.page.evaluate(() => window.KF.G.last);
  check(L.acc === 100 && L.counts.M === 0 && L.grade === 'S', `FIRST LIGHT (EASY) on autoplay: ${L.acc.toFixed(1)}% ${L.grade}, ${L.counts.M} misses`);
  check(Math.abs(L.meanDt) < 5, `mean timing ${L.meanDt.toFixed(2)} ms`);
  await screen(a.page, '04-first-light-results');

  await a.page.keyboard.press('Escape');
  await choose(a.page, 1); await a.page.keyboard.press('Enter');
  let boosted = false;
  await waitBeat(a.page, 22); await screen(a.page, '05-neon-verse'); await a.page.screenshot({ path: path.join(OUT, '05b-page-play.png') });
  await waitBeat(a.page, 37.9); await screen(a.page, '06-neon-oct-band');
  await waitBeat(a.page, 44); await screen(a.page, '07-neon-upper-octave');
  for (let i = 0; i < 400 && !boosted; i++) { boosted = await a.page.evaluate(() => !!(window.KF.play && window.KF.play.boostTo != null)); if (!boosted) await sleep(100); }
  check(boosted, 'OP BOOST was activated');
  if (boosted) { await sleep(150); await screen(a.page, '08-neon-boost'); }
  await waitBeat(a.page, 84); await screen(a.page, '09-neon-chorus');
  await waitResults(a.page);
  L = await a.page.evaluate(() => window.KF.G.last);
  check(L.acc === 100 && L.counts.S === 2 && L.counts.M === 0, `NEON CARRIER (MED) on autoplay: ${L.acc.toFixed(1)}%, ${L.counts.S} shifts, ${L.counts.M} misses, full combo ${L.full}`);
  await screen(a.page, '10-neon-results');

  /* 1b. the built-in songs went through the MIDI import: octave shifts placed by the import, OP phrases from the OP note */
  const built = await a.page.evaluate(() => window.KF.SONGS.map(s => ({ title: s.title, T: s.transpose, octs: s.octs.map(e => e.t + ':' + e.dir), ph: s.phrases.length, parts: s.parts, builtIn: s.builtIn })));
  check(built[1].octs.join() === '39.5:1,71.5:-1' && built[1].T === 0 && built[1].ph === 6 && built[0].ph === 3,
    `the built-in MIDI files import as before: ${JSON.stringify(built.map(b => [b.title, b.octs, b.ph]))}`);
  check(['BASS', 'PAD', 'ARP', 'LEAD', 'DRUMS'].every(k => built[1].parts[k] > 0), 'NEON CARRIER has bass, pad, arp, lead and drums: ' + JSON.stringify(built[1].parts));
  const pat = await a.page.evaluate(() => { const K = window.KF, n = K.SONGS[1], f = K.SONGS[0], at = (s, part, b) => K.PATCHSET[part][K.songPatchAt(s, part, b)];
    return [at(n, 'BASS', 20), at(n, 'BASS', 80), at(n, 'PAD', 50), at(n, 'PLAY', 50), at(n, 'PLAY', 80), at(n, 'ARP', 80), at(f, 'PLAY', 10), at(f, 'BASS', 10)].join(); });
  check(pat === 'PULSE,FM BASS,STRINGS,BELL EP,SYN LEAD,BELL,BELL EP,SUB', 'D6: the built-in songs switch patches with keyswitches: ' + pat);
  const audible = await a.page.evaluate(async () => {           /* every patch and kit, alone, offline */
    const K = window.KF, out = [];
    for (const [part, names] of Object.entries(K.PATCHSET)) for (const name of names) {
      const sr = 22050, ctx = new OfflineAudioContext(2, sr * 2, sr), E = K.makeEngine(ctx);
      if (part === 'DRUMS') [36, 38, 42, 39, 45, 49].forEach((d, i) => E.drum(d, .05 + i * .25, 110, name));
      else [57, 60, 64, 67].forEach((m, i) => E.note(name, .05 + i * .35, part === 'BASS' ? m - 24 : m, 104, { dur: .3 }));
      const buf = await ctx.startRendering(), L = buf.getChannelData(0); let sum = 0, pk = 0;
      for (let i = 0; i < L.length; i++) { sum += L[i] * L[i]; pk = Math.max(pk, Math.abs(L[i])); }
      out.push({ part, name, db: Math.round(10 * Math.log10(sum / L.length + 1e-12)), peak: Math.round(pk * 100) / 100 });
    }
    return out;
  });
  const bad = audible.filter(x => x.db < -45 || x.peak >= 1);
  check(!bad.length, `D6: all ${audible.length} patches and kits sound, none clips (${audible.map(x => x.name + ' ' + x.db + ' dB').join(', ')})` + (bad.length ? ' BAD: ' + JSON.stringify(bad) : ''));

  /* 1c. a song of your own: loaded through the file input, transposed to white keys, played perfectly */
  const em = path.join(OUT, 'em-drive.mid'); fs.writeFileSync(em, emDrive());
  await a.page.setInputFiles('#file', em);
  await a.page.waitForFunction(() => window.KF.SONGS.length === 5, null, { timeout: 5000 });
  const imp = await a.page.evaluate(() => { const s = window.KF.SONGS[4]; return { title: s.title, T: s.transpose, octs: s.octs.length, notes: s.melody.length, report: document.getElementById('report').textContent }; });
  check(imp.title === 'EM DRIVE' && imp.T === -5, `EM DRIVE is transposed 5 up onto white keys: ${JSON.stringify({ title: imp.title, T: imp.T })}`);
  check(imp.octs >= 2 && /black keys/.test(imp.report) && /more than three/.test(imp.report), `its climb gets OCT shifts (${imp.octs}), the chromatic note and the fourth chord note are reported`);
  check(/Channel 16/.test(imp.report) && /Channel 7/.test(imp.report), 'channels 7 and 16 are reported as not played');
  check(/Patches: BASS FM BASS from bar 1, PULSE from bar 5/.test(imp.report) && /without a patch: channel 2, note 7/.test(imp.report), 'the keyswitches are reported, and the one with no patch is flagged');
  const emPh = await a.page.evaluate(() => window.KF.SONGS[4].phrases.map(r => r.a + '-' + r.b).join());
  check(emPh === '16-24' && /1 OP phrase from the OP note/.test(imp.report), 'the OP note (B-1 on channel 1, held) makes the OP phrase: ' + emPh);
  const emLevel = await a.page.evaluate(() => { const s = window.KF.SONGS[4]; return { level: s.level, rate: s.rate.level, bars: s.rate.bars.length, graph: !!document.querySelector('#report .rate svg rect') }; });
  check(emLevel.level === emLevel.rate && /from the notes over time \([0-9.]+: [0-9.]+ notes a second over the song, [0-9.]+ in its busiest four bars/.test(imp.report) && emLevel.graph,
    'D13: a file without a LEVEL marker gets the level its notes over time give it, and the report shows them bar by bar: ' + JSON.stringify(emLevel));
  const over = await a.page.evaluate(b64 => { const K = window.KF, u8 = Uint8Array.from(atob(b64), c => c.charCodeAt(0)), r = K.importSong(K.parseMidi(u8), 'em-exp.mid');
    return { level: r.song.level, rate: r.song.rate.level, info: r.report.info.join(' | ') }; }, emDrive('LEVEL EXP').toString('base64'));
  check(over.level === 3 && over.rate === emLevel.rate && /Level: EXP, from the LEVEL marker\. The notes over time alone say/.test(over.info), 'D13: a LEVEL marker overrides the notes: ' + JSON.stringify({ level: over.level, rate: over.rate }));
  await a.page.evaluate(() => window.KF.setAuto(true));
  await choose(a.page, 4); await a.page.keyboard.press('Enter');
  await waitResults(a.page);
  L = await a.page.evaluate(() => window.KF.G.last);
  check(L.acc === 100 && L.counts.M === 0, `EM DRIVE (MED) on autoplay: ${L.acc.toFixed(1)}%, ${L.counts.S} shifts`);

  /* 1d. live MIDI, one part a channel (a sound module for a DAW) */
  const routes = await a.page.evaluate(() => [[0x91, 40, 100], [0x81, 40, 0], [0x99, 38, 110], [0x92, 60, 90], [0x82, 60, 0], [0x96, 60, 100], [0x90, 60, 100], [0x80, 60, 0]].map(m => window.KF.midiIn(m, performance.now())));
  check(JSON.stringify(routes) === JSON.stringify(['BASS', 'BASS', 'DRUMS', 'PAD', 'PAD', null, 'PLAY', 'PLAY']), 'live MIDI: channel 2 BASS, 3 PAD, 10 DRUMS, 1 the player, 7 nothing: ' + JSON.stringify(routes));
  const ks = await a.page.evaluate(() => {
    const K = window.KF, t = performance.now(), r = [];
    K.midiIn([0x91, 1, 100], t); K.midiIn([0x91, 1, 0], t); r.push(K.PATCHSET.BASS[K.LIVE_PATCH[1]]);        /* keyswitch: FM BASS */
    K.midiIn([0x91, 9, 100], t); r.push(K.PATCHSET.BASS[K.LIVE_PATCH[1]]);                                   /* no patch 10: unchanged */
    K.midiIn([0xC2, 2, 0], t); r.push(K.PATCHSET.PAD[K.LIVE_PATCH[2]]);                                      /* a program change: ignored (notes only) */
    K.midiIn([0x99, 2, 100], t); r.push(K.PATCHSET.DRUMS[K.LIVE_PATCH[9]]);                                  /* the BOOM kit */
    K.midiIn([0x90, 3, 100], t); r.push(K.voiceName());                                                       /* channel 1, TONE SONG: GLASS */
    K.midiIn([0x90, 11, 100], t); r.push(K.voiceName());                                                      /* the OP note: no patch change */
    const sounded = K.MM.sent.length; for (const ch of [0, 1, 2, 9]) K.LIVE_PATCH[ch] = 0; return r.join();
  });
  check(ks === 'FM BASS,FM BASS,SUPERSAW,BOOM,GLASS,GLASS', 'D6 live: keyswitch notes move the channel\'s patch; a missing one, a program change and the OP note change nothing: ' + ks);

  /* 1e. the templates download as the files in songs/ */
  const [dl] = await Promise.all([a.page.waitForEvent('download'), a.page.click('[data-tpl="1"]')]);
  const got = fs.readFileSync(await dl.path()), want = fs.readFileSync(path.join(__dirname, '..', 'songs', 'neon-carrier.mid'));
  check(dl.suggestedFilename() === 'neon-carrier.mid' && got.equals(want), 'the NEON CARRIER template downloads as songs/neon-carrier.mid');

  /* 1f. the sound: both songs rendered offline, loud enough and not clipping; the WAVs land next to the screens */
  for (const [i, name] of [[0, 'first-light'], [1, 'neon-carrier']]) {
    const r = await a.page.evaluate(async i => window.KF.renderSong(i, 20), i);
    fs.writeFileSync(path.join(OUT, name + '-20s.wav'), Buffer.from(r.wav, 'base64'));
    check(r.peak < 1 && r.rms > .03, `${name}: 20 s rendered, peak ${r.peak.toFixed(3)}, RMS ${r.rms.toFixed(3)}`);
  }
  await a.page.screenshot({ path: path.join(OUT, '11-page.png') });

  /* 1h. the two songs to hear the engine: they import without a warning, ENGINE TOUR plays every patch, CHROME
     HORIZON (D minor) is transposed onto white keys, and both play 100 % on autoplay, side by side in two windows */
  const demo = await a.page.evaluate(() => window.VAVEHERO_SONGS.slice(2).map(f => {
    const K = window.KF, { song, report } = K.importSong(K.parseMidi(Uint8Array.from(atob(f.data), c => c.charCodeAt(0))), f.file);
    const used = new Set(song.patchLog.map(e => e.part + ':' + e.ks)), all = Object.entries(K.PATCHSET).flatMap(([p, ns]) => ns.map((n, i) => p + ':' + i));
    return { title: song.title, T: song.transpose, warn: report.warn.length, unused: all.filter(x => !used.has(x)).length, octs: song.octs.length, ph: song.phrases.length };
  }));
  check(demo[0].title === 'ENGINE TOUR' && demo[0].warn === 0 && demo[0].unused === 0, 'ENGINE TOUR imports cleanly and uses every patch and kit: ' + JSON.stringify(demo[0]));
  check(demo[1].title === 'CHROME HORIZON' && demo[1].warn === 0 && demo[1].T === 5 && demo[1].octs === 2, 'CHROME HORIZON (D minor) imports cleanly, its play track 5 semitones down on white keys: ' + JSON.stringify(demo[1]));
  const pair = [await open(ctx), await open(ctx)];
  await Promise.all(pair.map(async (x, i) => {
    await x.page.evaluate(i => { window.KF.setAuto(true); window.KF.G.scr = 'songs'; window.KF.G.sel = 2 + i; }, i);
    await x.page.keyboard.press('Enter');
  }));
  await waitBeat(pair[0].page, 92); await screen(pair[0].page, '21-engine-tour'); await screen(pair[1].page, '22-chrome-horizon');
  for (const [i, x] of pair.entries()) {
    await waitResults(x.page, 160000);
    const R = await x.page.evaluate(() => window.KF.G.last);
    check(R.acc === 100 && R.counts.M === 0 && R.counts.S === 2, `${demo[i].title} MED on autoplay: ${R.acc.toFixed(1)}%, ${R.counts.S} shifts, ${R.total} notes`);
    check(x.errors.length === 0, `no page errors playing ${demo[i].title}` + (x.errors.length ? ': ' + x.errors.join(' | ') : ''));
    await x.page.close();
  }
  for (const [i, name] of [[2, 'engine-tour'], [3, 'chrome-horizon']]) {
    const r = await a.page.evaluate(async i => window.KF.renderSong(i, 20), i);
    fs.writeFileSync(path.join(OUT, name + '-20s.wav'), Buffer.from(r.wav, 'base64'));
    check(r.peak < 1 && r.rms > .03, `${name}: 20 s rendered, peak ${r.peak.toFixed(3)}, RMS ${r.rms.toFixed(3)}`);
  }

  /* 1i. GRIT (D7): the kick pumps the band, GRIT 0 does not; the songs stay under clipping from clean to dirtiest */
  const pump = await a.page.evaluate(async () => {
    const K = window.KF, res = {};
    for (const gr of [0, 70]) {
      const sr = 22050, ctx = new OfflineAudioContext(1, sr * 2, sr), E = K.makeEngine(ctx); E.setGrit(gr);
      for (const m of [57, 60, 64]) E.note('SUPERSAW', 0, m, 100, { dur: 1.9 });
      for (const t of [.8, 1.2, 1.6]) E.pump(t);                                       /* the kick's duck, without its sound */
      const L = (await ctx.startRendering()).getChannelData(0), rms = (a, b) => { let s = 0; for (let i = Math.floor(a * sr); i < b * sr; i++) s += L[i] * L[i]; return Math.sqrt(s / ((b - a) * sr)); };
      res[gr] = [.8, 1.2, 1.6].map(t => rms(t + .03, t + .07) / rms(t - .09, t - .01)).reduce((x, y) => x + y) / 3;
    }
    return res;
  });
  check(pump[70] / pump[0] < .75 && pump[0] > .8, `GRIT 70 pumps the band on the kick: right after it, ${(pump[70] / pump[0]).toFixed(2)} of the level GRIT 0 has there (${pump[70].toFixed(2)} against ${pump[0].toFixed(2)})`);
  for (const gr of [0, 100]) {
    await a.page.evaluate(g => window.KF.setGrit(g), gr);
    const r = await a.page.evaluate(async () => window.KF.renderSong(3, 20));
    check(r.peak < 1 && r.rms > .03, `CHROME HORIZON at GRIT ${gr}: peak ${r.peak.toFixed(3)}, RMS ${r.rms.toFixed(3)}`);
  }
  await a.page.evaluate(() => window.KF.setGrit(70));
  check(await a.page.evaluate(() => window.KF.engine.grit === 70 && document.getElementById('gritV').textContent === '70'), 'the GRIT setting reaches the engine and the side panel');

  /* 1g. MIDI MODE (D5): a DAW plays into the FM-1 */
  await a.page.evaluate(() => { window.KF.setAuto(false); window.KF.G.scr = 'songs'; window.KF.G.sel = window.KF.SONGS.length; });
  await screen(a.page, '18-songs-midi-row');
  await a.page.keyboard.press('Enter');
  check(await a.page.evaluate(() => window.KF.G.scr === 'midi' && window.KF.MM.mode === 'LIVE'), 'the last row of SONGS opens MIDI MODE, LIVE');
  const clk = await a.page.evaluate(() => {                     /* a DAW's clock at 120 BPM: 48 pulses a second, after START */
    const t0 = performance.now(); window.KF.midiIn([0xFA], t0);
    for (let i = 0; i < 48; i++) window.KF.midiIn([0xF8], t0 + i * 1000 / 48);
    return { bpm: window.KF.MM.bpm, count: window.KF.MM.count, running: window.KF.MM.running };
  });
  check(Math.abs(clk.bpm - 120) < .5 && clk.count === 48 && clk.running, `MIDI clock: ${clk.bpm} BPM, ${clk.count} pulses, running`);
  const live = await a.page.evaluate(() => {
    const t = performance.now(), K = window.KF;
    K.midiIn([0x91, 40, 100], t); K.midiIn([0x90, 64, 100], t); K.midiIn([0x99, 36, 110], t);
    const r = { act: Object.keys(K.MM.act).map(Number).sort((a, b) => a - b).join(), ledE4: K.leds()[11] };
    K.midiIn([0x81, 40, 0], t + 5); K.midiIn([0x80, 64, 0], t + 5);
    K.pressKey('test-echo', 7, t, 'test'); K.releaseNote('test-echo', t + 10);
    r.sent = K.MM.sent.slice(-2).map(m => m.join(' ')); return r;
  });
  check(live.act === '0,1,9' && live.ledE4 === 2, `LIVE: channels 1, 2 and 10 light up, and the DAW's E4 on channel 1 lights the E4 key: ${JSON.stringify(live)}`);
  check(live.sent.join() === '144 60 100,128 60 0', 'the FM-1 key C4 goes out on channel 1 for the DAW to record: ' + live.sent.join(' | '));
  await a.page.evaluate(() => { window.KF.midiIn([0x91, 45, 90], performance.now()); window.KF.midiIn([0x92, 57, 80], performance.now()); window.KF.midiIn([0x90, 61, 90], performance.now()); });
  await a.page.evaluate(() => window.KF.midiIn([0x91, 1, 100], performance.now()));   /* FM BASS on channel 2, for the picture */
  await sleep(120); await screen(a.page, '19-midi-live');
  await a.page.evaluate(() => { window.KF.LIVE_PATCH[1] = 0; });
  await a.page.evaluate(() => { window.KF.midiIn([0x81, 45, 0], performance.now()); window.KF.midiIn([0x82, 57, 0], performance.now()); window.KF.midiIn([0x80, 61, 0], performance.now()); });
  await a.page.keyboard.press('Enter');                          /* PLAY: PREVIEW */
  const pv = await a.page.evaluate(() => {
    const K = window.KF; K.setAuto(true);
    const t = performance.now(), beat = K.play.beat, bms = 60000 / K.play.bpm;
    K.midiIn([0x90, 60, 100], t); K.midiIn([0x90, 60, 0], t + bms * .5);                 /* C4 */
    K.midiIn([0x90, 61, 100], t + bms * .5); K.midiIn([0x90, 61, 0], t + bms * .9);      /* C#4: a black key */
    K.midiIn([0x90, 84, 100], t + bms); K.midiIn([0x90, 84, 0], t + bms * 3);            /* C6: a new window, a hold */
    K.midiIn([0x91, 40, 100], t);                                                          /* the band, a bar late */
    return { live: !!K.play.live, notes: K.play.notes.map(n => [Math.round((n.t - beat) * 10) / 10, n.pitch, n.hold]), ghosts: K.play.ghosts.length, octs: K.play.octs.length, pending: K.MM.pending.size };
  });
  check(pv.live && JSON.stringify(pv.notes.map(n => n.slice(0, 2))) === '[[4,60],[5,84]]' && pv.ghosts === 1 && pv.octs === 1 && pv.notes[1][2] === true,
    `PREVIEW: channel 1 lands a bar ahead (C4, then C6 as a hold after an OCT shift), C#4 is shown as a warning: ${JSON.stringify(pv)}`);
  await sleep(1400); await screen(a.page, '20-midi-preview');
  await sleep(2600);
  const pr = await a.page.evaluate(() => ({ P: window.KF.play.counts.P, M: window.KF.play.counts.M, S: window.KF.play.counts.S, combo: window.KF.play.combo }));
  check(pr.P === 2 && pr.M === 0 && pr.S === 1, `PREVIEW: playing along on autoplay hits both notes and the shift: ${JSON.stringify(pr)}`);
  await a.page.keyboard.press('Escape');
  check(await a.page.evaluate(() => window.KF.G.scr === 'songs' && !window.KF.MM.on && !window.KF.play), 'HOME leaves MIDI MODE for SONGS');
  await a.page.evaluate(() => { document.querySelector('input[name=link][value=bot]').click(); window.KF.G.sel = window.KF.SONGS.length; });
  await a.page.keyboard.press('Enter');
  check(await a.page.evaluate(() => window.KF.G.scr === 'songs' && !window.KF.MM.on), 'MIDI MODE stays shut while LINK is on (the clock and START would clash)');
  await a.page.evaluate(() => { document.querySelector('input[name=link][value=off]').click(); window.KF.setAuto(false); });
  check(a.errors.length === 0, 'no page errors' + (a.errors.length ? ': ' + a.errors.join(' | ') : ''));

  /* 2. a wrong key breaks the combo, a late press is GOOD, not PERFECT */
  await a.page.evaluate(() => { window.KF.setAuto(false); window.KF.G.scr = 'songs'; });
  await choose(a.page, 0); await a.page.keyboard.press('Enter');
  await waitBeat(a.page, 7.5);
  const probe = await a.page.evaluate(() => {
    const p = window.KF.play, n = p.notes[0];               /* C4 at beat 8 */
    return { t: n.t, pitch: n.pitch, state: n.state };
  });
  await waitBeat(a.page, 8.03);
  const after = await a.page.evaluate(() => {                    /* C4, stamped exactly 80 ms after it was due */
    const p = window.KF.play, bms = 60000 / p.bpm, ts = performance.now() - (p.beat - 8) * bms + 80 + window.KF.G.offset;
    window.KF.pressKey('test-late', 7, ts, 'test'); window.KF.releaseNote('test-late', ts + 50);
    return { state: p.notes[0].state, judge: p.judge && p.judge.text, combo: p.combo };
  });
  check(probe.pitch === 60 && after.state === 'hit' && after.judge === 'GOOD', `C4 80 ms late is GOOD: ${JSON.stringify(after)}`);
  await waitBeat(a.page, 8.95);
  await a.page.keyboard.down('KeyK'); await a.page.keyboard.up('KeyK');  /* C5 where E4 is due */
  const wrong = await a.page.evaluate(() => ({ judge: window.KF.play.judge && window.KF.play.judge.text, combo: window.KF.play.combo }));
  check(wrong.judge === 'WRONG' && wrong.combo === 0, `a wrong key breaks the combo: ${JSON.stringify(wrong)}`);
  await waitBeat(a.page, 11.93);                                 /* G4 is due at beat 12 (bar 3, beat 0)... */
  const black = await a.page.evaluate(() => {                    /* ...and a black key (C#4) next to it changes nothing */
    const p = window.KF.play, before = { combo: p.combo, judge: p.judge && p.judge.text, t0: p.judge && p.judge.t0 };
    window.KF.pressKey('test-black', 8, performance.now(), 'test'); window.KF.releaseNote('test-black', performance.now());
    return { before, combo: p.combo, judge: p.judge && p.judge.text, same: (p.judge && p.judge.t0) === before.t0 };
  });
  check(black.same && black.combo === black.before.combo, `D1: a black key in play is ignored: ${JSON.stringify(black)}`);
  await a.page.keyboard.press('Escape');

  /* 2b. SPREAD (D2): every other white key. Autoplay is still perfect, the LEDs light only the spread keys,
     a key between them does nothing, and the spread key of the lane hits. */
  await a.page.evaluate(() => { document.getElementById('spread').click(); window.KF.setAuto(true); });
  check(await a.page.evaluate(() => window.KF.G.spread) === true, 'SPREAD turns on');
  await choose(a.page, 1); await a.page.keyboard.press('Enter');
  await waitBeat(a.page, 8);                                     /* after the intro, when the notes come */
  let ledsOk = true, seen = new Set();
  for (let i = 0; i < 40; i++) {
    const r = await a.page.evaluate(() => ({ L: window.KF.leds(), lay: window.KF.LAYOUTS.spread }));
    r.L.forEach((v, k) => { if (v) { seen.add(k); if (!r.lay.includes(k)) ledsOk = false; } });
    await sleep(120);
  }
  check(ledsOk && seen.size >= 4, `SPREAD: the LEDs light only F3 A3 C4 E4 G4 B4 D5 F5 (saw keys ${[...seen].sort((x, y) => x - y).join(' ')})`);
  await screen(a.page, '05c-spread'); await a.page.screenshot({ path: path.join(OUT, '05d-page-spread.png') });
  await waitResults(a.page);
  L = await a.page.evaluate(() => window.KF.G.last);
  check(L.acc === 100 && L.counts.M === 0, `NEON CARRIER (MED) with SPREAD on autoplay: ${L.acc.toFixed(1)}%`);
  await a.page.evaluate(() => window.KF.setAuto(false));
  await a.page.keyboard.press('Escape'); await choose(a.page, 0); await a.page.keyboard.press('Enter');
  await waitBeat(a.page, 7.97);
  const sp = await a.page.evaluate(() => {                       /* C4 is due at beat 8: lane 0, the F3 key with SPREAD */
    const p = window.KF.play, bms = 60000 / p.bpm, at = performance.now() - (p.beat - 8) * bms + window.KF.G.offset;
    const t0 = p.judge && p.judge.t0;
    window.KF.pressKey('sp-gap', 9, at, 'test'); window.KF.releaseNote('sp-gap', at + 30);       /* D4: between two spread keys */
    const gapIgnored = (p.judge && p.judge.t0) === t0 && p.notes[0].state === 'pending';
    window.KF.pressKey('sp-hit', 0, at, 'test'); window.KF.releaseNote('sp-hit', at + 30);       /* F3: lane 0 */
    return { gapIgnored, state: p.notes[0].state, judge: p.judge && p.judge.text };
  });
  check(sp.gapIgnored && sp.state === 'hit' && sp.judge === 'PERFECT', `SPREAD: the key between does nothing, F3 plays lane 0: ${JSON.stringify(sp)}`);
  await a.page.keyboard.press('Escape');
  await a.page.evaluate(() => document.getElementById('spread').click());
  check(await a.page.evaluate(() => window.KF.G.spread) === false, 'SPREAD turns off');

  /* 3. against the bot: the protocol's receive path, and a winner */
  await a.page.evaluate(() => { document.querySelector('input[name=link][value=bot]').click(); window.KF.setAuto(true); });
  await choose(a.page, 0); await a.page.keyboard.press('Enter');
  await waitBeat(a.page, 30); await screen(a.page, '12-vs-bot');
  await waitResults(a.page); await sleep(300);
  const vs = await a.page.evaluate(() => ({ me: window.KF.G.last.score, bot: window.KF.Link.rival.score, done: window.KF.Link.rival.done, vs: window.KF.G.last.vs }));
  check(vs.vs && vs.done && vs.bot > 0 && vs.me >= vs.bot, `VS bot: me ${vs.me}, bot ${vs.bot}`);
  await screen(a.page, '13-vs-bot-results');
  await a.page.evaluate(() => document.querySelector('input[name=link][value=off]').click());

  /* 4. two windows over LINK (BroadcastChannel): roles, P1 starts both, scores cross, STOP brings P2 home */
  const p1 = a, p2 = await open(ctx);
  for (const x of [p1, p2]) {
    await x.page.bringToFront();
    await x.page.evaluate(() => { window.KF.Link.id = 5; document.querySelector('input[name=link][value=tab]').click(); });   /* the same id on purpose */
    await x.page.keyboard.press('Escape'); await x.page.keyboard.press('Escape'); await x.page.keyboard.press('Enter');
  }
  await sleep(4200);
  const roles = await Promise.all([p1, p2].map(x => x.page.evaluate(() => ({ role: window.KF.Link.role(), linked: window.KF.Link.linked(), id: window.KF.Link.id }))));
  check(roles[0].linked && roles[1].linked && roles[0].role !== roles[1].role && roles[0].id !== roles[1].id, 'two windows that start with the same id draw again and link up as P1 and P2: ' + JSON.stringify(roles));
  check(await p2.page.evaluate(() => window.KF.SONGS.length) === 5, 'a song loaded in one window is kept for the next (EM DRIVE in the second window)');
  const lead = roles[0].role === 'P1' ? p1 : p2, follow = lead === p1 ? p2 : p1;
  await lead.page.evaluate(() => window.KF.setAuto(true));
  await follow.page.evaluate(() => window.KF.setAuto(true));
  await choose(lead.page, 1); await sleep(1200);
  check(await follow.page.evaluate(() => window.KF.G.sel) === 1, 'P2 follows the song P1 picks');
  await lead.page.bringToFront(); await lead.page.keyboard.press('Enter');
  await waitBeat(follow.page, 1, 5000).catch(() => {});
  check(await follow.page.evaluate(() => window.KF.G.scr) === 'play', 'START from P1 starts P2');
  await waitBeat(lead.page, 24);
  const cross = await Promise.all([lead, follow].map(x => x.page.evaluate(() => ({ mine: window.KF.play.score, rival: window.KF.Link.rival.score, clocks: window.KF.Link.rival.clocks }))));
  check(cross[1].rival > 0 && cross[0].rival > 0, 'each window sees the other\'s score: ' + JSON.stringify(cross));
  check(cross[1].clocks > 24 * 16, `P2 gets P1's clock: ${cross[1].clocks} pulses`);
  await screen(lead.page, '14-link-p1'); await screen(follow.page, '15-link-p2');
  await lead.page.keyboard.press('Escape');
  await sleep(400);
  check(await follow.page.evaluate(() => window.KF.G.scr) === 'songs', 'STOP from P1 brings P2 back to SONGS');

  /* 5. the panel's knobs and SEQ, as on the FM-1: everything the firmware does can be done on the page, with a mouse
     or a finger (a tap is a step, a drag turns, so does the wheel) */
  const k = await open(ctx), kp = k.page;
  await kp.evaluate(() => { window.KF.G.scr = 'songs'; window.KF.G.sel = 0; window.KF.setAuto(false); window.KF.setGrit(70); window.KF.setMaster(100); });
  await sleep(200);
  const pan = await kp.$('#panel'), pb = await pan.boundingBox(), ps = pb.width / 760, at = (x, y) => [pb.x + x * ps, pb.y + y * ps];
  const st = () => kp.evaluate(() => { const G = window.KF.G; return { sel: G.sel, tone: G.tone, speed: G.speed, mix: G.mix, grit: G.grit, master: G.master, auto: G.auto }; });
  const s0 = await st();
  await kp.mouse.click(...at(150, 42));                                    /* SELECT, its right half: a step on */
  await kp.mouse.click(...at(510, 42));                                    /* KNOB 2: TONE */
  await kp.mouse.move(...at(590, 42)); await kp.mouse.down(); await kp.mouse.move(...at(590, 72), { steps: 6 }); await kp.mouse.up();   /* KNOB 3 dragged down: SPEED */
  await kp.mouse.click(...at(670, 42));                                    /* KNOB 4, its left half: MIX down */
  await kp.mouse.move(...at(232, 42)); await kp.mouse.wheel(0, -100);      /* ALGORITHM, the wheel up: GRIT */
  await kp.mouse.move(...at(56, 42)); await kp.mouse.wheel(0, 100);        /* MASTER, the wheel down */
  await kp.mouse.click(...at(464, 114));                                   /* SEQ: AUTO */
  await sleep(150);
  const s1 = await st();
  check(s1.sel === s0.sel + 1 && s1.tone === (s0.tone + 1) % 6 && s1.speed === s0.speed - 20 && s1.mix === s0.mix - 10 && s1.grit === s0.grit + 10 &&
    s1.master === s0.master - 5 && s1.auto === !s0.auto, 'the panel: SELECT, KNOB 2-4, ALGORITHM, MASTER and SEQ turn and press: ' + JSON.stringify([s0, s1]));
  await kp.mouse.click(...at(410, 42));
  check((await st()).sel === s1.sel, 'KNOB 1 does nothing in the song list (a song\'s level is its own)');
  await screen(kp, '16-panel-songs'); await pan.screenshot({ path: path.join(OUT, '16b-panel.png') });
  await kp.evaluate(() => { window.KF.setAuto(false); window.KF.setMaster(100); });
  check(k.errors.length === 0, 'no page errors with the knobs' + (k.errors.length ? ': ' + k.errors.join(' | ') : ''));

  check(p1.errors.length === 0 && p2.errors.length === 0, 'no page errors in either window' + (p1.errors.concat(p2.errors).length ? ': ' + p1.errors.concat(p2.errors).join(' | ') : ''));

  await browser.close();
  console.log(failures ? `${failures} FAILED` : 'all passed');
  process.exitCode = failures ? 1 : 0;
})().catch(e => { console.error(e); process.exit(1); });
