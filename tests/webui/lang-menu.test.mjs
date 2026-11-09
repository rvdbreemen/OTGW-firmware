// TASK-1764: the header language dropdown (Dutch, English, German, Greek) of the v2 web UI.
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. The fake device serves
// the packs from the repo-root i18n/ folder (the build stages them into LittleFS as i18n/<lang>.json).
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/lang-menu.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { startPage, CHROME, DATA } from './lib/page-harness.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const skip = !CHROME && 'no Chrome';


const ENGLISH_NAV = 'Home|SAT|Monitor|Settings|Advanced';

function flatten(obj, prefix, out) {
  for (const [k, v] of Object.entries(obj)) {
    const key = prefix ? prefix + '.' + k : k;
    if (v && typeof v === 'object') flatten(v, key, out); else out[key] = v;
  }
  return out;
}

// The English key set and the Greek nav labels come from the shipped files, so the test follows edits.
const EN_SRC = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
const EN_OBJ = JSON.parse(/var I18N_EN = (\{[\s\S]*?\r?\n  \});/.exec(EN_SRC)[1]);
const EN_KEYS = Object.keys(flatten(EN_OBJ, '', {}));
const EL_NAV = JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', 'el.json'), 'utf8')).nav;
const GREEK_NAV = [EL_NAV.home, EL_NAV.sat, EL_NAV.monitor, EL_NAV.settings, EL_NAV.advanced].join('|');

// The packs live outside data/: serve i18n/<lang>.json like the device does.
const asset = (p, q, s) => {
  const m = /^\/i18n\/([a-z]+)\.json$/.exec(p);
  if (!m) return false;
  const f = path.join(ROOT, 'i18n', m[1] + '.json');
  if (!fs.existsSync(f)) { s.writeHead(404); s.end('nf'); return true; }
  s.writeHead(200, { 'content-type': 'application/json' });
  s.end(fs.readFileSync(f));
  return true;
};

