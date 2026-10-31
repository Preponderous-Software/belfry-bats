// Smoke test for the browser build in headless Chromium (Playwright).
//
//   node web/smoke/smoke.cjs <url> <out-dir> [--phone]
//
// Checks: the page starts with no page errors and shows the title card; the
// canvas draws the village (not one flat colour); Fly starts night 1 with
// the flock in the air; the flock follows the pointer like a player leads it
// (the mouse on a desktop, a held finger on the iPhone 13 profile: real touch
// events); near the church (the belfry.warp page hook), the built-in pilot
// takes the flock into the belfry, the night ends with bats home and the
// card says so; Next night starts night 2; the sound button turns the sound
// off and a reload keeps it off. On the phone no two controls overlap and all
// are on screen. Writes PNGs to <out-dir>.
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { chromium, devices } = require('playwright');

const [url, outDir] = process.argv.slice(2);
const phone = process.argv.includes('--phone');
if (!url || !outDir) {
  console.error('usage: smoke.cjs <url> <out-dir> [--phone]');
  process.exit(2);
}
fs.mkdirSync(outDir, { recursive: true });

// Minimal PNG decoder (8-bit RGB/RGBA, non-interlaced: what Chromium writes).
function decodePng(buf) {
  let pos = 8, width = 0, height = 0, channels = 0;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      channels = { 2: 3, 6: 4 }[data[9]];
      if (data[8] !== 8 || !channels || data[12] !== 0) throw new Error('unsupported PNG');
    } else if (type === 'IDAT') idat.push(data);
    pos += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const out = Buffer.alloc(height * stride);
  for (let y = 0; y < height; y++) {
    const f = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const v = raw[y * (stride + 1) + 1 + x];
      const a = x >= channels ? out[y * stride + x - channels] : 0;
      const b = y > 0 ? out[(y - 1) * stride + x] : 0;
      const c = x >= channels && y > 0 ? out[(y - 1) * stride + x - channels] : 0;
      let p;
      if (f === 0) p = 0;
      else if (f === 1) p = a;
      else if (f === 2) p = b;
      else if (f === 3) p = (a + b) >> 1;
      else {
        const pa = Math.abs(b - c), pb = Math.abs(a - c), pc = Math.abs(a + b - 2 * c);
        p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      out[y * stride + x] = (v + p) & 255;
    }
  }
  return { width, height, channels, data: out };
}

function colourCount(png) {
  const img = decodePng(png);
  const seen = new Set();
  for (let i = 0; i < img.data.length; i += img.channels)
    seen.add(((img.data[i] >> 4) << 8) | ((img.data[i + 1] >> 4) << 4) | (img.data[i + 2] >> 4));
  return seen.size;
}

