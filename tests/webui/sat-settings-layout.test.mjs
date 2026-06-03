// Regression test for TASK-812: SAT settings rows must not overflow their
// panel when a <select> carries a long option label.
//
// Root cause (TASK-812): .sat-settings-group-body fell back to the legacy
// float-based .settingDiv layout (320px floated label + uncapped input/select).
// The legacy rule omitted `select`, so a wide <select> pushed the row beyond
// the group body's content box.
//
// Fix (components.css ~lines 1615-1644, TASK-812 comment block):
//   .sat-settings-group-body > .settingDiv          -- flex, float:none, min-width:0
//   .sat-settings-group-body > .settingDiv select   -- min-width:0; max-width:100%
//
// This test injects a faithful replica of the DOM buildSATSettingsGroups()
// emits (index.js lines 4250-4368) into the live page, then asserts:
//   1. The <select>'s right edge does not protrude past the group body.
//   2. The row's scrollWidth <= clientWidth (no internal overflow).
//   3. The group body's scrollWidth <= clientWidth (no container overflow).
//   4. The <select>'s computed max-width is "100%" (fix applied), not "none".
//   5. The <select>'s computed min-width is "0px" (fix applied).
//
// DOM structure mirrored from index.js buildSATSettingsGroups() (lines 4250-4368):
//
//   div.sat-settings-group
//     div.sat-settings-group-header
//       button.sat-settings-group-toggle
//         span.sat-settings-arrow  &#9660;
//         " Group title"
//       button.sat-settings-save-btn  "Save"
//     div.sat-settings-group-body          <-- selector anchor
//       div.settingDiv#SATD_<key>           <-- > .settingDiv
//         label.settings-field-container   <-- > .settings-field-container
//         div.settings-input-container     <-- > .settings-input-container
//           select.sat-setting-input       <-- the element that was overflowing
//
// The exact CSS selector exercised by the fix:
//   .sat-settings-group-body > .settingDiv select

import http from 'node:http';
import fs   from 'node:fs';
import path from 'node:path';
import { chromium } from 'playwright';

const DATA = process.env.OTGW_DATA_DIR;
const PORT = 8141;

if (!DATA) {
  console.error('FAIL: OTGW_DATA_DIR not set');
  process.exitCode = 1;
  process.exit(1);
}

const MIME = {
  '.html': 'text/html',
  '.js':   'text/javascript',
  '.css':  'text/css',
  '.json': 'application/json',
  '.svg':  'image/svg+xml',
  '.ico':  'image/x-icon',
  '.png':  'image/png',
};

const srv = http.createServer((q, s) => {
  let p = decodeURIComponent(new URL(q.url, `http://x:${PORT}`).pathname);
  if (p === '/') p = '/index.html';
  const f = path.join(DATA, p);
  if (fs.existsSync(f) && fs.statSync(f).isFile()) {
    s.writeHead(200, { 'content-type': MIME[path.extname(f)] || 'application/octet-stream' });
    fs.createReadStream(f).pipe(s);
  } else {
    s.writeHead(404); s.end('nf');
  }
});

await new Promise(r => srv.listen(PORT, '127.0.0.1', r));

const browser = await chromium.launch({ channel: 'chrome', headless: true });
const page    = await browser.newPage();

// Stub all API calls so the page does not hang waiting for the gateway.
await page.route('**/api/**', r => r.fulfill({ status: 200, contentType: 'application/json', body: '{}' }));

await page.goto(`http://127.0.0.1:${PORT}/index.html`, { waitUntil: 'domcontentloaded' });

let ok = true;
const log = (pass, msg) => {
  console.log((pass ? 'PASS' : 'FAIL') + ': ' + msg);
  if (!pass) ok = false;
};

