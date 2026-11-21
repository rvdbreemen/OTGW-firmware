// TASK-3017 (client side): a /ws socket that stays open but goes silent never healed, because v2.js only
// reconnects from ws.onclose. The firmware sends {"type":"keepalive"} every 30 s to every client
// (webSocketStuff.ino KEEPALIVE_INTERVAL_MS), so silence well beyond that means the stream is stuck.
// The watchdog closes such a socket so onclose reconnects; any message, keepalive included, counts as life.
//
// Runs the REAL shipped connectWs() and wsWatchdogTick() cut out of data/v2.js, against a fake WebSocket
// and a fake clock (as graph-sat-target.test.mjs does for the graph). OTGW_V2_JS=<older v2.js> shows it red.
//
// Run: node --test tests/webui/ws-silence-watchdog.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');

function extractFunction(src, name) {
  const start = src.indexOf('function ' + name + '(');
  if (start < 0) throw new Error(name + ' not found in ' + V2JS);
  let i = src.indexOf('{', start), depth = 0;
  for (; i < src.length; i++) {
    const c = src[i];
    if (c === "'" || c === '"' || c === '`') {
      for (i++; i < src.length && src[i] !== c; i++) if (src[i] === '\\') i++;
    } else if (c === '/' && src[i + 1] === '/') {
      i = src.indexOf('\n', i);
    } else if (c === '/' && src[i + 1] === '*') {
      i = src.indexOf('*/', i) + 1;
    } else if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return src.slice(start, i + 1);
  }
  throw new Error('unbalanced braces in ' + name);
}
function extractVarLine(src, name) {
  const a = src.indexOf('var ' + name + ' = ');
  if (a < 0) throw new Error(name + ' not found in ' + V2JS);
  return src.slice(a, src.indexOf('\n', a));
}

function load() {
  const body =
    'var clock = 1000000, fakeDate = { now: function () { return clock; } };\n' +
    'var location = { protocol: "http:", host: "gw" }, BASE = "/", WS_PATH = "ws";\n' +
    'var ws = null, wsTimer = null, wsOpenMs = 0, wsLastMsgMs = 0, psMode = false, reconnects = 0, seen = [];\n' +
    'function noteWsMark() {}\nfunction fetchSeed() {}\nfunction scheduleReconnect() { reconnects++; }\n' +
    'function onWsMessage(ev) { seen.push(ev.data); }\n' +
    'function WebSocket(url) { this.url = url; this.readyState = 0; this.closed = 0; }\n' +
    'WebSocket.prototype.close = function () { this.closed++; this.readyState = 3; if (this.onclose) this.onclose({}); };\n' +
    extractVarLine(SRC, 'WS_SILENCE_MS') + '\n' +
    [extractFunction(SRC, 'connectWs'), extractFunction(SRC, 'wsWatchdogTick')].join('\n').replace(/\bDate\.now\b/g, 'fakeDate.now') + '\n' +
    'return { connect: connectWs, tick: wsWatchdogTick, ws: function () { return ws; }, silenceMs: WS_SILENCE_MS,' +
    ' advance: function (ms) { clock += ms; }, setPs: function (v) { psMode = v; }, reconnects: function () { return reconnects; }, seen: seen };';
  return new Function(body)();
}
function openSocket(h) {
  h.connect();
  const s = h.ws(); s.readyState = 1; s.onopen({});
  return s;
}

test('the silence limit is well above the firmware keepalive interval (30 s)', () => {
  const h = load();
  const ka = Number((/KEEPALIVE_INTERVAL_MS\s*=\s*(\d+)/.exec(fs.readFileSync(path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'webSocketStuff.ino'), 'utf8')) || [])[1]);
  assert.equal(ka, 30000, 'firmware keepalive interval');
  assert.ok(h.silenceMs >= 2.5 * ka, 'limit ' + h.silenceMs + ' ms vs keepalive ' + ka + ' ms');
});

test('an open socket that stays silent past the limit is closed, so onclose reconnects', () => {
  const h = load();
  const s = openSocket(h);
  h.advance(h.silenceMs - 1000); h.tick();
  assert.equal(s.closed, 0, 'not yet at the limit');
  h.advance(2000); h.tick();
  assert.equal(s.closed, 1, 'closed after the limit');
  assert.equal(h.reconnects(), 1, 'onclose scheduled the reconnect');
});

test('a keepalive counts as life: a socket with only keepalives is never closed; messages still reach onWsMessage', () => {
  const h = load();
  const s = openSocket(h);
  for (let i = 0; i < 10; i++) { h.advance(30000); s.onmessage({ data: '{"type":"keepalive"}' }); h.tick(); }
  assert.equal(s.closed, 0);
  assert.equal(h.seen.length, 10, 'onWsMessage still gets every message');
});

test('no close while connecting, while closed, or in PS=1 mode', () => {
  const h = load();
  h.connect();
  const s = h.ws();   // readyState 0: still connecting
  h.advance(h.silenceMs * 3); h.tick();
  assert.equal(s.closed, 0, 'connecting socket left alone');
  s.readyState = 1; s.onopen({});
  h.setPs(true); h.advance(h.silenceMs * 3); h.tick();
  assert.equal(s.closed, 0, 'PS=1: left to the PS=1 check');
});
