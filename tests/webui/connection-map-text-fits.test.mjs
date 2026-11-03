// TASK-1427: on Monitor > Connection every node label stays inside its box (or inside the
// SVG viewBox for the labels under the Broker and browser nodes), at the reporter's width
// (1343 px), 1280 px and 375 px, and the page never scrolls horizontally.
// The Thermostat/Boiler recency text is the longest case: "master . seen 125s ago".
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/connection-map-text-fits.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const HEALTH = { health: {
  wifirssi: -69, otcommandinterface: 'PIC', thermostatconnected: true, boilerconnected: true,
  thermostat_age_s: 125, boiler_age_s: 1, otgwconnected: true, otgwmode: 'on', networkmode: 'WiFi',
  mqttconnected: true, ntpenable: true,
} };

// For every text in the map: the box it must stay inside, 2 units of padding.
const MEASURE = `(function(){
  var svg = document.getElementById('connMap'), vb = svg.viewBox.baseVal, out = [];
  function bb(t) { var b = t.getBBox(); return { l: b.x, r: b.x + b.width, t: b.y, b: b.y + b.height }; }
  Array.prototype.forEach.call(svg.querySelectorAll('g[id^="n-"]'), function (g) {
    var box = g.querySelector('.cn-box'); if (!box) return;
    var x = +box.getAttribute('x'), w = +box.getAttribute('width');
    Array.prototype.forEach.call(g.querySelectorAll('text'), function (t) {
      if (!t.textContent.trim() || t.closest('#cnOvr')) return;
      var b = bb(t);
      out.push({ node: g.id, text: t.textContent, over: Math.round((b.r - (x + w - 2)) * 10) / 10, under: Math.round(((x + 2) - b.l) * 10) / 10 });
    });
  });
  Array.prototype.forEach.call(svg.querySelectorAll(':scope > text.cn-sub'), function (t) {
    var b = bb(t);
    out.push({ node: 'label', text: t.textContent, over: Math.round((b.r - (vb.x + vb.width - 2)) * 10) / 10, under: Math.round(((vb.x + 2) - b.l) * 10) / 10 });
  });
  Array.prototype.forEach.call(svg.querySelectorAll('.cn-box'), function (r) {
    var x = +r.getAttribute('x'), w = +r.getAttribute('width');
    out.push({ node: 'box', text: r.parentNode.id, over: (x + w + 1) - (vb.x + vb.width), under: 0 });
  });
  var card = svg.parentNode.getBoundingClientRect(), sr = svg.getBoundingClientRect();
  return { rows: out, svgRight: sr.right, cardRight: card.right, scrollW: document.documentElement.scrollWidth, innerW: window.innerWidth,
           svgW: sr.width, scale: sr.width / vb.width };
})()`;

async function measure(width, shot) {
  const h = await startPage({ width, height: 900, api: (m, p) => (p.endsWith('/v2/health') ? HEALTH : undefined) });
  try {
    await h.until(`!!document.querySelector('.connsum')`);
    await h.evalJs(`document.querySelector('.connsum').click()`);
    await h.until(`(document.getElementById('cnThermSub') || {}).textContent && /seen/.test(document.getElementById('cnThermSub').textContent) && document.getElementById('connMap').getBoundingClientRect().width > 0`);
    await h.sleep(500);
    const r = await h.evalJs(MEASURE);
    if (process.env.OTGW_SHOT_DIR && shot) await h.shot(path.join(process.env.OTGW_SHOT_DIR, shot + '.png'));
    assert.deepEqual(h.exceptions, []);
    return r;
  } finally { await h.close(); }
}

const skip = !CHROME && 'no Chrome';
for (const [width, shot] of [[1343, 'connection-1343'], [1280, 'connection-1280'], [375, 'connection-375']]) {
  test(`connection map text stays inside its box at ${width}px`, { skip }, async () => {
    const r = await measure(width, shot);
    const bad = r.rows.filter((x) => x.over > 0 || x.under > 0);
    assert.equal(r.rows.length >= 14, true, 'measured the nodes: ' + r.rows.length);
    assert.deepEqual(bad, [], 'text or box outside its container (units over/under): ' + JSON.stringify(bad));
    assert.ok(r.svgRight <= r.cardRight + 0.5, `svg inside its card (${r.svgRight} <= ${r.cardRight})`);
    assert.ok(r.scrollW <= r.innerW, `no horizontal page scroll (${r.scrollW} <= ${r.innerW})`);
  });
}
