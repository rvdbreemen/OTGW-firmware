// TASK-2958 (UX review F18 F19 F20 F21, spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T3-a11y-structure.md,
// plus spec T5 section 2.1): accessibility structure of the v2 web UI.
//   - a "Skip to content" link is the first Tab stop and moves focus into <main id="main">;
//   - one <h1> (product name), one <main>, the page switcher is <nav aria-label="Main">, the Settings rail nav is
//     labelled, each .page starts with a visually hidden <h2>;
//   - aria-current on the active item of the main nav (page) and of the SAT views, Monitor tabs and Advanced tabs (true);
//   - the SAT switch is named "SAT" in both states, its state text is aria-hidden;
//   - with SAT off the disabled buttons look disabled (opacity .5, cursor not-allowed) and the dial card shows a hint;
//   - one focus ring: outline 2px solid #1e88e5 on keyboard focus;
//   - Settings rail items are <button type="button"> that Enter and Space open (WCAG 2.1.1).
//
// Drives the REAL v2.html/v2.js/v2.css in headless Chrome against a fake device (SAT off).
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/a11y-structure.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, sleep } from './lib/page-harness.mjs';

const S = (value, maxlen = 30) => ({ value, type: 's', maxlen });
const SETTINGS = { hostname: S('otgw'), mqttbroker: S('broker.local'), ntphostname: S('pool.ntp.org') };
function api(method, p) {
  if (p.endsWith('/v2/settings')) return method === 'POST' ? { status: 'ok' } : SETTINGS;
  if (p.endsWith('/v2/sat/status')) return { sat: { enabled: false, active: false, fallback_active: false, target_temp: 20 } };
  return undefined;
}

async function key(h, k) {
  const codes = { Tab: ['Tab', 9], Enter: ['Enter', 13], ' ': ['Space', 32] };
  const [code, vk] = codes[k];
  const base = { key: k, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk };
  await h.send('Input.dispatchKeyEvent', Object.assign({ type: 'keyDown', text: k === 'Enter' ? '\r' : (k === ' ' ? ' ' : undefined) }, base));
  await h.send('Input.dispatchKeyEvent', Object.assign({ type: 'keyUp' }, base));
  await sleep(150);
}
const active = `(function(){ var e = document.activeElement; return e ? (e.tagName + (e.id ? '#' + e.id : '') + (e.className && typeof e.className === 'string' ? '.' + e.className.split(' ')[0] : '')) : null; })()`;

