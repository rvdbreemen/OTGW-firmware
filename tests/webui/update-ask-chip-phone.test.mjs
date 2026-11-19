// TASK-2918: on a phone the open update-check popover must stay inside the screen.
// Found by 09 on .63 with beta.393 (fb753d0) at 390x844 in mobile emulation: the popover sits at left:0 under a
// chip in the middle of the header row, so it runs past the right edge. With a mobile viewport that overflow widens
// the page (innerWidth went 390 -> 533), and the clamp in show() measured against that widened width, so it never
// moved the popover. The earlier 390 px check ran without mobile emulation, where innerWidth stays 390.
// This test drives the REAL v2.html/v2.js in headless Chrome with mobile emulation and checks the open popover
// against the 390 px screen. Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/update-ask-chip-phone.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const B = (v) => ({ value: v, type: 'b' });
const SETTINGS = { updatecheck: B(false), updatecheckasked: B(false), updatecheckbeta: B(false), ui_onboarded: B(true) };
// The question appears 30 s after load; shorten only that timer.
const FAST = `(function(){var st=window.setTimeout;window.setTimeout=function(f,ms){return st.call(window,f,ms===30000?300:ms)};})()`;
const W = 390, H = 844;

test('the open update-check popover stays inside a 390 px phone screen', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ width: W, height: H, beforeScript: FAST, api: (m, p) => {
    if (p.endsWith('/v2/settings') && m === 'GET') return SETTINGS;
    return undefined;
  } });
  try {
    // Phone: mobile viewport (meta viewport width=device-width), so content that overflows widens the page.
    await h.send('Emulation.setDeviceMetricsOverride', { width: W, height: H, deviceScaleFactor: 2, mobile: true });
    await h.send('Page.reload', { ignoreCache: true });
    assert.ok(await h.until(`!!document.getElementById('updAsk')`, 15000), 'the question chip is shown');
    await h.evalJs(`document.getElementById('updAsk').children[0].click()`);
    await h.sleep(400);
    const r = await h.evalJs(`(function(){ var p = document.getElementById('updAskPop').getBoundingClientRect();
      return { l: Math.round(p.left), r: Math.round(p.right), shown: getComputedStyle(document.getElementById('updAskPop')).display,
        iw: innerWidth, sw: document.documentElement.scrollWidth }; })()`);
    assert.equal(r.shown, 'block', 'the popover is open');
    assert.ok(r.l >= 0, 'popover starts on screen: ' + JSON.stringify(r));
    assert.ok(r.r <= W, 'popover ends on screen (right <= ' + W + '): ' + JSON.stringify(r));
    assert.equal(r.iw, W, 'the open popover does not widen the page: ' + JSON.stringify(r));
    assert.ok(r.sw <= W, 'no horizontal scroll: ' + JSON.stringify(r));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
