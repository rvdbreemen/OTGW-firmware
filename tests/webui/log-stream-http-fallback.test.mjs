// TASK-1476 guard: Monitor > Log "Stream to file" must work on the plain-HTTP device page.
// showSaveFilePicker exists only in a secure context, and the device serves plain HTTP, so
// there the chip falls back to part files: every new frame (also while the display is
// paused) is collected and downloaded every 5 minutes, early at LOG_CAPTURE lines, and the
// remainder on switch-off. Each part holds only the frames since the previous part.
//
// Runs the REAL shipped pushLog() / flushStreamChunk() / toggleStream() cut out of
// data/v2.js, with a fake timer and a fake download. Older v2.js has no flushStreamChunk,
// so extraction fails (red).
//
// Not covered: the browser download itself, Chrome's "allow multiple downloads" prompt,
// and the file-picker path in a secure context (bench / HTTPS proxy).
//
// Run: node --test tests/webui/log-stream-http-fallback.test.mjs
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

const LOG_CAPTURE = Number(SRC.match(/LOG_CAPTURE = (\d+)/)[1]);
const BODY = ['pushLog', 'flushStreamChunk', 'toggleStream'].map(n => extractFunction(SRC, n)).join('\n');

// A fresh page: no picker (plain HTTP), display paused so pushLog returns before rendering.
function page() {
  const downloads = [], timers = [];
  const env = {
    window: {},
    downloadBlob: (blob, name) => downloads.push({ blob, name }),
    ts: () => '20261029-120000',
    setInterval: (fn, ms) => { timers.push({ fn, ms, live: true }); return timers.length; },
    clearInterval: id => { if (timers[id - 1]) timers[id - 1].live = false; },
  };
  const make = new Function('window', 'downloadBlob', 'ts', 'setInterval', 'clearInterval',
    'var LOG_CAPTURE = ' + LOG_CAPTURE + ', logStreamWriter = null, logStreamChunk = null, logStreamTimer = null;\n' +
    'var logRecvTimes = [], logPaused = true, logBuf = [];\n' + BODY +
    '\nreturn { pushLog: pushLog, toggleStream: toggleStream };');
  const api = make(env.window, env.downloadBlob, env.ts, env.setInterval, env.clearInterval);
  const cls = new Set();
  const chip = { classList: { add: c => cls.add(c), remove: c => cls.delete(c), contains: c => cls.has(c) } };
  const tick = () => timers.filter(t => t.live).forEach(t => t.fn());
  const text = async i => (await downloads[i].blob.text()).split('\n').filter(Boolean);
  return { ...api, chip, downloads, timers, tick, text };
}

test('without a file picker the chip switches on and arms a 5-minute part timer', () => {
  const p = page();
  p.toggleStream(p.chip);
  assert.equal(p.chip.classList.contains('on'), true);
  assert.equal(p.timers.filter(t => t.live).length, 1);
  assert.equal(p.timers[0].ms, 300000);
  assert.equal(p.downloads.length, 0);
});

test('parts hold only the new frames, also while paused, rest on switch-off', async () => {
  const p = page();
  p.toggleStream(p.chip);
  ['f1\r\n', 'f2\r\n', 'f3\r\n'].forEach(p.pushLog);
  p.tick();                                  // 5 minutes
  assert.equal(p.downloads.length, 1);
  assert.deepEqual(await p.text(0), ['f1', 'f2', 'f3']);
  p.tick();                                  // nothing new: no empty part
  assert.equal(p.downloads.length, 1);
  ['f4', 'f5'].forEach(p.pushLog);
  p.toggleStream(p.chip);                    // off flushes the rest
  assert.equal(p.chip.classList.contains('on'), false);
  assert.equal(p.downloads.length, 2);
  assert.deepEqual(await p.text(1), ['f4', 'f5']);
  assert.match(p.downloads[1].name, /^otgw-stream-.*\.txt$/);
  assert.equal(p.timers.filter(t => t.live).length, 0);
  p.pushLog('f6'); p.tick();                 // off: nothing collected any more
  assert.equal(p.downloads.length, 2);
});

test('a part is downloaded early at LOG_CAPTURE lines, so memory stays bounded', async () => {
  const p = page();
  p.toggleStream(p.chip);
  for (let i = 0; i < LOG_CAPTURE; i++) p.pushLog('x' + i);
  assert.equal(p.downloads.length, 1);
  assert.equal((await p.text(0)).length, LOG_CAPTURE);
  p.pushLog('next');
  p.toggleStream(p.chip);
  assert.deepEqual(await p.text(1), ['next']);
});

test('the chip is not dimmed and its tooltip no longer blames the browser', () => {
  assert.doesNotMatch(SRC, /Stream to file needs Chrome \/ Edge/);
  assert.doesNotMatch(SRC, /cst\.style\.opacity/);
});
