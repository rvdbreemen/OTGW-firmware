// Headless validation of the TASK-808 tab-restore feature.
// Verifies that index.html restores the active page section from the URL hash
// on load, and that setActivePageSection() persists the active page into the hash.
//
// DOM "active section" signal: setActivePageSection() adds/removes the 'active'
// CSS class on each top-level section div (index.js line 313:
//   if (id === activeId) section.classList.add('active');
//   else section.classList.remove('active');
// ).
//
// Hash map (index.js lines 300-307, PAGE_SECTION_HASH):
//   displayMainPage       -> '' (no hash, clean URL)
//   displaySATPage        -> '#sat'
//   displaySettingsPage   -> '#settings'
//   displayDeviceInfo     -> '#deviceinfo'
//   displaySATSettingsPage -> '#satsettings'
//   displayPICflash       -> '#tabPICflash'
//
// startMainPage() (index.js line 3346) is a nested function inside initMainPage()
// and is called after fetchDallasLabels() resolves/rejects. It reads
// window.location.hash and dispatches (lines 3369-3373):
//   '#sat'        -> satPage()
//   '#settings'   -> settingsPage()
//   '#deviceinfo' -> deviceinfoPage()
//   '#satsettings'-> satSettingsPage()
//   else          -> showMainPage()  (falls through to Home)
//
// setActivePageSection IS a top-level global function (index.js line 309).

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { chromium } from 'playwright';

const DATA = process.env.OTGW_DATA_DIR;
const PORT = 8140;
const SETTLE_MS = 600; // allow the Dallas labels fetch chain + startMainPage() to run

const MIME = {
  '.html': 'text/html',
  '.js':   'text/javascript',
  '.css':  'text/css',
  '.json': 'application/json',
  '.svg':  'image/svg+xml',
  '.ico':  'image/x-icon',
  '.png':  'image/png',
};

if (!DATA) {
  console.error('FATAL: OTGW_DATA_DIR env var is not set');
  process.exit(2);
}

// Minimal static file server (same pattern as sat-log-filter.test.mjs).
const srv = http.createServer((q, s) => {
  let p = decodeURIComponent(new URL(q.url, `http://x:${PORT}`).pathname);
  if (p === '/') p = '/index.html';
  const f = path.join(DATA, p);
  if (fs.existsSync(f) && fs.statSync(f).isFile()) {
    s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(s);
  } else {
    s.writeHead(404);
    s.end('nf');
  }
});
await new Promise(r => srv.listen(PORT, '127.0.0.1', r));

const browser = await chromium.launch({ channel: 'chrome', headless: true });
let ok = true;
const log = (pass, msg) => { console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg); if (!pass) ok = false; };

// Helper: returns the id of the section that currently carries class 'active'.
// Checks the five user-navigable top-level page sections.
async function activeSection(page) {
  return page.evaluate(() => {
    const ids = [
      'displayMainPage',
      'displaySettingsPage',
      'displayDeviceInfo',
      'displaySATPage',
      'displaySATSettingsPage',
    ];
    for (const id of ids) {
      const el = document.getElementById(id);
      if (el && el.classList.contains('active')) return id;
    }
    return null;
  });
}

// -----------------------------------------------------------------------
// Test 1: navigate to index.html#settings -> settingsPage() must activate
//         displaySettingsPage, not displayMainPage.
//         (index.js line 3370: else if (h === "#settings") { settingsPage(); })
//         settingsPage() calls setActivePageSection('displaySettingsPage') (line 3490).
// -----------------------------------------------------------------------
{
  const pg = await browser.newPage();
  // Mock all /api/** calls to return {} so fetch chains don't reject.
  await pg.route('**/api/**', r => r.fulfill({ status: 200, contentType: 'application/json', body: '{}' }));
  await pg.goto(`http://127.0.0.1:${PORT}/index.html#settings`, { waitUntil: 'domcontentloaded' });
  // Wait for the async Dallas-labels fetch chain to call startMainPage().
  await pg.waitForTimeout(SETTLE_MS);

  const active = await activeSection(pg);
  log(active === 'displaySettingsPage',
    `#settings hash restores Settings section (active='${active}', expected='displaySettingsPage')`);
  log(active !== 'displayMainPage',
    `#settings hash does NOT leave Home active (active='${active}')`);
  await pg.close();
}

