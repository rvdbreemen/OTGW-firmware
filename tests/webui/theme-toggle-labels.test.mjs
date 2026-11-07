// TASK-1692: honest theme toggle labels, and no no-op "Dark theme by default" setting.
// D1: the toggle keeps its action label ('☀ Light' while dark, '🌙 Dark' while light) but carries no
//     aria-pressed (WAI-ARIA APG Button pattern: a toggle button's label must not change with its
//     state); aria-label and title name the action: "Switch to light theme" / "Switch to dark theme".
// D2: "theme" in every user-facing string about it (no "dark mode").
// D3: the darktheme setting is not shown on the settings page (the v2 UI never reads it: the theme
//     comes from the toggle in localStorage, else the OS preference, v2.html pre-paint script); the
//     key stays in REST and in the settings file.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. Point OTGW_DATA_DIR at an
// older copy of data/ to see it red.
//
// Run: node --test tests/webui/theme-toggle-labels.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, CHROME, DATA } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const SRC = path.join(HERE, '..', '..', 'src', 'OTGW-firmware');
const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = { settings: {
  hostname: S('otgw', 32), darktheme: { value: true, type: 'b' }, mqttbroker: S('broker.local', 30),
} };
const api = (m, p) => (p.endsWith('/v2/settings') ? SETTINGS : undefined);
const skip = !CHROME && 'no Chrome';

const BUTTON = `(function(){ var b = document.getElementById('themeToggle'); return {
  text: b.textContent, pressed: b.hasAttribute('aria-pressed'), label: b.getAttribute('aria-label'),
  title: b.getAttribute('title'), theme: document.documentElement.getAttribute('data-theme') }; })()`;

test('toggle: action label, no aria-pressed, aria-label and title name the action (D1)', { skip }, async () => {
  const h = await startPage({ api, beforeScript: `try { localStorage.setItem('otgw-theme', 'light'); } catch (e) {}` });
  try {
    // .connsum is built by v2.js at start-up: from then on the toggle's click handler is bound.
    assert.ok(await h.until(`!!document.querySelector('.connsum') && document.documentElement.getAttribute('data-theme') === 'light'`),
      'v2.js started with the light theme');
    const light = await h.evalJs(BUTTON);
    assert.equal(light.text, '🌙 Dark');
    assert.equal(light.pressed, false, 'no aria-pressed while light');
    assert.equal(light.label, 'Switch to dark theme');
    assert.equal(light.title, 'Switch to dark theme');
    await h.evalJs(`document.getElementById('themeToggle').click()`);
    assert.ok(await h.until(`document.documentElement.getAttribute('data-theme') === 'dark'`), 'the click switched to dark');
    const dark = await h.evalJs(BUTTON);
    assert.equal(dark.text, '☀ Light');
    assert.equal(dark.pressed, false, 'no aria-pressed while dark');
    assert.equal(dark.label, 'Switch to light theme');
    assert.equal(dark.title, 'Switch to light theme');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('settings page does not show the no-op darktheme setting (D3)', { skip }, async () => {
  const h = await startPage({ api });
  try {
    await h.until(`!!document.querySelector('button.seg[data-page="settings"]')`);
    await h.evalJs(`document.querySelector('button.seg[data-page="settings"]').click()`);
    // The rail shows one category at a time: open every category and collect the rows it renders.
    assert.ok(await h.until(`document.querySelectorAll('.rail-item[data-cat]').length > 1`), 'the settings rail rendered');
    const cats = await h.evalJs(`Array.prototype.map.call(document.querySelectorAll('.rail-item[data-cat]'), function (r) { return r.dataset.cat; })`);
    const keys = new Set();
    let sawText = false;
    for (const c of cats) {
      await h.evalJs(`document.querySelector('.rail-item[data-cat="${c}"]').click()`);
      await h.sleep(150);
      for (const k of await h.evalJs(`Array.prototype.map.call(document.querySelectorAll('[data-key]'), function (r) { return r.dataset.key; })`)) keys.add(k);
      sawText = sawText || await h.evalJs(`/Dark theme by default/i.test(document.body.textContent)`);
    }
    assert.ok(keys.has('hostname') && keys.has('mqttbroker'), 'the other settings render: ' + [...keys].join(','));
    assert.equal(keys.has('darktheme'), false, 'the settings page renders a darktheme row');
    assert.equal(sawText, false, 'the text "Dark theme by default" is on the page');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('"theme", not "dark mode", in the user-facing strings of v2.html and v2.js (D2)', () => {
  for (const f of ['v2.html', 'v2.js']) {
    const text = fs.readFileSync(path.join(DATA, f), 'utf8');
    assert.equal(/dark mode|light mode/i.test(text), false, `${f} still says "dark/light mode"`);
  }
});

test('darktheme stays in REST and in the settings file (D3)', () => {
  const rest = fs.readFileSync(path.join(SRC, 'restAPI.ino'), 'utf8');
  const store = fs.readFileSync(path.join(SRC, 'settingStuff.ino'), 'utf8');
  assert.match(rest, /F\("darktheme"\)/, 'GET /api/v2/settings still lists darktheme');
  assert.match(store, /F\("darktheme"\), settings\.bDarkTheme/, 'the settings file still carries darktheme');
});
