// TASK-1392: the Home view words the DHW state and the emitter label neutrally.
//   gas boiler (heating source 1, no storage tank)  -> "Hot water running · tap open" (unchanged)
//   heat pump (source 2) or a DHW storage tank      -> "Hot water demand · charging the tank"
//   emitter label under the room                    -> "SPACE HEATING", never "RADIATORS"
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device (hot water on).
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/home-heatpump-wording.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const ON = (v) => ({ value: v, epoch: 1 });
const OTMON = { otmonitor: {
  boilertemperature: ON('45.0'), returnwatertemperature: ON('38.0'), dhwtemperature: ON('52.0'), roomtemperature: ON('20.4'),
  roomsetpoint: ON('21.0'), outsidetemperature: ON('8.0'), relmodlvl: ON('30'), chwaterpressure: ON('1.5'),
  flamestatus: ON('On'), chmodus: ON('Off'), dhwmode: ON('On'), faultindicator: ON('Off'),
} };

async function home(satStatus, shotName) {
  const h = await startPage({
    width: 1280,
    api: (m, p) => {
      if (p.endsWith('/v2/otgw/otmonitor')) return OTMON;
      if (p.endsWith('/v2/sat/status')) return satStatus;
      return undefined;
    },
  });
  try {
    // v2.js fetches sat/status 2.8 s after load (seed at 2 s); wait for both, then for the render.
    await h.until(`/Hot water/.test((document.getElementById('aStatusTxt') || {}).textContent || '')`);
    for (let i = 0; i < 100 && !h.log.some((l) => l.startsWith('GET /api/v2/sat/status')); i++) await h.sleep(100);
    await h.sleep(800);
    const r = await h.evalJs(`(function(){
      var tags = Array.prototype.map.call(document.querySelectorAll('#schem .label-tag'), function (t) { return t.textContent; });
      return { a: document.getElementById('aStatusTxt').textContent, tags: tags, hp: (document.getElementById('aTFlame').querySelector('.lbl') || {}).textContent };
    })()`);
    // concept B shows the same sentence plus the DHW state word
    await h.evalJs(`(function(){ var b = document.querySelector('.vp-item[data-design="b"]'); if (b) b.click(); })()`);
    await h.sleep(400);
    r.b = await h.evalJs(`(document.getElementById('bStatusTxt') || {}).textContent`);
    r.bState = await h.evalJs(`(document.getElementById('bDhwState') || {}).textContent`);
    await h.evalJs(`(function(){ var a = document.querySelector('.vp-item[data-design="a"]'); if (a) a.click(); })()`);
    await h.sleep(300);
    if (process.env.OTGW_SHOT_DIR && shotName) await h.shot(path.join(process.env.OTGW_SHOT_DIR, shotName + '.png'));
    assert.deepEqual(h.exceptions, []);
    return r;
  } finally { await h.close(); }
}

const skip = !CHROME && 'no Chrome';

test('gas boiler keeps "Hot water running · tap open"; the emitter label is neutral', { skip }, async () => {
  const r = await home({ heating_source: 1, heating_source_detected: 1, dhw_config_tank: false }, '1392-gas');
  assert.equal(r.a, 'Hot water running · tap open');
  assert.equal(r.b, 'Hot water running · tap open');
  assert.equal(r.bState, 'running');
  assert.ok(r.tags.includes('SPACE HEATING'), 'emitter label: ' + JSON.stringify(r.tags));
  assert.ok(!r.tags.some((t) => /radiator/i.test(t)), 'no RADIATORS label: ' + JSON.stringify(r.tags));
});

test('heat pump uses tank-charging wording', { skip }, async () => {
  const r = await home({ heating_source: 2, heating_source_detected: 2, dhw_config_tank: false }, '1392-heatpump');
  assert.equal(r.hp, 'Compressor', 'the page really is in heat-pump mode');
  assert.equal(r.a, 'Hot water demand · charging the tank');
  assert.equal(r.b, 'Hot water demand · charging the tank');
  assert.equal(r.bState, 'charging');
  assert.ok(r.tags.includes('SPACE HEATING') && !r.tags.some((t) => /radiator/i.test(t)), JSON.stringify(r.tags));
});

test('a boiler that reports a DHW storage tank (MsgID 3 HB bit 3) uses the tank wording too', { skip }, async () => {
  const r = await home({ heating_source: 1, heating_source_detected: 1, dhw_config_tank: true }, '1392-gas-tank');
  assert.equal(r.a, 'Hot water demand · charging the tank');
  assert.equal(r.bState, 'charging');
});
