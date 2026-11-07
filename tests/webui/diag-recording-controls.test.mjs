// TASK-1700: Advanced > Debug Information has start, stop and download controls for the diagnostic
// recording (ADR-213), with a status line from /api/v2/diag/recording/status.
// - Stopped, no data: Start enabled, Stop and Download disabled, the status line says stopped.
// - Start: POST /api/v2/diag/recording/start, then the status shows "recording" and the ring size;
//   Start disabled, Stop enabled.
// - A refused start shows the reason the firmware gives.
// - Download: one GET of /api/v2/diag/recording through an <a download> link (no fetch burst), file *.bin.
// - Stop: POST /api/v2/diag/recording/stop.
// - The calls run one after another (ADR-165: the page never has two diag calls in flight).
//
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device. OTGW_DATA_DIR at an older
// copy of data/ shows it red.
//
// Run: node --test tests/webui/diag-recording-controls.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { startPage, CHROME } from './lib/page-harness.mjs';

const skip = !CHROME && 'no Chrome';
const status = (o) => ({ recording: Object.assign({ state: 'stopped', enabled: false, active: false, reason: '', psram: true,
  block_size: 4096, ring_blocks: 0, ring_bytes: 0, has_data: false, oldest_seq: 0, newest_seq: 0, dropped: 0, exact: true }, o) });

// psram = false models a board without PSRAM (ADR-213: a 4 KB heap ring that Stop frees)
function fakeDevice(refuse, psram = true) {
  const mk = (o) => status(Object.assign({ psram }, psram ? {} : { ring_blocks: 1, ring_bytes: 4096 }, o));
  let rec = mk({});
  const calls = [];
  let inflight = 0, maxInflight = 0;
  const api = (method, p) => {
    if (!/\/v2\/diag\/recording/.test(p)) return undefined;
    calls.push(method + ' ' + p);
    inflight++; maxInflight = Math.max(maxInflight, inflight); setTimeout(() => { inflight--; }, 0);
    if (p.endsWith('/v2/diag/recording/start')) {
      if (refuse) {
        rec = mk({ state: 'refused', reason: 'free heap would drop below 5120 B' });
        return { ok: false, result: 'refused', reason: 'free heap would drop below 5120 B', recording: rec.recording };
      }
      rec = mk(psram ? { state: 'recording', enabled: true, active: true, ring_blocks: 384, ring_bytes: 1572864, has_data: true }
                         : { state: 'recording', enabled: true, active: true, has_data: true });
      return { ok: true, result: 'started', reason: '', recording: rec.recording };
    }
    if (p.endsWith('/v2/diag/recording/stop')) {
      rec = mk({ state: 'stopped', has_data: true, oldest_seq: 0, newest_seq: 2 });
      return { ok: true, result: 'stopped', reason: '', recording: rec.recording };
    }
    if (p.endsWith('/v2/diag/recording/status')) return rec;
    return undefined;
  };
  return { api, calls, get maxInflight() { return maxInflight; } };
}

const STATE = `(function(){ var g = function (id) { return document.getElementById(id); };
  return { status: (g('diagRecStatus') || {}).textContent || '', start: g('diagRecStart') && !g('diagRecStart').disabled,
           stop: g('diagRecStop') && !g('diagRecStop').disabled,
           dl: !!g('diagRecDownload') && g('diagRecDownload').getAttribute('aria-disabled') !== 'true',
           href: g('diagRecDownload') && (g('diagRecDownload').getAttribute('href') || ''),
           file: g('diagRecDownload') && (g('diagRecDownload').getAttribute('download') || '') }; })()`;

async function openDebug(h) {
  // Page load and the first panel fetches get 20 s: on a loaded build PC Chrome can take longer than
  // the harness's 5 s default (seen once: a 20 s run that failed in this first wait).
  // The nav buttons exist in the HTML before v2.js binds their click handlers; .connsum is built by
  // v2.js at start-up, so from then on a click opens the panel (a click before it is lost).
  assert.ok(await h.until(`!!document.querySelector('.connsum')`, 20000), 'v2.js started');
  await h.evalJs(`document.querySelector('button.seg[data-page="advanced"]').click()`);
  await h.evalJs(`document.querySelector('.subtab[data-adv="debug"]').click()`);
  assert.ok(await h.until(`!!document.getElementById('diagRecStatus') && /stopped|recording|refused/i.test(document.getElementById('diagRecStatus').textContent)`, 20000),
    'the Debug panel shows the diag recording status');
}

test('start, stop and download the diagnostic recording', { skip }, async () => {
  const dev = fakeDevice(false);
  const h = await startPage({ api: dev.api });
  try {
    await openDebug(h);
    let s = await h.evalJs(STATE);
    assert.match(s.status, /stopped/i);
    assert.deepEqual([s.start, s.stop, s.dl], [true, false, false], 'stopped without data: only Start');

    await h.evalJs(`document.getElementById('diagRecStart').click()`);
    assert.ok(await h.until(`/recording/i.test(document.getElementById('diagRecStatus').textContent)`), 'status shows recording');
    s = await h.evalJs(STATE);
    assert.match(s.status, /1\.5 MB|1536 KB|384 blocks/i, 'the ring size is shown: ' + s.status);
    assert.deepEqual([s.start, s.stop, s.dl], [false, true, true], 'recording: Stop and Download');
    assert.ok(dev.calls.includes('POST /api/v2/diag/recording/start'), dev.calls.join(','));

    assert.match(s.href, /\/api\/v2\/diag\/recording$/, 'the download link points at the recording route');
    assert.match(s.file, /\.bin$/, 'the download is saved as a .bin file');

    await h.evalJs(`document.getElementById('diagRecStop').click()`);
    assert.ok(await h.until(`/stopped/i.test(document.getElementById('diagRecStatus').textContent)`), 'status shows stopped');
    s = await h.evalJs(STATE);
    assert.deepEqual([s.start, s.stop, s.dl], [true, false, true], 'stopped with data: Start and Download');
    assert.ok(dev.calls.includes('POST /api/v2/diag/recording/stop'), dev.calls.join(','));
    assert.equal(dev.maxInflight, 1, 'never two diag calls in flight');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('without PSRAM the status warns that Stop clears the recording', { skip }, async () => {
  const dev = fakeDevice(false, false);
  const h = await startPage({ api: dev.api });
  try {
    await openDebug(h);
    await h.evalJs(`document.getElementById('diagRecStart').click()`);
    assert.ok(await h.until(`/^Recording/.test(document.getElementById('diagRecStatus').textContent)`), 'status shows recording');
    const s = await h.evalJs(STATE);
    assert.match(s.status, /no PSRAM: Stop clears the recording, download it first/, s.status);
    assert.doesNotMatch(s.status, /in PSRAM/, s.status);
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});

test('a refused start shows the reason', { skip }, async () => {
  const dev = fakeDevice(true);
  const h = await startPage({ api: dev.api });
  try {
    await openDebug(h);
    await h.evalJs(`document.getElementById('diagRecStart').click()`);
    assert.ok(await h.until(`/5120/.test(document.getElementById('diagRecStatus').textContent)`),
      'the refusal reason is shown: ' + (await h.evalJs(STATE)).status);
    const s = await h.evalJs(STATE);
    assert.match(s.status, /refused/i);
    assert.equal(s.start, true, 'Start stays available after a refusal');
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
