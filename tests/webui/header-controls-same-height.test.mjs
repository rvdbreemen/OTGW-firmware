// TASK-1873: on a phone the header theme switch was taller than the language button.
// v2.css @media(max-width:719px) (TASK-932 tap targets) lifts .theme-switch button to 44 px, so the
// switch box is 44 + 2x2 padding + 2x1 border = 50 px, while .lang-menu summary kept its 40 px.
// The test loads the REAL v2.html/v2.css in headless Chrome at a phone width and a desktop width and
// measures both header controls. Red on the old v2.css (50 vs 40 at 390 px), green on the fix.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = { settings: { hostname: S('otgw', 32), mqttbroker: S('broker.local', 30) } };
const api = (m, p) => (p.endsWith('/v2/settings') ? SETTINGS : undefined);
const skip = !CHROME && 'no Chrome';

const SIZES = `(function(){
  var l = document.querySelector('.lang-menu summary'), t = document.querySelector('.theme-switch');
  if (!l || !t || !l.offsetParent || !t.offsetParent) return null;
  return { lang: l.getBoundingClientRect().height, theme: t.getBoundingClientRect().height }; })()`;

async function measure(width) {
  const h = await startPage({ api, width, height: 900 });
  try {
    await h.until(`${SIZES} !== null`);
    return await h.evalJs(SIZES);
  } finally {
    await h.close();
  }
}

test('phone width (390 px): the language button and the theme switch have the same height, both >= 44 px', { skip }, async () => {
  const s = await measure(390);
  assert.ok(Math.abs(s.lang - s.theme) < 0.5, `language ${s.lang} px vs theme switch ${s.theme} px`);
  assert.ok(s.lang >= 44 && s.theme >= 44, `tap targets ${s.lang} / ${s.theme} px, want >= 44`);
});

test('desktop width (1400 px): both stay at their 40 px height (no change outside the phone rule)', { skip }, async () => {
  const s = await measure(1400);
  assert.ok(Math.abs(s.lang - 40) < 0.5, `language ${s.lang} px, want 40`);
  assert.ok(Math.abs(s.theme - 40) < 0.5, `theme switch ${s.theme} px, want 40`);
});