test('el is a supported language and i18n/el.json has exactly the English key set', () => {
  const src = fs.readFileSync(path.join(DATA, 'v2.js'), 'utf8');
  const langs = /var I18N_LANGS = \[([^\]]*)\]/.exec(src)[1].replace(/['"\s]/g, '').split(',');
  assert.deepEqual(langs, ['en', 'nl', 'de', 'el']);
  const pack = flatten(JSON.parse(fs.readFileSync(path.join(ROOT, 'i18n', 'el.json'), 'utf8')), '', {});
  assert.deepEqual(Object.keys(pack).sort(), EN_KEYS.slice().sort());
  assert.equal(pack['nav.settings'], 'Ρυθμίσεις');
});

const MENU = `(function(){ var d = document.querySelector('.headerrow details.lang-menu');
  if (!d) return null;
  var sum = d.querySelector('summary');
  return { open: d.hasAttribute('open'),
    code: sum.querySelector('.code').textContent, label: sum.getAttribute('aria-label'),
    flag: getComputedStyle(sum.querySelector('.flag')).backgroundImage,
    entries: [].map.call(d.querySelectorAll('ul li a'), function (a) { var f = a.querySelector('.flag');
      return { lang: a.getAttribute('lang'), name: a.querySelector('.nm').textContent,
        code: a.querySelector('.code').textContent, active: a.classList.contains('active'),
        current: a.getAttribute('aria-current'), w: f.offsetWidth, h: f.offsetHeight,
        bg: getComputedStyle(f).backgroundImage }; }) }; })()`;
const NAV = `[].map.call(document.querySelectorAll('.switcher .seg b[data-i18n^=\"nav.\"]'), function (b) { return b.textContent; }).join('|')`;

test('header has the language dropdown with four entries and flag files in data/flags', { skip }, async () => {
  const h = await startPage({ asset, beforeScript: `try { localStorage.removeItem('otgw.lang'); } catch (e) {}` });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum')`), 'v2.js started');
    const m = await h.evalJs(MENU);
    assert.ok(m, 'details.lang-menu in the header');
    assert.deepEqual(m.entries.map(e => e.lang), ['en', 'nl', 'de', 'el']);
    assert.deepEqual(m.entries.map(e => e.name), ['English', 'Nederlands', 'Deutsch', 'Ελληνικά']);
    assert.deepEqual(m.entries.map(e => e.code), ['EN', 'NL', 'DE', 'EL']);
    for (const e of m.entries) {
      assert.equal(e.w, 21); assert.equal(e.h, 14);
      const file = /flags\/([a-z]+\.svg)/.exec(e.bg);
      assert.ok(file, 'flag image for ' + e.lang + ': ' + e.bg);
      assert.ok(fs.existsSync(path.join(DATA, 'flags', file[1])), file[1] + ' exists under data/flags');
    }
    assert.deepEqual(m.entries.map(e => e.active), [true, false, false, false]);
    assert.equal(m.entries[0].current, 'true');
    assert.equal(m.code, 'EN');
    assert.equal(m.label, 'Language: English');
    assert.match(m.flag, /flags\/gb\.svg/);
    assert.equal(m.open, false);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('choosing Greek stores otgw.lang, switches the nav live without a reload, updates the summary', { skip }, async () => {
  const h = await startPage({ asset, beforeScript: `try { localStorage.removeItem('otgw.lang'); } catch (e) {}` });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum') && !!document.querySelector('details.lang-menu')`));
    assert.equal(await h.evalJs(NAV), ENGLISH_NAV);
    await h.evalJs(`window.__noReload = 42; document.querySelector('details.lang-menu summary').click();`);
    assert.equal(await h.evalJs(`document.querySelector('details.lang-menu').open`), true, 'summary opens it');
    await h.evalJs(`document.querySelector('details.lang-menu a[lang="el"]').click()`);
    assert.ok(await h.until(`${NAV} === '${GREEK_NAV}'`), 'nav text is Greek');
    assert.equal(await h.evalJs(`window.__noReload`), 42, 'no reload');
    assert.equal(await h.evalJs(`localStorage.getItem('otgw.lang')`), 'el');
    assert.equal(await h.evalJs(`document.documentElement.lang`), 'el');
    const m = await h.evalJs(MENU);
    assert.equal(m.open, false, 'selecting closes the menu');
    assert.equal(m.code, 'EL');
    assert.equal(m.label, 'Γλώσσα: Ελληνικά', 'the label word itself is translated');
    assert.match(m.flag, /flags\/gr\.svg/);
    assert.deepEqual(m.entries.map(e => e.active), [false, false, false, true]);
    assert.equal(m.entries[3].current, 'true');
    assert.equal(m.entries[0].current, null);
    // and back to English
    await h.evalJs(`document.querySelector('details.lang-menu a[lang="en"]').click()`);
    assert.ok(await h.until(`${NAV} === '${ENGLISH_NAV}'`), 'back to English');
    assert.equal(await h.evalJs(`localStorage.getItem('otgw.lang')`), 'en');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

// A pack that arrives late must not override a newer choice: pick Greek (served after 1.5 s), then English at once.
const slowEl = (p, q, s) => {
  if (p !== '/i18n/el.json') return asset(p, q, s);
  setTimeout(() => { s.writeHead(200, { 'content-type': 'application/json' }); s.end(fs.readFileSync(path.join(ROOT, 'i18n', 'el.json'))); }, 1500);
  return true;
};
test('a late Greek pack does not override a newer English choice', { skip }, async () => {
  const h = await startPage({ asset: slowEl, beforeScript: `try { localStorage.removeItem('otgw.lang'); } catch (e) {}` });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum') && !!document.querySelector('details.lang-menu')`));
    await h.evalJs(`document.querySelector('details.lang-menu a[lang="el"]').click(); document.querySelector('details.lang-menu a[lang="en"]').click();`);
    await new Promise(r => setTimeout(r, 2500));   // the Greek reply has arrived by now
    assert.equal(await h.evalJs(NAV), ENGLISH_NAV, 'the late Greek pack replaced the English choice');
    assert.equal(await h.evalJs(`document.documentElement.lang`), 'en');
    assert.equal((await h.evalJs(MENU)).code, 'EN');
    assert.equal(await h.evalJs(`localStorage.getItem('otgw.lang')`), 'en');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

// A pack that cannot be loaded (the device answers 503 twice) keeps the current language and is not stored.
const failEl = (p, q, s) => {
  if (p !== '/i18n/el.json') return asset(p, q, s);
  s.writeHead(503); s.end('busy'); return true;
};
test('a Greek pack that fails to load keeps English and is not stored', { skip }, async () => {
  const h = await startPage({ asset: failEl, beforeScript: `try { localStorage.removeItem('otgw.lang'); } catch (e) {}` });
  try {
    assert.ok(await h.until(`!!document.querySelector('.connsum') && !!document.querySelector('details.lang-menu')`));
    await h.evalJs(`document.querySelector('details.lang-menu a[lang="el"]').click()`);
    await new Promise(r => setTimeout(r, 4500));   // first try, 3 s pause, second try
    assert.equal(await h.evalJs(NAV), ENGLISH_NAV);
    assert.equal((await h.evalJs(MENU)).code, 'EN');
    assert.equal(await h.evalJs(`localStorage.getItem('otgw.lang')`), null, 'a language that did not load was stored');
  } finally { await h.close(); }
});

test('a stored Greek choice is applied at start; Escape and an outside pointerdown close the menu', { skip }, async () => {
  const h = await startPage({ asset, beforeScript: `try { localStorage.setItem('otgw.lang', 'el'); } catch (e) {}` });
  try {
    assert.ok(await h.until(`${NAV} === '${GREEK_NAV}'`), 'stored el applied');
    assert.equal((await h.evalJs(MENU)).code, 'EL');
    await h.evalJs(`document.querySelector('details.lang-menu summary').click()`);
    assert.equal((await h.evalJs(MENU)).open, true);
    await h.evalJs(`document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))`);
    const m = await h.evalJs(MENU);
    assert.equal(m.open, false, 'Escape closes');
    assert.equal(await h.evalJs(`document.activeElement === document.querySelector('details.lang-menu summary')`), true, 'focus back on summary');
    await h.evalJs(`document.querySelector('details.lang-menu summary').click()`);
    assert.equal((await h.evalJs(MENU)).open, true);
    await h.evalJs(`document.body.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }))`);
    assert.equal((await h.evalJs(MENU)).open, false, 'a pointerdown outside closes');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('at phone width the summary hides the language code', { skip }, async () => {
  const h = await startPage({ asset, width: 375, height: 800 });
  try {
    assert.ok(await h.until(`!!document.querySelector('details.lang-menu')`));
    const r = await h.evalJs(`({ w: window.innerWidth, d: getComputedStyle(document.querySelector('details.lang-menu summary .code')).display })`);
    assert.ok(r.w <= 420, 'headless Chrome gave innerWidth ' + r.w + ', cannot check the <=420px rule');
    assert.equal(r.d, 'none');
  } finally { await h.close(); }
});
