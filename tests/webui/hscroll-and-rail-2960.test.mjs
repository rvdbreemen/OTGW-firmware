// TASK-2960 (UX review F8 F9, spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T5-mobile-strips-and-rail.md):
//   1. the strips that scroll sideways (.hscroll: main nav, SAT views, Monitor and Advanced tabs, Settings rail, Home chips)
//      show an edge fade on the side with hidden content: data-overflow end / both / start, following the scroll position;
//      after a page or view change the active item is scrolled into view;
//   2. at 1400 px every Settings rail item keeps icon and label on one line (ellipsis, full title in title), Network shows
//      a reboot dot instead of the text badge, and OpenTherm / SAT / S0 use short rail titles;
//   3. a category with at most 2 cards renders in one column of up to 760 px, unless a search is active.
//
// Drives the REAL v2.html/v2.js/v2.css in headless Chrome against a fake device. OTGW_DATA_DIR=<dir> runs it against an older
// copy of data/ (old-vs-fix). OTGW_SHOT_DIR=<dir> saves screenshots (synthetic data).
//
// Run: node --test tests/webui/hscroll-and-rail-2960.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const S = (value) => ({ value, type: 's', maxlen: 30 });
const SETTINGS = { hostname: S('otgw'), wifissid: S('home'), mqttbroker: S('broker.local'), ntphostname: S('pool.ntp.org'),
  otgwcommands: S('PR=A'), s0pulsekw: S('1000'), satkp: S('1.0') };
const ON = (v) => ({ value: v, epoch: 1 });
const OTMON = { otmonitor: { boilertemperature: ON('45.0'), returnwatertemperature: ON('38.0'), dhwtemperature: ON('52.0'),
  roomtemperature: ON('20.4'), roomsetpoint: ON('21.0'), outsidetemperature: ON('8.0'), relmodlvl: ON('30'), chwaterpressure: ON('1.5'),
  flamestatus: ON('On'), chmodus: ON('Off'), dhwmode: ON('On'), faultindicator: ON('Off') } };
function api(method, p) {
  if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : SETTINGS;
  if (p.endsWith('/v2/otgw/otmonitor')) return OTMON;
  if (p.endsWith('/v2/sat/status')) return { sat: { enabled: false, active: false, fallback_active: false, target_temp: 20 } };
  return undefined;
}
const shotDir = process.env.OTGW_SHOT_DIR;
const OV = (sel) => `(function () { var e = document.querySelector(${JSON.stringify(sel)}); return e ? e.getAttribute('data-overflow') : 'missing'; })()`;
// every .hscroll element: the fade attribute must match whether it really overflows (invariant, any page)
const CONSISTENT = `(function () { var bad = [], n = 0; Array.prototype.forEach.call(document.querySelectorAll('.hscroll'), function (e) {
  if (!e.offsetParent && getComputedStyle(e).display === 'none') return; n++;
  var over = e.scrollWidth - e.clientWidth > 1, has = !!e.getAttribute('data-overflow');
  if (over !== has) bad.push((e.id || e.className) + ' over=' + over + ' attr=' + e.getAttribute('data-overflow')); }); return { n: n, bad: bad }; })()`;

