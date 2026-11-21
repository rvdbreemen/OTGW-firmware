// TASK-2959 F15 (spec T4): the sensor-discovery card body is in the UI font (only a real identifier
// in mono, as <code>), and at 390 px the stack spans the width minus 16 px margins. Before the fix the
// whole hint was mono and the stack kept max-width 340 px at the bottom right.
// It mounts the card markup the page builds (.discover-stack > .discover-card > .discover-bd > .s) and
// reads the real v2.css rules. OTGW_DATA_DIR=<dir> runs it against another tree (old-vs-fix).
//
// Run: node --test tests/webui/discover-card-font-width.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const MOUNT = `(function () {
  var s = document.createElement('div'); s.className = 'discover-stack';
  s.innerHTML = '<div class="discover-card ble"><span class="discover-ic"></span><div class="discover-bd"><div class="t">1 new sensor</div>' +
    '<div class="s" id="tSub">View them in Settings › Sensors. <code>A4:C1:38:00:11:22</code></div></div></div>';
  document.body.appendChild(s);
  var sub = document.getElementById('tSub'), code = sub.querySelector('code');
  var mono = getComputedStyle(document.documentElement).getPropertyValue('--font-mono').trim();
  var r = s.getBoundingClientRect();
  return { subFont: getComputedStyle(sub).fontFamily, codeFont: getComputedStyle(code).fontFamily, mono: mono,
    left: Math.round(r.left), right: Math.round(window.innerWidth - r.right), vw: window.innerWidth };
})()`;

test('390 px: UI font for the hint, mono for the identifier, full width minus 16 px', async () => {
  const h = await startPage({ width: 390, height: 800 });
  try {
    await h.until(`!!document.querySelector('.connsum')`, 8000);
    const r = await h.evalJs(MOUNT);
    console.log('  ' + JSON.stringify(r));
    assert.notEqual(r.subFont, r.codeFont, 'the hint and the identifier use different fonts');
    assert.ok(!/mono/i.test(r.subFont) && r.subFont !== r.mono, 'the hint is not in the mono font: ' + r.subFont);
    assert.equal(r.left, 16); assert.ok(Math.abs(r.right - 16) <= 1, 'right margin ' + r.right);
  } finally { await h.close(); }
});
