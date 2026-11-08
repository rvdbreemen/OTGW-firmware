// TASK-1681: the OT-Direct override panel (Monitor > Connection) listed every write override as
// "Write · MsgID N" with no way to clear it. tjfs saw MsgID 1 = 45.0 C on a fresh unit and could
// not tell what set it. Each write row now names its command and either offers Clear, which sends
// the firmware's own clear command, or says why it cannot be cleared here.
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake OT-Direct device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/otd-override-clear.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage } from './lib/page-harness.mjs';

const HEALTH = { health: { otcommandinterface: 'OT-Direct', wifirssi: -55 } };
const DEVICE = { device: { otcommandinterface: 'OT-Direct', hardwaremode: 'OT-Direct', otdirectavailable: true } };

// Opens Monitor > Connection on a fake OT-Direct device whose overrides list is `write`, and
// returns the harness plus the rendered override rows.
async function openPanel(write, satOwns = false) {
  const ov = { overrides: { write, response: [], modify: [], unknown: [], sat_owns: satOwns },
    queue: { depth: 0, highWater: 0, capacity: 12 } };
  const h = await startPage({ api: (m, p) => {
    if (p.endsWith('/v2/otdirect/overrides')) return ov;
    if (p.endsWith('/v2/health')) return HEALTH;
    if (p.endsWith('/v2/device/info')) return DEVICE;
    if (p.endsWith('/v2/otgw/commands')) return { status: 'queued' };
    return undefined;
  } });
  assert.ok(await h.until(`!!document.querySelector('.seg[data-page="monitor"]')`, 20000), 'v2.js started');
  await h.evalJs(`document.querySelector('.seg[data-page="monitor"]').click()`);
  await h.until(`(function(){ var t = document.querySelector('.subtab[data-tab="mconn"]'); if (!t) return false; t.click(); return true; })()`, 20000);
  // Wait for the rendered list, not for the first child: under CPU load the panel can still be empty.
  const rows = await h.until(`(function(){ var el = document.getElementById('otdOvrList'); if (!el || el.children.length < ${write.length}) return null;
    return [].slice.call(el.children).map(function (r) { var b = r.querySelector('button');
      return { text: r.textContent, clear: b ? b.textContent : '' }; }); })()`, 30000);
  if (!rows) {
    const st = await h.evalJs(`(function(){ var el = document.getElementById('otdOvrList'), p = document.getElementById('otdOvr');
      return { panel: p ? p.style.display : 'none?', kids: el ? el.children.length : -1, text: el ? el.textContent.slice(0, 200) : '' }; })()`);
    assert.fail(`override list not rendered: ${JSON.stringify(st)} api=${JSON.stringify(h.log.map((l) => l.split(' ').slice(0, 2).join(' ')))}`);
  }
  return { h, rows };
}

const isPost = (l) => l.startsWith('POST /api/v2/otgw/commands');
const isList = (l) => l.startsWith('GET /api/v2/otdirect/overrides');

// The row for one MsgID: the value (0x....) follows the id without a space in the row text, so the id
// must be followed by exactly "0x" (MsgID 1 must not match "MsgID 10x..." or "MsgID 100x...").
const idRe = (id) => `/MsgID ${id}(?=0x)/`;
const row = (rows, id) => rows.find((r) => new RegExp(`MsgID ${id}(?=0x)`).test(r.text));

// Clicks the Clear button of the row whose text matches `re`; returns the command bodies POSTed.
// Waits for the POST and for the list refetch that follows it, so the next click sees a settled list
// and no late POST of this click lands in the next one's window.
async function clickClear(h, re) {
  const n = h.log.length;
  await h.evalJs(`(function(){ var rows = document.getElementById('otdOvrList').children;
    for (var i = 0; i < rows.length; i++) if (${re}.test(rows[i].textContent)) { rows[i].querySelector('button').click(); return; } })()`);
  for (let i = 0; i < 150 && !h.log.slice(n).some(isPost); i++) await h.sleep(100);
  const p = n + h.log.slice(n).findIndex(isPost);
  for (let i = 0; i < 150 && !h.log.slice(p + 1).some(isList); i++) await h.sleep(100);
  await h.sleep(300);
  return h.log.slice(n).filter(isPost).map((l) => l.slice(l.indexOf('{')));
}

