// Renders vavehero-mockup.html to vavehero-mockup.png (2x) and the four screens at their native 240 x 240.
// node mockup/render.cjs   (needs Playwright with Chromium)
const { chromium } = require('playwright');
const path = require('path');
const fs = require('fs');

(async () => {
  const here = __dirname;
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1840, height: 1000 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  await page.goto('file://' + path.join(here, 'vavehero-mockup.html'));
  await page.waitForFunction(() => window.__ready === true, null, { timeout: 15000 }).catch(() => {});
  if (errors.length) { console.error(errors.join('\n')); process.exitCode = 1; }
  await page.screenshot({ path: path.join(here, 'vavehero-mockup.png'), fullPage: true });
  const shots = await page.evaluate(() => window.__screens ? window.__screens() : {});
  fs.mkdirSync(path.join(here, 'screens'), { recursive: true });
  const names = { big: 'play', boost: 'op-boost', songs: 'songs', results: 'results' };
  for (const [id, url] of Object.entries(shots)) {
    fs.writeFileSync(path.join(here, 'screens', names[id] + '.png'), Buffer.from(url.split(',')[1], 'base64'));
  }
  await browser.close();
  console.log('ok', Object.keys(shots).length, 'screens');
})();
