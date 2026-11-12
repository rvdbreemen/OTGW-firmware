// TASK-1692: honest theme toggle labels, and no no-op "Dark theme by default" setting.
// D1 (TASK-1830, Robert 2026-11-12, replaces the single action button of TASK-1692): a Light/Dark
//     segmented switch as on the OTGW site: role=group with two buttons that keep their labels
//     ('☀ Light', '☾ Dark'); aria-pressed marks the theme that is on (WAI-ARIA button group); the title
//     names the action ("Switch to light theme" / "Switch to dark theme").
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

const SWITCH = `(function(){ var g = document.getElementById('themeSwitch'); if (!g) return null;
  var bs = g.querySelectorAll('button[data-theme-set]');
  return { role: g.getAttribute('role'), label: g.getAttribute('aria-label'), theme: document.documentElement.getAttribute('data-theme'),
    stored: localStorage.getItem('otgw-theme'),
    buttons: Array.prototype.map.call(bs, function (b) { return [b.getAttribute('data-theme-set'), b.textContent, b.getAttribute('aria-pressed'), b.getAttribute('title')]; }) }; })()`;

test('Light/Dark segmented switch: fixed labels, aria-pressed on the active theme, click stores it (D1)', { skip }, async () => {
  const h = await startPage({ api, beforeScript: `try { if (!sessionStorage.getItem('seeded')) { localStorage.setItem('otgw-theme', 'light'); sessionStorage.setItem('seeded', '1'); } } catch (e) {}` });
  try {
    // .connsum is built by v2.js at start-up: from then on the click handlers are bound.
    assert.ok(await h.until(`!!document.querySelector('.connsum') && document.documentElement.getAttribute('data-theme') === 'light'`),
      'v2.js started with the light theme');
    const light = await h.evalJs(SWITCH);
    assert.ok(light, '#themeSwitch exists');
    assert.equal(light.role, 'group'); assert.equal(light.label, 'Theme');
    assert.deepEqual(light.buttons, [['light', '☀Light', 'true', 'Switch to light theme'], ['dark', '☾Dark', 'false', 'Switch to dark theme']]);
    await h.evalJs(`document.querySelector('#themeSwitch [data-theme-set="dark"]').click()`);
    assert.ok(await h.until(`document.documentElement.getAttribute('data-theme') === 'dark'`), 'the click switched to dark');
    const dark = await h.evalJs(SWITCH);
    assert.deepEqual(dark.buttons.map((b) => b[2]), ['false', 'true'], 'aria-pressed follows the theme');
    assert.deepEqual(dark.buttons.map((b) => b[1]), ['☀Light', '☾Dark'], 'labels do not change with the state');
    assert.equal(dark.stored, 'dark');
    await h.evalJs(`document.querySelector('#themeSwitch [data-theme-set="dark"]').click()`);   // clicking the active one keeps it
    assert.equal((await h.evalJs(SWITCH)).theme, 'dark');
    await h.send('Page.reload');
    assert.ok(await h.until(`!!document.querySelector('.connsum') && document.documentElement.getAttribute('data-theme') === 'dark'`), 'dark survives a reload');
    assert.deepEqual((await h.evalJs(SWITCH)).buttons.map((b) => b[2]), ['false', 'true']);
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