test('a CS write override is named and Clear sends CS=0', async () => {
  const { h, rows } = await openPanel([{ msgid: 1, value: 11520 }]);
  try {
    const cs = row(rows, 1);
    assert.ok(cs, `a row for MsgID 1 (${JSON.stringify(rows)})`);
    assert.match(cs.text, /CS/, 'the row names the CS command');
    assert.equal(cs.clear, 'Clear', 'the row offers Clear');
    const posts = await clickClear(h, idRe(1));
    assert.deepEqual(posts.map((b) => JSON.parse(b).command), ['CS=0'], 'Clear sends the firmware clear command');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('while SAT owns the heating commands, a CS row says so and offers no Clear', async () => {
  const { h, rows } = await openPanel([{ msgid: 1, value: 9164 }], true);
  try {
    const cs = row(rows, 1);
    assert.ok(cs, `a row for MsgID 1 (${JSON.stringify(rows)})`);
    assert.equal(cs.clear, '', 'no Clear: the firmware refuses CS= from other channels while SAT is enabled (TASK-1695)');
    assert.match(cs.text, /SAT controls this/, 'the row says why');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('a room setpoint row names every source that sets MsgID 16', async () => {
  const { h, rows } = await openPanel([{ msgid: 16, value: 0x1400 }]);
  try {
    const rs = row(rows, 16);
    assert.ok(rs, `a row for MsgID 16 (${JSON.stringify(rows)})`);
    assert.match(rs.text, /TT\/TC/, 'TT= and TC= set it');
    assert.match(rs.text, /BS/, 'BS= sets it too (OTDirect.ino BS branch)');
    assert.match(rs.text, /MQTT/, 'and the MQTT room_setpoint route');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

// The clear command per write override, from the OT-Direct command handler (OTDirect.ino):
// CS=0 / C2=0 / SW=0 / SH=0 clear their MsgID, CL=0 clears 7 (CC= has no clear of its own),
// a non-numeric MM= clears 14, TT=0 clears 16 and 100.
const CLEARS = { 7: 'CL=0', 8: 'C2=0', 14: 'MM=T', 16: 'TT=0', 56: 'SW=0', 57: 'SH=0', 100: 'TT=0' };
const ALL = Object.keys(CLEARS).map((id) => ({ msgid: +id, value: 0x1400 }));

test('every clearable write override sends its own clear command', async () => {
  const { h, rows } = await openPanel(ALL);
  try {
    for (const [id, cmd] of Object.entries(CLEARS)) {
      const r = row(rows, id);
      assert.ok(r && r.clear === 'Clear', `MsgID ${id} offers Clear (${JSON.stringify(r)}) rows=${JSON.stringify(rows)}`);
      const posts = await clickClear(h, idRe(id));
      assert.deepEqual(posts.map((b) => JSON.parse(b).command), [cmd], `MsgID ${id} Clear sends ${cmd}`);
    }
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('while SAT owns the heating commands, CL, SW and SH stay clearable and the others do not', async () => {
  const { h, rows } = await openPanel(ALL, true);
  try {
    for (const id of [8, 14, 16, 100]) {
      const r = row(rows, id);
      assert.ok(r && r.clear === '' && /SAT controls this/.test(r.text), `MsgID ${id}: SAT reason, no Clear (${JSON.stringify(r)})`);
    }
    for (const id of [7, 56, 57]) {
      const r = row(rows, id);
      assert.ok(r && r.clear === 'Clear', `MsgID ${id}: TASK-1695 leaves CL=/SW=/SH= free, so Clear stays (${JSON.stringify(r)})`);
    }
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
