// Phone layouts: on several phones, portrait and landscape, the title card
// and the top bar fit on screen with nothing overlapping, and a tap on Fly
// starts the night.
//
//   node web/smoke/layout.cjs <url> <out-dir>
const fs = require('fs');
const path = require('path');
const { chromium, devices } = require('playwright');

const [url, outDir] = process.argv.slice(2);
if (!url || !outDir) {
  console.error('usage: layout.cjs <url> <out-dir>');
  process.exit(2);
}
fs.mkdirSync(outDir, { recursive: true });

const profiles = [
  ['iphone-se', devices['iPhone SE']],
  ['pixel-7', devices['Pixel 7']],
  ['galaxy-s5', devices['Galaxy S5']],
  ['iphone-13-landscape', devices['iPhone 13 landscape']],
];

(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  let failed = false;
  for (const [name, dev] of profiles) {
    const context = await browser.newContext({ ...dev, userAgent: dev.userAgent + ' belfry-smoke headless' });
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (e) => errors.push(String(e)));
    const problems = [];
    const check = async (when, sel) => {
      const r = await page.evaluate((sel) => {
        const out = [];
        document.querySelectorAll(sel).forEach((b) => {
          const r = b.getBoundingClientRect();
          if (r.width > 0 && r.height > 0 && b.offsetParent && getComputedStyle(b).display !== 'none')
            out.push({ id: b.id || b.textContent.trim().slice(0, 20), x: r.left, y: r.top, w: r.width, h: r.height });
        });
        return { out, vw: innerWidth, vh: innerHeight };
      }, sel);
      for (const a of r.out) {
        if (a.x < -0.5 || a.y < -0.5 || a.x + a.w > r.vw + 0.5 || a.y + a.h > r.vh + 0.5) problems.push(`${when}: ${a.id} off screen`);
        for (const b of r.out)
          if (a !== b && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) problems.push(`${when}: ${a.id} overlaps ${b.id}`);
      }
      return r.out.length;
    };
    try {
      await page.goto(url, { waitUntil: 'commit' });
      await page.waitForFunction(() => window.belfry && window.belfryStatus, null, { timeout: 90000 });
      const n1 = await check('title', '#top button, #top .stat, #top .brand, #title, #startBtn');
      // The title card is one box with its button inside it: only the card against the bar.
      await page.screenshot({ path: path.join(outDir, `${name}-title.png`), timeout: 180000 });
      await page.locator('#startBtn').tap();
      await page.waitForFunction(() => window.belfryStatus && window.belfryStatus.phase === 'flying', null, { timeout: 30000 });
      const n2 = await check('flying', '#top button, #top .stat, #top .brand');
      await page.waitForTimeout(800);
      await page.screenshot({ path: path.join(outDir, `${name}-flying.png`), timeout: 180000 });
      if (errors.length) problems.push('page errors: ' + errors.join(' | '));
      console.log(`${name}: ${n1} boxes at the title, ${n2} in flight${problems.length ? '' : ', ok'}`);
    } catch (e) {
      problems.push(String(e.message || e).split('\n')[0]);
    }
    // The card contains its own button: drop those pairs.
    const real = problems.filter((p) => !/title: (startBtn overlaps title|title overlaps startBtn)/.test(p));
    for (const p of real) console.error(`FAIL ${name}: ${p}`);
    failed = failed || real.length > 0;
    await context.close();
  }
  await browser.close();
  process.exit(failed ? 1 : 0);
})();