(async () => {
  const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const base = phone ? devices['iPhone 13'] : { viewport: { width: 1280, height: 800 } };
  // "headless" in the UA keeps arcade from counting a smoke run as a play.
  const context = await browser.newContext({ ...base, userAgent: (base.userAgent || 'Mozilla/5.0') + ' belfry-smoke headless' });
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const fail = async (why) => {
    console.error('FAIL: ' + why);
    await page.screenshot({ path: path.join(outDir, 'failure.png'), timeout: 60000 }).catch(() => {});
    await browser.close();
    process.exit(1);
  };
  const shot = (name) => page.screenshot({ path: path.join(outDir, name), timeout: 180000 });
  const open = async (reload) => {
    if (reload) {
      await page.reload({ waitUntil: 'commit', timeout: 120000 }).catch(async () => {
        await page.goto(url, { waitUntil: 'commit', timeout: 120000 });
      });
    } else await page.goto(url, { waitUntil: 'commit' });
    try {
      await page.waitForFunction(() => window.belfry && window.belfryStatus, null, { timeout: 90000 });
    } catch (e) {
      await fail('the game never became ready' + (reload ? ' after a reload' : '') + ': ' + errors.join(' | '));
    }
  };
  const status = async () => JSON.parse(await page.evaluate(() => window.belfry.status()));
  const until = async (pred, ms = 30000) => {
    const end = Date.now() + ms;
    let st = await status();
    while (!pred(st) && Date.now() < end) {
      await page.waitForTimeout(250);
      st = await status();
    }
    return st;
  };
  const checkLayout = async (when) => {
    if (!phone) return;
    const boxes = await page.evaluate(() => {
      const out = [];
      document.querySelectorAll('#top button, #top .stat, #top .brand, .card button').forEach((b) => {
        const r = b.getBoundingClientRect();
        if (r.width > 0 && r.height > 0 && b.offsetParent && getComputedStyle(b).display !== 'none')
          out.push({ id: b.id || b.textContent.trim(), x: r.left, y: r.top, w: r.width, h: r.height });
      });
      return { out, vw: innerWidth, vh: innerHeight };
    });
    for (const a of boxes.out) {
      if (a.x < 0 || a.y < 0 || a.x + a.w > boxes.vw + 0.5 || a.y + a.h > boxes.vh + 0.5) await fail(`${when}: ${a.id} is off screen`);
      for (const b of boxes.out) {
        if (a === b) continue;
        if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) await fail(`${when}: ${a.id} overlaps ${b.id}`);
      }
    }
    console.log(`ok: ${boxes.out.length} controls on screen, none overlapping (${when})`);
  };

  await open(false);
  if (!(await page.locator('#title').waitFor({ state: 'visible', timeout: 10000 }).then(() => true, () => false)))
    await fail('the title card was not shown');
  await page.waitForTimeout(1500);
  const title = await shot('belfry-title.png');
  const colours = colourCount(title);
  console.log(`title: ${colours} distinct colours`);
  if (colours < 12) await fail('the canvas looks blank (' + colours + ' colours)');
  await checkLayout('title');

  if (phone) await page.locator('#startBtn').tap();
  else await page.locator('#startBtn').click();
  let s = await until((st) => st.phase === 'flying');
  console.log(`flying: night ${s.night}, ${s.flock} in the flock, ${s.strays} strays, ${s.owls} owls`);
  if (s.phase !== 'flying' || s.night !== 1 || s.flock !== 8) await fail('Fly did not start night 1 with eight bats: ' + JSON.stringify(s));
  await checkLayout('flying');
  // The camera's aspect is the canvas's (a stretched picture otherwise).
  const view = JSON.parse(await page.evaluate(() => window.belfry.guide()));
  const css = await page.evaluate(() => { const c = document.getElementById('canvas'); return { w: c.clientWidth, h: c.clientHeight }; });
  console.log(`camera ${view.w}x${view.h}, canvas ${css.w}x${css.h} CSS px`);
  if (Math.abs(view.w / view.h - css.w / css.h) > 0.03) await fail('the camera\'s aspect does not match the canvas');

  // Lead the flock to the right, as a player would: up and ahead of it.
  const box = await page.locator('#canvas').boundingBox();
  const lead = { x: box.x + box.width * 0.82, y: box.y + box.height * 0.42 };
  const startX = s.cx;
  let cdp = null;
  if (phone) {
    cdp = await context.newCDPSession(page);
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: lead.x, y: lead.y }] });
  } else {
    await page.mouse.move(lead.x - 20, lead.y);
    await page.mouse.move(lead.x, lead.y, { steps: 4 });
  }
  s = await until((st) => st.cx > startX + 8, 60000);
  console.log(`led: the flock's centre went from ${startX} to ${s.cx}`);
  if (!(s.cx > startX + 8)) await fail(`the flock did not follow the ${phone ? 'finger' : 'mouse'} (${startX} -> ${s.cx})`);
  await shot('belfry-flying.png');
  if (phone) {
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    // Lifted: the flock circles where the finger was and stops going on.
    await page.waitForTimeout(4000);
    const a = await status();
    await page.waitForTimeout(3000);
    const b = await status();
    console.log(`finger lifted: centre ${a.cx} then ${b.cx}`);
    if (Math.abs(b.cx - a.cx) > 4) await fail('with the finger lifted the flock kept going');
  } else {
    await page.mouse.move(box.x + box.width * 0.5, box.y + box.height * 0.5);
  }

  // Near the church, the pilot takes the flock home.
  await page.evaluate(() => window.belfry.warp(window.belfryStatus.towerX - 16));
  await page.evaluate(() => window.belfry.autopilot(1));
  await page.waitForTimeout(2500);
  await shot('belfry-tower.png');  // the flock on its way into the belfry (the README's picture)
  s = await until((st) => st.phase !== 'flying', 150000);
  console.log(`night 1 over: ${s.home} of ${s.of} home, ending ${s.ending}, score ${s.score}`);
  if (s.phase !== 'over' || s.home < 1 || s.ending !== 'home') await fail('the pilot did not bring the flock into the belfry: ' + JSON.stringify(s));
  await page.evaluate(() => window.belfry.autopilot(0));
  // The page relabels itself from the status every 150 ms.
  if (!(await page.locator('#over').waitFor({ state: 'visible', timeout: 10000 }).then(() => true, () => false)))
    await fail('the end-of-night card was not shown');
  const words = await page.locator('#overCount').innerText();
  if (!new RegExp('^' + s.home + ' of ' + s.of + ' home$').test(words)) await fail('the card says ' + words);
  await page.waitForTimeout(800);
  await shot('belfry-home.png');
  await checkLayout('night over');
  if (phone) await page.locator('#nextBtn').tap();
  else await page.keyboard.press('Space');
  s = await until((st) => st.phase === 'flying' && st.night === 2);
  if (s.night !== 2 || s.phase !== 'flying') await fail('Next night did not start night 2: ' + JSON.stringify(s));
  console.log(`night 2: ${s.flock} in the flock, ${s.owls} owls, score carried ${s.score}`);

  // Keys on a desktop: the right arrow leads the flock on.
  if (!phone) {
    const k0 = s.cx;
    await page.keyboard.down('ArrowRight');
    s = await until((st) => st.cx > k0 + 6, 60000);
    await page.keyboard.up('ArrowRight');
    console.log(`arrow key: centre ${k0} -> ${s.cx}`);
    if (!(s.cx > k0 + 6)) await fail('the right arrow did not lead the flock');
  }

  // Sound off, and it stays off.
  if (phone) await page.locator('#soundBtn').tap();
  else await page.locator('#soundBtn').click();
  s = await until((st) => st.muted, 5000);
  if (!s.muted) await fail('the sound button did not turn the sound off');
  await open(true);
  s = await status();
  if (!s.muted) await fail('a reload turned the sound back on');
  if (s.phase !== 'title') await fail('a reload should start at the title');

  if (errors.length) await fail('page errors: ' + errors.join(' | '));
  console.log('PASS: ' + (phone ? 'phone' : 'desktop'));
  await browser.close();
})().catch((e) => {
  console.error('FAIL: ' + (e && e.stack || e));
  process.exit(1);
});
