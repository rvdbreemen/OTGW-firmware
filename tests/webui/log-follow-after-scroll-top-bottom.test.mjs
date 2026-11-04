// TASK-1363: Monitor > Log keeps following the newest lines after the user scrolled to the top and back down.
// Reported on alpha.404 (tester maarten_41762): "the log screen sometimes stops auto updating once I scrolled it
// all the way up and down". Since TASK-1533 the follow state only changes through the Auto-scroll chip.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. The page's WebSocket is replaced by a
// stub (installed before the page scripts run) so the test can push log lines into the real onWsMessage path.
// Steps: fill the log, scroll the real #monLog element to the top, back to the bottom, push more lines, and
// check that the newest lines are shown, the console is scrolled to the bottom and the line counter moves.
// A second case scrolls up and leaves it there: the chip stays on and new lines still arrive (render keeps
// going); the pane is pulled to the bottom again by the next render (follow is the chip's, not the scroll's).
//
// Not covered: mouse-wheel/touch gestures (scrollTop is set from script) and a real device WebSocket.
//
// Run: node --test tests/webui/log-follow-after-scroll-top-bottom.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';

const WS_STUB = `(function () {
  window.__wss = [];
  function FakeWS(url) { this.url = url; this.readyState = 1; window.__wss.push(this); var s = this; setTimeout(function () { if (s.onopen) s.onopen({}); }, 0); }
  FakeWS.prototype.close = function () { this.readyState = 3; };
  FakeWS.prototype.send = function () {};
  FakeWS.OPEN = 1;
  window.WebSocket = FakeWS;
  window.__push = function (txt) { window.__wss.forEach(function (w) { if (w.onmessage) w.onmessage({ data: txt }); }); };
  window.__pushN = function (from, to) { for (var i = from; i <= to; i++) window.__push('12:00:00.000 * line ' + i); };
})();`;

async function openLog() {
  const h = await startPage({ width: 1280, height: 900 });
  await h.send('Page.addScriptToEvaluateOnNewDocument', { source: WS_STUB });
  await h.send('Page.navigate', { url: `http://127.0.0.1:${h.PORT}/v2.html` });
  await h.until(`document.readyState === 'complete' && !!window.__wss && window.__wss.length > 0`, 10000);
  await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
  await h.until(`!!document.querySelector('#page-monitor.active') && !!document.querySelector('#mlog.active')`);
  return h;
}

const state = `(function () {
  var el = document.getElementById('monLog');
  return { count: document.getElementById('logCount').textContent, chipOn: document.getElementById('chipScroll').classList.contains('on'),
           gap: el.scrollHeight - el.clientHeight - el.scrollTop, scrollable: el.scrollHeight > el.clientHeight,
           last: el.textContent.split('\\n').pop() };
})()`;

test('scroll to the top, then to the bottom: new lines keep appearing and the view follows', { skip }, async () => {
  const h = await openLog();
  try {
    await h.evalJs(`window.__pushN(1, 300)`);
    await h.until(`/line 300$/.test(document.getElementById('monLog').textContent)`);
    const s0 = await h.evalJs(state);
    assert.equal(s0.scrollable, true, 'the log must be tall enough to scroll for this test to mean anything');
    assert.ok(s0.gap <= 2, 'not at the bottom after the first batch: ' + JSON.stringify(s0));

    await h.evalJs(`document.getElementById('monLog').scrollTop = 0`);
    await h.sleep(300);
    await h.evalJs(`(function () { var e = document.getElementById('monLog'); e.scrollTop = e.scrollHeight; })()`);
    await h.sleep(300);

    await h.evalJs(`window.__pushN(301, 320)`);
    await h.until(`/line 320$/.test(document.getElementById('monLog').textContent)`);
    const s1 = await h.evalJs(state);
    assert.equal(s1.chipOn, true, 'the Auto-scroll chip went off by itself');
    assert.equal(s1.last, '12:00:00.000 * line 320', 'the newest line is not shown');
    assert.equal(s1.count, '320', 'the line counter stopped');
    assert.ok(s1.gap <= 2, 'the view no longer follows the newest line: ' + JSON.stringify(s1));
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('scrolled up and left there: lines keep arriving and the chip stays on', { skip }, async () => {
  const h = await openLog();
  try {
    await h.evalJs(`window.__pushN(1, 300)`);
    await h.until(`/line 300$/.test(document.getElementById('monLog').textContent)`);
    await h.evalJs(`document.getElementById('monLog').scrollTop = 0`);
    await h.sleep(300);
    await h.evalJs(`window.__pushN(301, 310)`);
    await h.until(`/line 310$/.test(document.getElementById('monLog').textContent)`);
    const s = await h.evalJs(state);
    assert.equal(s.chipOn, true);
    assert.equal(s.count, '310');
    assert.equal(s.last, '12:00:00.000 * line 310');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
