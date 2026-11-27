// TASK-3124 guard: the page's WebSocket reconnect backs off (5, 10, 20, 40 s, then 60 s, +-20 % jitter) instead of a fixed 5 s;
// the attempt counter resets only after a socket stayed open 60 s; three sockets that opened and closed within 60 s pause the
// page until the Reconnect button; fetchSeed from onclose and the 20 s timer runs at most once per 30 s.
//
// Runs the REAL code of data/v2.js: connectWs()/onclose handlers and the backoff block are cut out by name and run against a fake
// WebSocket, a fake clock (setTimeout/Date.now) and counters for fetchSeed. OTGW_V2_JS=<older v2.js> shows it red.
// OTGW_TIMELINE=1 prints the connection-attempt timeline per scenario (the before/after evidence).
//
// Run: node --test tests/webui/ws-reconnect-backoff-3124.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function block() {
  const a = SRC.indexOf('function connectWs()');
  assert.ok(a >= 0, 'connectWs not found in ' + V2JS);
  const b = SRC.indexOf('// ---------- render scheduling', a);
  assert.ok(b > a, 'end marker not found');
  return SRC.slice(a, b);
}

// scenario: sockets = array of behaviours consumed in order: 'refuse' (close without open), {open: ms} (open, close after ms), 'hold' (open, stay)
function harness(sockets, rnd = () => 0.5) {
  let now = 1000000; const timers = []; const attempts = []; let seeds = 0; const calls = { render: 0 };
  const fakeSetTimeout = (fn, ms) => { const t = { fn, at: now + ms, id: timers.length + 1 }; timers.push(t); return t.id; };
  const fakeClearTimeout = (id) => { const t = timers.find((x) => x.id === id); if (t) t.dead = true; };
  const Dt = { now: () => now };
  const queue = sockets.slice();
  function FakeWS() {
    const beh = queue.shift() || 'refuse';
    attempts.push(now);
    const s = this; s.readyState = 0; s.close = () => { if (s.readyState !== 3) { s.readyState = 3; fakeSetTimeout(() => s.onclose && s.onclose(), 0); } };
    fakeSetTimeout(() => {
      if (beh === 'refuse') { s.readyState = 3; s.onerror && s.onerror(); s.onclose && s.onclose(); return; }
      s.readyState = 1; s.onopen && s.onopen();
      if (beh !== 'hold') fakeSetTimeout(() => { s.readyState = 3; s.onclose && s.onclose(); }, beh.open);
    }, 1);
  }
  const body = `
    var ws = null, wsTimer = null, wsOpenMs = 0, wsLastMsgMs = 0, WS_PATH = '/ws', BASE = '', location = { protocol: 'http:', host: 'x' };
    var psMode = false;
    function noteWsMark() {} function onWsMessage() {} function fetchSeed() { H.seeds++; } function renderConnDetail() { H.render++; }
    ${block()}
    return { connectWs: connectWs, wsResume: wsResume, seedThrottled: seedThrottled, state: function () { return { paused: wsPaused, attempt: wsAttempt, hasTimer: !!wsTimer }; } };
  `;
  const H = { seeds: 0, render: 0 };
  const api = new Function('WebSocket', 'setTimeout', 'clearTimeout', 'Date', 'Math', 'H', body)(FakeWS, fakeSetTimeout, fakeClearTimeout, Dt, { random: rnd, min: Math.min, round: Math.round }, H);
  const run = (ms) => { const end = now + ms; for (;;) { const due = timers.filter((t) => !t.dead && t.at <= end).sort((x, y) => x.at - y.at || x.id - y.id)[0]; if (!due) break; due.dead = true; now = Math.max(now, due.at); due.fn(); } now = end; };
  return { api, run, attempts, H, advance: (ms) => { now += ms; }, t0: now, nowMs: () => now };
}
const gaps = (a) => a.slice(1).map((v, i) => Math.round((v - a[i]) / 1000));

test('refusing endpoint: attempts back off 5, 10, 20, 40, 60, 60 s (jitter midpoint)', () => {
  const h = harness([]);
  h.api.connectWs(); h.run(400000);
  const g = gaps(h.attempts).slice(0, 6);
  if (process.env.OTGW_TIMELINE) console.log('refusing endpoint, attempt gaps (s):', g.join(' '));
  assert.deepEqual(g, [5, 10, 20, 40, 60, 60]);
});

test('jitter stays within +-20 %', () => {
  for (const r of [0, 1]) {
    const h = harness([], () => r); h.api.connectWs(); h.run(200000);
    const g = gaps(h.attempts).slice(0, 3);
    assert.ok(g[0] >= 4 && g[0] <= 6 && g[1] >= 8 && g[1] <= 12 && g[2] >= 16 && g[2] <= 24, 'gaps ' + g);
  }
});

test('a socket that stayed open 60 s resets the delay to 5 s', () => {
  const h = harness(['refuse', 'refuse', { open: 61000 }, 'refuse']);
  h.api.connectWs(); h.run(200000);
  const g = gaps(h.attempts);
  assert.equal(g[0], 5); assert.equal(g[1], 10);
  assert.equal(g[2], 66, 'stable socket: 61 s open + the reset 5 s delay (not 10 or 20): ' + g);
  assert.equal(g[3], 10, 'and growth starts again from 5 s: ' + g);
});

test('a socket that opens and closes at once does not reset the delay', () => {
  const h = harness([{ open: 100 }, 'refuse', 'refuse']);
  h.api.connectWs(); h.run(100000);
  const g = gaps(h.attempts);
  assert.ok(g[1] >= 9, 'second gap must be the grown delay (10 s), not 5 s: ' + g);
});

test('three sockets that opened and closed within 60 s pause the page; Reconnect resumes', () => {
  const h = harness([{ open: 1000 }, { open: 1000 }, { open: 1000 }, 'hold']);
  h.api.connectWs(); h.run(300000);
  assert.equal(h.attempts.length, 3, 'no fourth automatic attempt');
  assert.equal(h.api.state().paused, true);
  assert.equal(h.api.state().hasTimer, false);
  if (process.env.OTGW_TIMELINE) console.log('flapping endpoint: attempts', h.attempts.length, 'then paused');
  h.api.wsResume(); h.run(5000);
  assert.equal(h.attempts.length, 4, 'resume connects at once');
  assert.equal(h.api.state().paused, false);
  assert.equal(h.api.state().attempt, 0);
});

test('refused connects alone never pause (a rebooting device comes back)', () => {
  const h = harness([]); h.api.connectWs(); h.run(500000);
  assert.equal(h.api.state().paused, false);
  assert.ok(h.attempts.length > 6);
});

test('fetchSeed runs at most once per 30 s, from onclose and from the 20 s timer', () => {
  const h = harness([{ open: 100 }, { open: 100 }, { open: 100 }]);
  h.api.connectWs(); h.run(25000);
  assert.equal(h.H.seeds, 1, 'three closes within 30 s give one seed');
  h.api.seedThrottled(); assert.equal(h.H.seeds, 1);
  h.advance(31000); h.api.seedThrottled(); assert.equal(h.H.seeds, 2);
});

test('call sites: the 20 s timer uses the throttled seed; the pause text and button exist in the pack source', () => {
  assert.match(SRC, /setInterval\(function \(\) \{ if \(!ws \|\| ws\.readyState !== 1\) seedThrottled\(\); \}, 20000\)/);
  assert.match(SRC, /conn_ws_paused/); assert.match(SRC, /conn_ws_resume/);
});
