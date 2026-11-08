// TASK-1715: a stalled Chrome makes a harness call fail fast with a clear message instead of
// hanging the test until the node test timeout.
// The stall is real: the page runs `for(;;){}`, so the renderer never answers another
// Runtime.evaluate (as a headless Chrome that stops responding under load).
// - evalJs() rejects within the CDP timeout and names the CDP method;
// - until() returns within its wall-time budget plus at most one CDP timeout.
// On the old harness both calls hang; the test's own guard then reports "hung".
//
// Run: node --test tests/webui/harness-stall.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';

process.env.OTGW_CDP_TIMEOUT_MS = process.env.OTGW_CDP_TIMEOUT_MS || '3000';
const { startPage, CHROME } = await import('./lib/page-harness.mjs');
const skip = !CHROME && 'no Chrome';
const GUARD_MS = 30000;

// resolves to { ok, value | error, ms } or { hung: true } after GUARD_MS
function guarded(p) {
  const t0 = Date.now();
  return Promise.race([
    p.then((value) => ({ ok: true, value, ms: Date.now() - t0 }), (error) => ({ ok: false, error, ms: Date.now() - t0 })),
    new Promise((r) => setTimeout(() => r({ hung: true, ms: Date.now() - t0 }), GUARD_MS).unref()),
  ]);
}

test('a stalled renderer makes evalJs and until fail fast', { skip }, async () => {
  const h = await startPage({});
  try {
    assert.equal(await h.evalJs('1 + 1'), 2, 'the page answers before the stall');
    h.evalJs('for(;;){}').catch(() => {});   // the stall: this call never gets a reply

    const e = await guarded(h.evalJs('document.title'));
    assert.ok(!e.hung, `evalJs still hangs after ${e.ms} ms on a stalled renderer`);
    assert.equal(e.ok, false, 'evalJs on a stalled renderer must reject, not resolve');
    assert.match(String(e.error && e.error.message), /Runtime\.evaluate/, 'the error names the CDP method');
    assert.ok(e.ms < 3000 + 2000, `evalJs took ${e.ms} ms, expected about the 3000 ms CDP timeout`);

    const u = await guarded(h.until('true', 1000));
    assert.ok(!u.hung, `until() still hangs after ${u.ms} ms on a stalled renderer`);
    assert.ok(u.ms < 1000 + 3000 + 2000, `until(…, 1000) took ${u.ms} ms; it must stop near its own budget`);
  } finally { await h.close(); }
});
