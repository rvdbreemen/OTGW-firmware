// TASK-1533 guard: Monitor > Log with auto-scroll off keeps the shown lines in place.
// Reported by a tester on alpha.577: with auto-scroll off the page kept its scroll position but
// rebuilt the newest lines underneath it on every frame, so the text kept rolling by.
//
// Runs the REAL shipped pushLog() / renderLog() / renderLogCounts() / setLogAutoScroll() cut out of
// data/v2.js against a tiny fake DOM, with requestAnimationFrame run synchronously.
// Old v2.js has no setLogAutoScroll/renderLogCounts, so extraction fails (red).
//
// Not covered: real scrolling in a browser (scrollTop is only recorded).
//
// Run: node --test tests/webui/log-autoscroll-off-freezes.test.mjs
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

function page() {
  const els = {};
  const el = (id) => (els[id] = els[id] || {
    id, text: '', scrollTop: 0, scrollHeight: 1000, classes: new Set(),
    set textContent(v) { this.text = String(v); }, get textContent() { return this.text; },
    appendChild(f) { this.text += f.text; },
    classList: { toggle(c, on) { (on ? els[id].classes.add(c) : els[id].classes.delete(c)); } },
  });
  const document = {
    getElementById: el,
    createDocumentFragment: () => ({ text: '', appendChild(n) { this.text += n.textContent; } }),
    createTextNode: (t) => ({ textContent: t }),
    createElement: () => ({ className: '', textContent: '' }),
  };
  // Old v2.js has neither helper: stand in with what its chip click handler did, so the old
  // pushLog/renderLog run and the test fails on the behaviour, not on extraction.
  const OLD = {
    renderLogCounts: 'function renderLogCounts() {}',
    setLogAutoScroll: 'function setLogAutoScroll(on) { logAutoScroll = !!on; document.getElementById("chipScroll").classList.toggle("on", logAutoScroll); }',
  };
  const body = ['pushLog', 'renderLog', 'renderLogCounts', 'setLogAutoScroll']
    .map((n) => (SRC.indexOf('function ' + n + '(') < 0 && OLD[n]) ? OLD[n] : extractFunction(SRC, n)).join('\n');
  const make = new Function('document', 'requestAnimationFrame',
    'var LOG_MAX = 2000, LOG_SHOW = 2000, LOG_CAPTURE = 200000, logBuf = [], logRecvTimes = [];\n' +
    'var logPaused = false, logSearch = "", logShowTs = true, logAutoScroll = true, logSatOnly = false;\n' +
    'var logStreamWriter = null, logStreamChunk = null, logRenderPending = false;\n' +
    'var SAT_LINE_RE = /^x/;\n' +
    'function isMonitorLogVisible() { return true; }\n' +
    'function eventLineClass() { return ""; }\n' + body +
    '\nreturn { pushLog: pushLog, setLogAutoScroll: setLogAutoScroll, buf: function () { return logBuf; } };');
  const api = make(document, (fn) => fn());
  return { api, shown: () => el('monLog').text, count: () => el('logCount').text, chip: () => el('chipScroll').classes.has('on') };
}

test('auto-scroll off: the shown lines stay put while new lines arrive, and none are lost', () => {
  const p = page();
  for (let i = 1; i <= 5; i++) p.api.pushLog('12:00:0' + i + '.000 line ' + i + '\r\n');
  const before = p.shown();
  assert.match(before, /line 5$/);
  p.api.setLogAutoScroll(false);
  assert.equal(p.chip(), false);
  for (let i = 6; i <= 9; i++) p.api.pushLog('12:00:0' + i + '.000 line ' + i);
  assert.equal(p.shown(), before, 'the view moved while auto-scroll was off');
  assert.equal(p.api.buf().length, 9, 'lines that arrived while auto-scroll was off were dropped');
  assert.equal(p.count(), '9', 'the line counter stopped');
});

test('auto-scroll on again shows the newest lines at once', () => {
  const p = page();
  p.api.pushLog('a');
  p.api.setLogAutoScroll(false);
  p.api.pushLog('b'); p.api.pushLog('c');
  p.api.setLogAutoScroll(true);
  assert.equal(p.chip(), true);
  assert.match(p.shown(), /c$/);
});