for (const theme of ['light', 'dark']) {
  test(`390 px: the main nav fades on the hidden side, follows scrolling and keeps the active page in view (${theme})`, { skip }, async () => {
    const h = await startPage({ width: 390, api, beforeScript: `try { localStorage.setItem('otgw-theme', '${theme}'); } catch (e) {}` });
    try {
      assert.ok(await h.until(`!!document.querySelector('.switcher.hscroll')`, 20000), 'nav has the hscroll class');
      assert.ok(await h.until(`${OV('.switcher')} === 'end'`, 10000), 'more pages to the right: fade at the end, got ' + await h.evalJs(OV('.switcher')));
      await h.evalJs(`document.querySelector('.switcher').scrollLeft = 40`);
      assert.ok(await h.until(`${OV('.switcher')} === 'both'`, 5000), 'scrolled a little: both sides');
      await h.evalJs(`var n = document.querySelector('.switcher'); n.scrollLeft = n.scrollWidth`);
      assert.ok(await h.until(`${OV('.switcher')} === 'start'`, 5000), 'scrolled to the end: fade at the start');
      await h.evalJs(`document.querySelector('.switcher').scrollLeft = 0`);
      await h.evalJs(`document.querySelector('.seg[data-page="advanced"]').click()`);
      assert.ok(await h.until(`(function () { var a = document.querySelector('.seg[data-page="advanced"]').getBoundingClientRect(), n = document.querySelector('.switcher').getBoundingClientRect(); return a.left >= n.left - 1 && a.right <= n.right + 1; })()`, 5000), 'Advanced is in view after the page change');
      if (shotDir) await h.shot(path.join(shotDir, `nav-390-${theme}.png`));
      const c = await h.evalJs(CONSISTENT);
      assert.deepEqual(c.bad, [], 'fade matches overflow on every visible strip: ' + JSON.stringify(c));
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });
}

test('390 px: the Settings rail and the Home chips fade, and the rail keeps the chosen category in view', { skip }, async () => {
  const h = await startPage({ width: 390, api });
  try {
    assert.ok(await h.until(`document.querySelectorAll('#aStrip .mchip').length > 2`, 20000), 'Home chips drawn');
    assert.ok(await h.until(`${OV('#aStrip')} === 'end'`, 5000), 'Home chips fade at the end: ' + await h.evalJs(OV('#aStrip')));
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`document.querySelectorAll('#setRail .rail-item').length > 3`, 20000), 'rail drawn');
    assert.ok(await h.until(`${OV('#setRail')} === 'end'`, 5000), 'rail fades at the end');
    await h.evalJs(`var r = document.querySelectorAll('#setRail .rail-item'); r[r.length - 1].click()`);
    assert.ok(await h.until(`(function () { var a = document.querySelector('#setRail .rail-item.active').getBoundingClientRect(), n = document.getElementById('setRail').getBoundingClientRect(); return a.left >= n.left - 1 && a.right <= n.right + 1; })()`, 5000), 'the chosen category is in view');
    assert.ok(await h.until(`${OV('#setRail')} === 'start'`, 5000), 'rail scrolled to its end: fade at the start');
    if (shotDir) await h.shot(path.join(shotDir, 'rail-390.png'));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('1400 px: rail items keep icon and label on one line, short titles, reboot dot; few cards use one column', { skip }, async () => {
  const h = await startPage({ width: 1400, api });
  const R = {};
  try {
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`document.querySelectorAll('#setRail .rail-item').length > 3`, 20000), 'rail drawn');
    R.items = await h.evalJs(`Array.prototype.map.call(document.querySelectorAll('#setRail .rail-item'), function (b) {
      var ic = b.querySelector('.ri-ic'), lb = b.querySelector('.ri-lbl'), ri = b.querySelector('.ri-reboot');
      var a = ic && ic.getBoundingClientRect(), l = lb && lb.getBoundingClientRect();
      return { cat: b.dataset.cat, label: lb && lb.textContent, title: lb && lb.title, sameLine: !!(a && l && Math.abs((a.top + a.bottom) / 2 - (l.top + l.bottom) / 2) < 3),
        h: Math.round(b.getBoundingClientRect().height), dot: !!ri, textBadge: !!b.querySelector('.badge-reboot') }; })`);
    R.few = await h.evalJs(`(function () { var w = document.querySelector('#setCols .set-cards'); return { cls: w.className, width: Math.round(w.getBoundingClientRect().width), cards: w.querySelectorAll('.set-group').length }; })()`);
    if (shotDir) await h.shot(path.join(shotDir, 'rail-1400.png'));
    await h.evalJs(`var s = document.getElementById('setSearch'); s.value = 'o'; s.dispatchEvent(new Event('input'))`);
    await h.sleep(500);
    R.search = await h.evalJs(`Array.prototype.map.call(document.querySelectorAll('#setCols .set-cards'), function (w) { return w.className; })`);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
  assert.ok(R.items.length > 3 && R.items.every((i) => i.sameLine), 'icon and label on one line: ' + JSON.stringify(R.items.filter((i) => !i.sameLine)));
  assert.ok(R.items.every((i) => i.h < 52), 'no item wraps to two lines: ' + JSON.stringify(R.items.map((i) => i.h)));
  const by = Object.fromEntries(R.items.map((i) => [i.cat, i]));
  assert.equal(by.network.dot, true, 'Network shows the reboot dot'); assert.equal(by.network.textBadge, false, 'no text badge in the rail');
  assert.deepEqual([by.otgw.label, by.otgw.title], ['OpenTherm', 'OpenTherm Gateway']);
  assert.deepEqual([by.s0.label, by.s0.title], ['S0 counter', 'S0 Pulse Counter']);
  assert.deepEqual([by.sat.label, by.sat.title], ['SAT', 'SAT — Smart Thermostat']);
  assert.equal(by.system.title, 'System');
  assert.match(R.few.cls, /set-cards--few/, 'one or two cards: one-column class (' + R.few.cards + ' cards)');
  assert.ok(R.few.width <= 761, 'the cards column is at most 760 px, is ' + R.few.width);
  assert.ok(R.search.length > 0 && R.search.every((c) => !/set-cards--few/.test(c)), 'search active: columns as before: ' + JSON.stringify(R.search));
});