// -----------------------------------------------------------------------
// Test 2: navigate to index.html (no hash) -> showMainPage() falls through,
//         activates displayMainPage.
//         (index.js line 3373: else { showMainPage(); })
//         showMainPage() calls setActivePageSection('displayMainPage') (line 3449).
// -----------------------------------------------------------------------
{
  const pg = await browser.newPage();
  await pg.route('**/api/**', r => r.fulfill({ status: 200, contentType: 'application/json', body: '{}' }));
  await pg.goto(`http://127.0.0.1:${PORT}/index.html`, { waitUntil: 'domcontentloaded' });
  await pg.waitForTimeout(SETTLE_MS);

  const active = await activeSection(pg);
  log(active === 'displayMainPage',
    `no hash defaults to Home (active='${active}', expected='displayMainPage')`);
  await pg.close();
}

// -----------------------------------------------------------------------
// Test 3: round-trip via setActivePageSection() — the persistence half.
// setActivePageSection('displaySettingsPage') must write '#settings' into
// window.location.hash via history.replaceState (index.js lines 326-330).
// setActivePageSection IS a top-level global (index.js line 309).
// -----------------------------------------------------------------------
{
  const pg = await browser.newPage();
  await pg.route('**/api/**', r => r.fulfill({ status: 200, contentType: 'application/json', body: '{}' }));
  // Load on Home (no hash) so the page is fully initialised.
  await pg.goto(`http://127.0.0.1:${PORT}/index.html`, { waitUntil: 'domcontentloaded' });
  await pg.waitForTimeout(SETTLE_MS);

  const result = await pg.evaluate(() => {
    if (typeof setActivePageSection !== 'function') return { skipped: true, reason: 'setActivePageSection not global' };
    setActivePageSection('displaySettingsPage');
    return { skipped: false, hash: window.location.hash };
  });

  if (result.skipped) {
    console.log('SKIP: round-trip persistence test — ' + result.reason);
  } else {
    log(result.hash === '#settings',
      `setActivePageSection('displaySettingsPage') sets hash to '#settings' (got '${result.hash}')`);
  }
  await pg.close();
}

// -----------------------------------------------------------------------
// Test 4: extra hash — navigate to index.html#sat -> satPage() activates
//         displaySATPage.
//         (index.js line 3369: if (h === "#sat") { satPage(); })
//         satPage() calls setActivePageSection('displaySATPage') (line 3516).
// -----------------------------------------------------------------------
{
  const pg = await browser.newPage();
  await pg.route('**/api/**', r => r.fulfill({ status: 200, contentType: 'application/json', body: '{}' }));
  await pg.goto(`http://127.0.0.1:${PORT}/index.html#sat`, { waitUntil: 'domcontentloaded' });
  await pg.waitForTimeout(SETTLE_MS);

  const active = await activeSection(pg);
  log(active === 'displaySATPage',
    `#sat hash restores SAT section (active='${active}', expected='displaySATPage')`);
  await pg.close();
}

// -----------------------------------------------------------------------
// Test 5: unknown hash falls through to Home (no TASK-808 match).
//         (index.js line 3373: else { showMainPage(); })
// -----------------------------------------------------------------------
{
  const pg = await browser.newPage();
  await pg.route('**/api/**', r => r.fulfill({ status: 200, contentType: 'application/json', body: '{}' }));
  await pg.goto(`http://127.0.0.1:${PORT}/index.html#unknownhash`, { waitUntil: 'domcontentloaded' });
  await pg.waitForTimeout(SETTLE_MS);

  const active = await activeSection(pg);
  log(active === 'displayMainPage',
    `unknown hash falls through to Home (active='${active}', expected='displayMainPage')`);
  await pg.close();
}

await browser.close();
srv.close();

console.log(ok ? 'RESULT: ALL PASS' : 'RESULT: FAILURES');
process.exitCode = ok ? 0 : 1;