test('accessibility structure: skip link, landmarks, aria-current, SAT switch, disabled style, focus ring, rail buttons', async (t) => {
  const h = await startPage({ api });
  const R = {};
  try {
    await h.send('Emulation.setFocusEmulationEnabled', { enabled: true });
    assert.ok(await h.until(`!!document.querySelector('.seg[data-page="settings"]')`, 30000), 'page loaded');
    for (let i = 0; i < 150 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
    await sleep(800);

    // TASK-2982: initial state, before any click: each group has exactly one aria-current
    R.init = await h.evalJs(`(function(){ function n(sel, val) { return [].slice.call(document.querySelectorAll(sel)).filter(function (b) { return b.getAttribute('aria-current') === val; }).map(function (b) { return b.dataset.page || b.dataset.sv || b.dataset.tab || b.dataset.adv; }); }
      return { seg: n('.seg', 'page'), sv: n('.satview-btn', 'true'), mon: n('#page-monitor .subtab', 'true'), adv: n('#advTabs .subtab', 'true') }; })()`);
    // 1 + 2: structure and the skip link (fresh page, nothing focused yet)
    R.struct = await h.evalJs(`(function(){
      var pages = [].slice.call(document.querySelectorAll('.page'));
      var nav = document.querySelector('nav[aria-label="Main"]');
      return { h1: document.querySelectorAll('h1').length, h1text: (document.querySelector('h1') || {}).textContent || '',
        main: document.querySelectorAll('main').length, mainId: (document.querySelector('main') || {}).id || '',
        navMain: !!nav, navHasSeg: !!(nav && nav.querySelector('.seg')),
        railLabel: (document.getElementById('setRail') || { getAttribute: function(){ return null; } }).getAttribute('aria-label'),
        pageH2: pages.map(function (p) { var f = p.firstElementChild; return !!f && f.tagName === 'H2' && f.classList.contains('sr-only') && f.textContent.trim().length > 0; }) };
    })()`);
    await key(h, 'Tab');
    R.firstTab = await h.evalJs(`(function(){ var e = document.activeElement; var r = e.getBoundingClientRect();
      return { tag: e.tagName, cls: e.className, text: e.textContent.trim(), href: e.getAttribute('href'), visible: r.width > 0 && r.height > 0 && r.left >= 0 && r.top >= 0 && r.left < innerWidth && r.top < innerHeight }; })()`);
    await key(h, 'Enter');
    R.afterSkip = await h.evalJs(active);

    // 6: one focus ring on a keyboard-focused control (next Tab stop after the skip link target)
    await key(h, 'Tab');
    R.ring = await h.evalJs(`(function(){ var e = document.activeElement, c = getComputedStyle(e); return { el: e.tagName, style: c.outlineStyle, color: c.outlineColor, width: c.outlineWidth }; })()`);

    // 3: aria-current on page and view changes
    const cur = `(function(){
      function grp(sel, val) { var all = [].slice.call(document.querySelectorAll(sel)); return all.filter(function (b) { return b.getAttribute('aria-current') === val; }).map(function (b) { return b.dataset.page || b.dataset.sv || b.dataset.tab || b.dataset.adv; }); }
      return { seg: grp('.seg', 'page'), segOther: [].slice.call(document.querySelectorAll('.seg')).filter(function (b) { return b.hasAttribute('aria-current') && b.getAttribute('aria-current') !== 'page'; }).length,
        sv: grp('.satview-btn', 'true'), mon: grp('#page-monitor .subtab', 'true'), adv: grp('#advTabs .subtab', 'true') };
    })()`;
    await h.evalJs(`document.querySelector('.seg[data-page="sat"]').click()`); await sleep(400);
    await h.evalJs(`document.querySelector('.satview-btn[data-sv="control"]').click()`); await sleep(400);
    R.curSat = await h.evalJs(cur);
    // 4 + 5 on the SAT page (SAT off)
    R.sw = await h.evalJs(`(function(){ var i = document.getElementById('satEnable'), l = document.getElementById('satEnableLbl'); return { label: i && i.getAttribute('aria-label'), hidden: l && l.getAttribute('aria-hidden') }; })()`);
    await h.evalJs(`document.querySelector('.satview-btn[data-sv="simple"]').click()`); await sleep(400);
    R.disabled = await h.evalJs(`(function(){
      var b = [].slice.call(document.querySelectorAll('#page-sat button:disabled'));
      var hint = document.getElementById('satDialOffHint');
      return { n: b.length, bad: b.filter(function (x) { var c = getComputedStyle(x); return c.opacity !== '0.5' || c.cursor !== 'not-allowed'; }).map(function (x) { return x.id || x.className; }),
        hint: !!hint && !hint.hidden && hint.getClientRects().length > 0, hintText: hint ? hint.textContent : null };
    })()`);
    await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`); await sleep(400);
    await h.evalJs(`document.querySelector('#page-monitor .subtab[data-tab="mlog"]').click()`); await sleep(300);
    R.curMon = await h.evalJs(cur);
    await h.evalJs(`document.querySelector('.seg[data-page="advanced"]').click()`); await sleep(400);
    await h.evalJs(`document.querySelector('#advTabs .subtab[data-adv="debug"]').click()`); await sleep(300);
    R.curAdv = await h.evalJs(cur);

    // 9: rail items are buttons and open with Enter and Space
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`document.querySelectorAll('#setRail .rail-item').length > 1`, 20000), 'settings rail built');
    R.rail = await h.evalJs(`[].slice.call(document.querySelectorAll('#setRail .rail-item')).map(function (r) { return { tag: r.tagName, type: r.getAttribute('type'), cat: r.dataset.cat, cur: r.getAttribute('aria-current') }; })`);
    const cats = R.rail.map((r) => r.cat);
    await h.evalJs(`(function(){ var r = document.querySelector('#setRail .rail-item[data-cat="${cats[1]}"]'); if (r) r.focus(); })()`);
    await key(h, 'Enter');
    R.afterEnter = await h.evalJs(`(function(){ var a = document.querySelector('#setRail .rail-item.active'); return a ? { cat: a.dataset.cat, cur: a.getAttribute('aria-current') } : null; })()`);
    await h.evalJs(`(function(){ var r = document.querySelector('#setRail .rail-item[data-cat="${cats[0]}"]'); if (r) r.focus(); })()`);
    await key(h, ' ');
    R.afterSpace = await h.evalJs(`(function(){ var a = document.querySelector('#setRail .rail-item.active'); return a ? a.dataset.cat : null; })()`);
    R.cats = cats;
    // TASK-2982 D3: keyboard only: Tab from one rail button reaches the next, Enter opens it, Space opens the next one
    await h.evalJs(`document.querySelector('#setRail .rail-item[data-cat="${cats[0]}"]').focus()`);
    await key(h, 'Tab');
    R.tabReach = await h.evalJs(`document.activeElement && document.activeElement.dataset ? document.activeElement.dataset.cat : null`);
    await key(h, 'Enter');
    R.tabEnter = await h.evalJs(`(function(){ var a = document.querySelector('#setRail .rail-item.active'); return a ? a.dataset.cat : null; })()`);
    if (cats.length > 2) {
      await h.evalJs(`document.querySelector('#setRail .rail-item[data-cat="${cats[1]}"]').focus()`);
      await key(h, 'Tab'); await key(h, ' ');
      R.tabSpace = await h.evalJs(`(function(){ var a = document.querySelector('#setRail .rail-item.active'); return a ? a.dataset.cat : null; })()`);
    }
    // TASK-2982 D2: the main nav ring sits inside the button, so .switcher's overflow does not clip it
    R.segRing = await h.evalJs(`(function(){ var s = document.querySelector('.seg[data-page="home"]'); s.focus({ focusVisible: true }); var c = getComputedStyle(s); return { style: c.outlineStyle, offset: c.outlineOffset }; })()`);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }

  await t.test('one <h1>, one <main id="main">, <nav aria-label="Main">, labelled rail, a hidden <h2> per page', () => {
    assert.equal(R.struct.h1, 1, 'exactly one h1');
    assert.match(R.struct.h1text, /OTGW firmware/);
    assert.equal(R.struct.main, 1, 'exactly one main');
    assert.equal(R.struct.mainId, 'main');
    assert.ok(R.struct.navMain && R.struct.navHasSeg, 'page switcher is <nav aria-label="Main">');
    assert.equal(R.struct.railLabel, 'Settings sections');
    assert.ok(R.struct.pageH2.length >= 5 && R.struct.pageH2.every(Boolean), `each .page starts with h2.sr-only (${R.struct.pageH2})`);
  });
  await t.test('the first Tab focuses a visible "Skip to content" link; Enter moves focus into <main>', () => {
    assert.equal(R.firstTab.tag, 'A', JSON.stringify(R.firstTab));
    assert.match(R.firstTab.text, /Skip to content/);
    assert.equal(R.firstTab.href, '#main');
    assert.ok(R.firstTab.visible, 'skip link visible on focus');
    assert.equal(R.afterSkip, 'MAIN#main.wrap');
  });
  await t.test('one focus ring: 2px solid #1e88e5 on keyboard focus', () => {
    assert.equal(R.ring.style, 'solid', JSON.stringify(R.ring));
    assert.equal(R.ring.color, 'rgb(30, 136, 229)', JSON.stringify(R.ring));
    assert.equal(R.ring.width, '2px');
  });
  await t.test('aria-current: one per group on the first page open, before any click (TASK-2982)', () => {
    assert.equal(R.init.seg.length, 1, `main nav (${R.init.seg})`);
    assert.deepEqual(R.init.sv, ['simple'], 'SAT views'); assert.deepEqual(R.init.mon, ['mlog'], 'Monitor tabs'); assert.deepEqual(R.init.adv, ['pic'], 'Advanced tabs');
  });
  await t.test('aria-current: one per group after page and view changes', () => {
    assert.deepEqual(R.curSat.seg, ['sat']); assert.equal(R.curSat.segOther, 0);
    assert.deepEqual(R.curSat.sv, ['control']);
    assert.deepEqual(R.curMon.seg, ['monitor']); assert.deepEqual(R.curMon.mon, ['mlog']);
    assert.deepEqual(R.curAdv.seg, ['advanced']); assert.deepEqual(R.curAdv.adv, ['debug']);
  });
  await t.test('the SAT switch is named "SAT"; its state text is aria-hidden', () => {
    assert.deepEqual(R.sw, { label: 'SAT', hidden: 'true' });
  });
  await t.test('SAT off: disabled buttons look disabled and the dial card shows the off hint', () => {
    assert.ok(R.disabled.n >= 3, `disabled buttons found (${R.disabled.n})`);
    assert.deepEqual(R.disabled.bad, [], 'opacity .5 and cursor not-allowed on every disabled button');
    assert.ok(R.disabled.hint, 'dial off hint shown');
    assert.match(R.disabled.hintText || '', /SAT is off/);
  });
  await t.test('Settings rail items are buttons that Enter and Space open, the active one has aria-current', () => {
    assert.ok(R.rail.length > 1 && R.rail.every((r) => r.tag === 'BUTTON' && r.type === 'button'), JSON.stringify(R.rail));
    assert.deepEqual(R.afterEnter, { cat: R.cats[1], cur: 'true' }, 'Enter opens the focused category');
    assert.equal(R.afterSpace, R.cats[0], 'Space opens the focused category');
    assert.equal(R.tabReach, R.cats[1], 'Tab moves from one rail button to the next');
    assert.equal(R.tabEnter, R.cats[1], 'Enter on the Tab-reached button opens it');
    if (R.cats.length > 2) assert.equal(R.tabSpace, R.cats[2], 'Space on the Tab-reached button opens it');
  });
  await t.test('the main nav focus ring is drawn inside the button (outline-offset -2px), not clipped (TASK-2982 D2)', () => {
    assert.deepEqual(R.segRing, { style: 'solid', offset: '-2px' });
  });
});

// TASK-2982 D1: a Monitor tab restored from localStorage on page entry carries aria-current, and the default Log otherwise
for (const stored of [null, 'mvalues']) {
  test(`Monitor entry: aria-current on the ${stored || 'default (Log)'} tab before any tab click`, async () => {
    const before = stored ? `try{localStorage.setItem('otgw-v2-montab',${JSON.stringify(stored)})}catch(e){}` : '';
    const h = await startPage({ api, beforeScript: before });
    try {
      assert.ok(await h.until(`!!document.querySelector('.seg[data-page="monitor"]')`, 30000), 'page loaded');
      await sleep(600);
      await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`); await sleep(600);
      const cur = await h.evalJs(`[].slice.call(document.querySelectorAll('#page-monitor .subtab')).filter(function (b) { return b.getAttribute('aria-current') === 'true'; }).map(function (b) { return b.dataset.tab; })`);
      const act = await h.evalJs(`(document.querySelector('#page-monitor .subtab.active') || {dataset:{}}).dataset.tab`);
      assert.deepEqual(cur, [act], `aria-current follows the active tab (${cur} vs ${act})`);
      if (stored) assert.equal(act, stored, 'the stored tab is restored');
      assert.deepEqual(h.exceptions, []);
    } finally { await h.close(); }
  });
}