// ---------------------------------------------------------------------------
// Inject a faithful SAT-settings group DOM that matches buildSATSettingsGroups()
// (index.js lines 4250-4368) and apply a realistic constrained width (360px)
// to the group body so the overflow-or-not decision is forced.
// The <select> carries a long option matching the "manual sensor address" case
// that was the concrete trigger in the TASK-812 field report.
// ---------------------------------------------------------------------------
const result = await page.evaluate(() => {
  // --- Build the group card (mirrors index.js 4252-4254) ---
  const grpDiv = document.createElement('div');
  grpDiv.className = 'sat-settings-group';
  grpDiv.style.cssText = 'width:360px; max-width:360px; overflow:hidden; position:fixed; left:-9999px; top:0;';

  // --- Header (mirrors index.js 4257-4288) ---
  const header = document.createElement('div');
  header.className = 'sat-settings-group-header';
  header.id = 'test-grp-header';

  const toggleBtn = document.createElement('button');
  toggleBtn.type = 'button';
  toggleBtn.className = 'sat-settings-group-toggle';
  toggleBtn.setAttribute('aria-expanded', 'true');
  toggleBtn.setAttribute('aria-controls', 'test-grp-body');

  const arrow = document.createElement('span');
  arrow.className = 'sat-settings-arrow';
  arrow.innerHTML = '&#9660;';
  toggleBtn.appendChild(arrow);
  toggleBtn.appendChild(document.createTextNode(' Room Sensor'));
  header.appendChild(toggleBtn);

  const saveBtn = document.createElement('button');
  saveBtn.type = 'button';
  saveBtn.className = 'sat-settings-save-btn';
  saveBtn.textContent = 'Save';
  header.appendChild(saveBtn);

  grpDiv.appendChild(header);

  // --- Body (mirrors index.js 4292-4363) ---
  const body = document.createElement('div');
  body.className = 'sat-settings-group-body';
  body.id = 'test-grp-body';

  // --- One row with a <select> that has a long option (TASK-812 trigger) ---
  // Mirrors index.js 4298-4362 for field.type === 'select'
  const rowDiv = document.createElement('div');
  rowDiv.className = 'settingDiv';
  rowDiv.id = 'SATD_testSensorSelect';

  const labelEl = document.createElement('label');
  labelEl.className = 'settings-field-container';
  labelEl.setAttribute('for', 'SAT_testSensorSelect');
  labelEl.textContent = 'Room sensor';
  rowDiv.appendChild(labelEl);

  const inputDiv = document.createElement('div');
  inputDiv.className = 'settings-input-container';

  const sel = document.createElement('select');
  sel.id = 'SAT_testSensorSelect';
  sel.className = 'sat-setting-input';

  // Short option (would not overflow on its own)
  const opt0 = document.createElement('option');
  opt0.value = 'auto';
  opt0.textContent = '(auto) best sensor';
  sel.appendChild(opt0);

  // Long option matching the field-report case (sensor MAC + long label)
  const optLong = document.createElement('option');
  optLong.value = 'manual';
  optLong.textContent = '(manual) 28:FF:AA:BB:CC:DD:EE:FF  very long sensor address option that would overflow';
  optLong.selected = true;
  sel.appendChild(optLong);

  inputDiv.appendChild(sel);
  rowDiv.appendChild(inputDiv);
  body.appendChild(rowDiv);
  grpDiv.appendChild(body);
  document.body.appendChild(grpDiv);

  // Force layout resolution.
  void grpDiv.getBoundingClientRect();

  // --- Measure ---
  const bodyRect = body.getBoundingClientRect();
  const selRect  = sel.getBoundingClientRect();

  // Content right edge of the body (excluding any border/padding on the right).
  // getBoundingClientRect() gives the border box; we need the content right
  // edge, which for overflow detection equals bodyRect.right minus the right
  // border (typically 0px here since border is only on the group, not body).
  // Using bodyRect.right is conservative and correct for overflow detection
  // because the <select> must not protrude beyond the group body's box at all.
  const bodyRight    = bodyRect.right;
  const selRight     = selRect.right;

  const selStyle     = window.getComputedStyle(sel);
  const selMaxWidth  = selStyle.maxWidth;   // expect "100%" resolved, not "none"
  const selMinWidth  = selStyle.minWidth;   // expect "0px"

  // scrollWidth vs clientWidth on the row and the body
  const rowScroll   = rowDiv.scrollWidth;
  const rowClient   = rowDiv.clientWidth;
  const bodyScroll  = body.scrollWidth;
  const bodyClient  = body.clientWidth;

  // Clean up so we don't pollute subsequent tests if any.
  document.body.removeChild(grpDiv);

  return {
    bodyRight,
    selRight,
    selMaxWidth,
    selMinWidth,
    rowScroll,
    rowClient,
    bodyScroll,
    bodyClient,
  };
});

// ---------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------

// 1. The <select>'s right edge must not protrude beyond the group body.
//    +1px tolerance for sub-pixel rounding.
log(
  result.selRight <= result.bodyRight + 1,
  `<select> right edge (${result.selRight.toFixed(1)}) <= group body right edge (${result.bodyRight.toFixed(1)}) [TASK-812 overflow]`
);

// 2. The row must not have internal horizontal overflow.
log(
  result.rowScroll <= result.rowClient + 1,
  `row scrollWidth (${result.rowScroll}) <= clientWidth (${result.rowClient}) [row overflow-free]`
);

// 3. The group body must not have horizontal overflow caused by the row.
log(
  result.bodyScroll <= result.bodyClient + 1,
  `group body scrollWidth (${result.bodyScroll}) <= clientWidth (${result.bodyClient}) [body overflow-free]`
);

// 4. max-width on the <select> must be "100%" (fix applied).
//    When the fix is absent the legacy rule omits `select`, leaving max-width:"none".
//    The browser resolves a percentage max-width to a px value, so we check it
//    is NOT "none" rather than checking for the literal "100%".
log(
  result.selMaxWidth !== 'none',
  `<select> computed max-width is "${result.selMaxWidth}" (must not be "none" — fix ensures max-width:100% applies)`
);

// 5. min-width on the <select> must be "0px" (fix applied).
log(
  result.selMinWidth === '0px',
  `<select> computed min-width is "${result.selMinWidth}" (must be "0px" — fix ensures min-width:0 applies)`
);

// ---------------------------------------------------------------------------
await browser.close();
srv.close();
console.log(ok ? 'RESULT: ALL PASS' : 'RESULT: FAILURES');
process.exitCode = ok ? 0 : 1;
