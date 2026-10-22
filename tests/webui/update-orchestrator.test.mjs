// TASK-1283 guard: the one-action WiFi update on the /update page.
//
// The page logic (file recognition, zip reader, phase engine) is the first <script>
// of src/OTGW-firmware/updateServerHtml.h (id "updLogic"). This test cuts that script
// out of the header and runs the REAL text under node; nothing is re-implemented.
// Node's Blob, Response and DecompressionStream('deflate-raw') are the same API the
// browser uses for the zip.
//
// Three groups:
//  1. file recognition on the real built artifacts in build/ (skipped when absent)
//  2. the phase engine against a fake unit with a virtual clock (the request order,
//     the 35 s settle, one request at a time, rollback, dropped connections, resume)
//  3. mutants: the same scenarios run against deliberately broken copies of the
//     logic and must FAIL, which shows each check can go red.
//
// Not covered here: a real browser (XHR progress, auth dialogs, Safari), real
// timing on a device, the DOM wiring of the second <script> (syntax-checked only).
// The fake unit's auth model is NOT the browser's: it answers an unauthenticated
// /info with a plain 401 once, while a browser shows a login dialog and keeps the
// XHR pending (or aborts it on timeout). passwordPrompt therefore only pins the
// poll timeouts, not the dialog behaviour.
//
// Run: node --test tests/webui/update-orchestrator.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const HEADER = process.env.OTGW_UPDATE_H || path.join(ROOT, 'src', 'OTGW-firmware', 'updateServerHtml.h');
const BUILD = process.env.OTGW_BUILD_DIR || path.join(ROOT, 'build');

const header = fs.readFileSync(HEADER, 'utf8');

function scriptByMarker(src, open) {
  const a = src.indexOf(open);
  if (a < 0) throw new Error('marker not found: ' + open);
  const s = a + open.length;
  return src.slice(s, src.indexOf('</script>', s));
}
const LOGIC = scriptByMarker(header, "<script id='updLogic'>");

// Load a copy of the logic (optionally mutated) into its own function scope.
function load(text = LOGIC) {
  return new Function(text + '\nreturn OTGWUpd;')();
}
const U = load();

// ---------- page structure ----------
test('the raw string has one terminator and the page script parses', () => {
  assert.equal(header.split(')UPD"').length - 1, 1, 'the raw-string delimiter must occur once');
  const page = header.slice(header.indexOf("<script id='updLogic'>") + 1);
  const second = page.indexOf('<script>', page.indexOf('</script>'));
  const src = page.slice(second + '<script>'.length, page.indexOf('</script>', second));
  assert.doesNotThrow(() => new vm.Script(src), 'the page script has a syntax error');
  assert.doesNotThrow(() => new vm.Script(LOGIC));
});

test('the default "download backups" checkbox is gone from the update flow', () => {
  assert.ok(!/chkPreserve|doBackups|downloadBackup/.test(header));
  assert.ok(/id='bkLink'[^>]*href='\/settings\.ini'[^>]*download/.test(header), 'separate settings backup link');
});

test('the header comment names a test file that exists', () => {
  const m = /(tests\/webui\/[\w.-]+\.mjs)/.exec(header);
  assert.ok(m, 'no test file named in the header comment');
  assert.ok(fs.existsSync(path.join(ROOT, m[1])), m[1] + ' does not exist');
});

test('the logic script does not touch the DOM', () => {
  assert.ok(!/\bdocument\b|\bwindow\b|\blocalStorage\b/.test(LOGIC));
});

// ---------- 1. recognition on the real artifacts ----------
function artifacts() {
  if (!fs.existsSync(BUILD)) return null;
  const names = fs.readdirSync(BUILD);
  const zip = names.filter((n) => /^OTGW-firmware-esp32-[a-z0-9]+-.+-flash\.zip$/.test(n)).sort().pop();
  if (!zip) return null;
  const stem = zip.replace(/-flash\.zip$/, '');
  const f = (suffix) => {
    const p = path.join(BUILD, stem + suffix);
    return fs.existsSync(p) ? p : null;
  };
  const sb = names.filter((n) => /^OTGW-safeboot-.*\.bin$/.test(n)).sort().pop();
  return {
    stem, zip: path.join(BUILD, zip), app: f('.ino.bin'), fsimg: f('.littlefs.bin'),
    mergedFull: f('-merged-full.bin'), merged: f('-merged.bin'), mig0: f('-migrate-0x0.bin'),
    migRest: f('-migrate-rest.bin'), safeboot: sb ? path.join(BUILD, sb) : null
  };
}
const A = artifacts();
const skip = A && A.app && A.fsimg ? false : 'no built artifacts in build/ (run build.bat first)';
const rd = (f) => Promise.resolve(new Uint8Array(fs.readFileSync(f.path)));
const file = (p) => ({ name: path.basename(p), path: p });
const bytes = (p) => new Uint8Array(fs.readFileSync(p));

test('classify: the real .ino.bin is an app with its version, the real .littlefs.bin is a filesystem', { skip }, () => {
  const a = U.classify(bytes(A.app));
  assert.equal(a.kind, 'app');
  const ver = /(\d+\.\d+\.\d+(?:-[a-z]+\.\d+)?\+[0-9a-f]{7})/.exec(A.stem)[1];
  assert.ok(a.vers.includes(ver), 'version from the file name is found in the image');
  assert.equal(U.classify(bytes(A.fsimg)).kind, 'fs');
});

// Every main image carries this page, and so every marker literal the page contains. A main image
// must never be taken for the updater (bench build A and the alpha.449 build were: HIGH finding).
// pageSrc is the page text a rebuilt main image would carry; a mutant passes its own
// mutated text, so the check does not depend on which build happens to be in build/.
async function appWithPage(L, quiet, pageSrc = header) {
  const cands = [A && A.app, process.env.OTGW_PAGE_APP_BIN].filter((x) => x && fs.existsSync(x));
  const pageText = Buffer.from(pageSrc, 'latin1');
  const out = [];
  for (const c of cands) {
    out.push(L.classify(bytes(c)).kind === 'app');
    // the same image with the page's complete text appended, as a rebuilt main image carries it
    const withPage = new Uint8Array(Buffer.concat([fs.readFileSync(c), pageText]));
    out.push(L.classify(withPage).kind === 'app');
  }
  if (quiet) return out.length > 0 && out.every(Boolean);
  assert.ok(out.length > 0 && out.every(Boolean), 'a main image was not classified as app: ' + JSON.stringify(out));
}
test('classify: a main image that carries this page (the old literal too) is an app, not the updater', { skip }, async () => {
  await appWithPage(U);
  const s = Buffer.from(fs.readFileSync(A.safeboot)).toString('latin1');
  assert.ok(s.includes('[safe' + 'boot] app valid'), 'the marker is really in the updater image');
  assert.ok(!header.includes('[safeboot] start') && !header.includes('[safeboot] app valid'), 'the page text carries no marker literal');
});

test('classify: USB images, the updater itself and a zip are refused with the right kind', { skip }, () => {
  for (const [k, p] of [['usb', A.mergedFull], ['usb', A.merged], ['usb', A.mig0], ['usb', A.migRest], ['safeboot', A.safeboot], ['zip', A.zip]]) {
    if (!p) continue;
    assert.equal(U.classify(bytes(p)).kind, k, path.basename(p));
  }
});

test('classify: a filesystem image of the wrong size, and an app with a bad magic, are not accepted', { skip }, () => {
  const f = bytes(A.fsimg);
  assert.equal(U.classify(f.subarray(0, f.length - 4096)).kind, 'fssize');
  const a = bytes(A.app).slice();
  a[0x20] ^= 0xFF;
  assert.notEqual(U.classify(a).kind, 'app');
  const e = bytes(A.app).slice();
  e[0] = 0x00;
  assert.notEqual(U.classify(e).kind, 'app');
  const big = new Uint8Array(0x230000 + 1);
  big.set(bytes(A.app).subarray(0, 64));
  assert.notEqual(U.classify(big).kind, 'app');
});

test('intake: the real release zip yields byte-identical app and filesystem, CRC checked', { skip }, async () => {
  const r = await U.intake([file(A.zip)], rd);
  assert.deepEqual(Buffer.from(r.app.u8), fs.readFileSync(A.app));
  assert.deepEqual(Buffer.from(r.fs.u8), fs.readFileSync(A.fsimg));
  assert.equal(r.G, r.E.split('+')[1]);
  assert.ok(A.stem.includes(r.E));
  assert.equal(r.slug, /^OTGW-firmware-(esp32-[a-z0-9]+)-/.exec(A.stem)[1]);
  assert.deepEqual(r.warns, []);
});

test('intake: two loose .bin files work in either order', { skip }, async () => {
  const x = await U.intake([file(A.app), file(A.fsimg)], rd);
  const y = await U.intake([file(A.fsimg), file(A.app)], rd);
  assert.equal(x.E, y.E);
  assert.equal(x.app.name, path.basename(A.app));
  assert.equal(y.fs.name, path.basename(A.fsimg));
});

test('intake: wrong picks are refused with a clear message and name the file', { skip }, async () => {
  const msg = async (files) => { try { await U.intake(files.map(file), rd); } catch (e) { return e.message; } return null; };
  assert.match(await msg([A.mergedFull, A.fsimg]), /USB image/);
  assert.match(await msg([A.app, A.migRest]), /USB image/);
  assert.match(await msg([A.safeboot, A.fsimg]), /updater itself/);
  assert.match(await msg([A.app, A.app]), /one firmware file/);
  assert.match(await msg([A.fsimg]), /release \.zip, or both/);
  assert.match(await msg([A.app]), /release \.zip, or both/);
  assert.match(await msg([A.zip, A.fsimg]), /zip file/);
  assert.match(await msg([A.app, A.fsimg, A.zip]), /release \.zip, or both/);
});

test('intake: a pair from different builds is accepted with a warning (githash check)', { skip }, async () => {
  const f = bytes(A.fsimg).slice();
  const g = Buffer.from(f).toString('latin1');
  const hash = /\+([0-9a-f]{7})/.exec(A.stem)[1];
  const at = g.indexOf(hash);
  assert.ok(at > 0, 'the build hash is inside the filesystem image');
  f.set(Buffer.from('0000000'), at);
  const tmp = path.join(fs.mkdtempSync(path.join(process.env.TEMP || '/tmp', 'upd-')), path.basename(A.fsimg));
  fs.writeFileSync(tmp, f);
  const r = await U.intake([file(A.app), file(tmp)], rd);
  assert.equal(r.warns.length, 1);
  assert.match(r.warns[0], /different builds/);
});

// A small zip writer for the container edge cases (stored or deflated entries).
async function zipWith(entries, tweak = {}) {
  const parts = [], central = [];
  let off = 0;
  for (const [name, data, method] of entries) {
    const raw = Buffer.from(data);
    let comp = raw;
    if (method === 8) {
      const cs = new Blob([raw]).stream().pipeThrough(new CompressionStream('deflate-raw'));
      comp = Buffer.from(await new Response(cs).arrayBuffer());
    }
    const nb = Buffer.from(name);
    const crc = tweak.crc !== undefined ? tweak.crc : U.crc32(new Uint8Array(raw));
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0); lh.writeUInt16LE(20, 4); lh.writeUInt16LE(tweak.flags || 0, 6); lh.writeUInt16LE(tweak.method !== undefined ? tweak.method : method, 8);
    lh.writeUInt32LE(crc, 14); lh.writeUInt32LE(comp.length, 18); lh.writeUInt32LE(raw.length, 22); lh.writeUInt16LE(nb.length, 26);
    parts.push(lh, nb, comp);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0); ch.writeUInt16LE(20, 4); ch.writeUInt16LE(20, 6); ch.writeUInt16LE(tweak.flags || 0, 8); ch.writeUInt16LE(tweak.method !== undefined ? tweak.method : method, 10);
    ch.writeUInt32LE(crc, 16); ch.writeUInt32LE(comp.length, 20); ch.writeUInt32LE(raw.length, 24); ch.writeUInt16LE(nb.length, 28); ch.writeUInt32LE(off, 42);
    central.push(ch, nb);
    off += lh.length + nb.length + comp.length;
  }
  const cd = Buffer.concat(central);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0); eocd.writeUInt16LE(entries.length, 8); eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(cd.length, 12); eocd.writeUInt32LE(off, 16);
  return new Uint8Array(Buffer.concat([...parts, cd, eocd]));
}

test('zip reader: stored and deflated entries, CRC and size are enforced, odd zips are refused', async () => {
  const body = Buffer.from('hello hello hello hello hello hello'.repeat(50));
  for (const method of [0, 8]) {
    const z = await zipWith([['sub/x.bin', body, method]]);
    const es = U.parseZip(z);
    assert.equal(es.length, 1);
    assert.deepEqual(Buffer.from(await U.entryBytes(z, es[0])), body);
  }
  const bad = await zipWith([['x.bin', body, 8]], { crc: 0x12345678 });
  await assert.rejects(U.entryBytes(bad, U.parseZip(bad)[0]), /checksum/);
  const enc = await zipWith([['x.bin', body, 0]], { flags: 1 });
  await assert.rejects(U.entryBytes(enc, U.parseZip(enc)[0]), /Encrypted/);
  const meth = await zipWith([['x.bin', body, 0]], { method: 12 });
  await assert.rejects(U.entryBytes(meth, U.parseZip(meth)[0]), /compression method/);
  assert.throws(() => U.parseZip(new Uint8Array(100)), /not a valid zip/);
  const z = await zipWith([['x.bin', body, 8]]);
  z[40] ^= 0xFF; // corrupt the deflate stream
  await assert.rejects(U.entryBytes(z, U.parseZip(z)[0]));
});

test('zip reader: without DecompressionStream the user is told to pick the two .bin files', async () => {
  const z = await zipWith([['x.bin', Buffer.from('abcabcabc'.repeat(40)), 8]]);
  const saved = globalThis.DecompressionStream;
  try {
    delete globalThis.DecompressionStream;
    await assert.rejects(U.entryBytes(z, U.parseZip(z)[0]), /Unzip the file and choose the two \.bin files/);
  } finally {
    globalThis.DecompressionStream = saved;
  }
});

test('zip reader: an entry that inflates past its declared size is cut off, not fully read', async () => {
  const big = Buffer.alloc(6 * 1024 * 1024);                 // 6 MB of zeros deflates to a few KB
  const z = await zipWith([['x.bin', big, 8]]);
  const e = U.parseZip(z)[0];
  e.usize = 1000;                                            // the zip claims 1000 bytes
  await assert.rejects(U.entryBytes(z, e), /larger than declared/);
  const e2 = U.parseZip(z)[0];
  e2.usize = 0x230000 + 1;
  await assert.rejects(U.entryBytes(z, e2), /larger than any firmware file/);
});

test('zip reader: DecompressionStream without deflate-raw gives the unzip advice, not a TypeError', async () => {
  const z = await zipWith([['x.bin', Buffer.from('abcabcabc'.repeat(40)), 8]]);
  const saved = globalThis.DecompressionStream;
  try {
    globalThis.DecompressionStream = class { constructor(fmt) { throw new TypeError("Unsupported compression format: '" + fmt + "'"); } };
    await assert.rejects(U.entryBytes(z, U.parseZip(z)[0]), /Unzip the file and choose the two \.bin files/);
  } finally {
    globalThis.DecompressionStream = saved;
  }
});

test('zip reader: only the matching OTGW-firmware pair is picked, other entries are ignored', () => {
  const e = (name) => ({ name });
  const ok = U.pickZip([e('flash_otgw.bat'), e('OTGW-firmware-esp32-combo-2.0.0-alpha.448+7eb246d.ino.bin'), e('README.txt'), e('OTGW-firmware-esp32-combo-2.0.0-alpha.448+7eb246d.littlefs.bin'), e('OTGW-firmware-esp32-combo-2.0.0-alpha.448+7eb246d-merged.bin')]);
  assert.equal(ok.app.slug, 'esp32-combo');
  assert.throws(() => U.pickZip([e('OTGW-firmware-esp32-combo-1.0.0+aaaaaaa.ino.bin')]), /matching/);
  assert.throws(() => U.pickZip([e('OTGW-firmware-esp32-combo-1.0.0+aaaaaaa.ino.bin'), e('OTGW-firmware-esp32-combo-2.0.0+bbbbbbb.littlefs.bin')]), /matching/);
});

test('board and address helpers', () => {
  assert.ok(U.hwOk('esp32-combo', 'otgw32') && U.hwOk('esp32-combo', 'otgw-classic'));
  assert.ok(U.hwOk('esp32-classic', 'otgw-classic') && !U.hwOk('esp32-classic', 'otgw32'));
  assert.ok(U.hwOk('esp32-otgw32', 'otgw32') && !U.hwOk('esp32-otgw32', 'otgw-classic'));
  assert.ok(U.hwOk('', 'otgw32'));
  assert.ok(U.isIPv4('192.168.1.5') && !U.isIPv4('otgw.local') && !U.isIPv4('192.168.1'));
});

// ---------- 2. the phase engine against a fake unit ----------
const E = '2.0.0-alpha.448+7eb246d', OLD = '2.0.0-alpha.447+aaaaaaa', G = '7eb246d', OLDG = 'aaaaaaa';
const html302 = '<html>updater page</html>';

function makeUnit(o = {}) {
  const d = {
    clock: 0, mode: 'old', ver: o.startVer || OLD, fsHash: o.startFs || OLDG, log: [], timeline: [], inflight: 0, maxInflight: 0,
    firstNewAnswer: null, fsUploadAt: null, firstVerifyAt: null, flashed: false, fsWritten: false, fsUploads: 0, flaky: 0, infoTimeouts: [], opts: o
  };
  const at = (ms, fn) => d.timeline.push({ at: d.clock + ms, fn });
  const advance = () => {
    d.timeline.sort((a, b) => a.at - b.at);
    while (d.timeline.length && d.timeline[0].at <= d.clock) d.timeline.shift().fn();
  };
  const json = (x) => ({ status: 200, text: JSON.stringify(x) });
  function handle(method, url, opt) {
    const u = url.split('?')[0], q = url.split('?')[1] || '';
    d.log.push({ t: d.clock, m: method, u, q, mode: d.mode, field: opt.form && opt.form.field });
    if (d.mode === 'down') throw { kind: opt.timeout < 5000 ? 'timeout' : 'net', sentAll: false };
    if (d.mode === 'old' || d.mode === 'new') {
      if (method === 'POST' && u === '/safeboot') {
        if (o.handover === 409) return { status: 409, text: 'No safeboot updater on this board' };
        if (o.handover === 401) return { status: 401, text: '' };
        at(3000, () => { d.mode = 'down'; });
        at(8000, () => { d.mode = 'safeboot'; });
        return { status: 200, text: 'Restarting into the firmware updater...' };
      }
      if (u === '/api/v2/device/info') {
        if (o.infoFlaky && d.firstVerifyAt !== null && d.flaky < 2) { d.flaky++; throw { kind: 'net', sentAll: false }; }
        if (d.mode === 'new' && d.firstNewAnswer === null) d.firstNewAnswer = d.clock;
        return json({ device: { fwversion: d.ver, hostname: 'OTGW', ipaddress: '192.168.1.5', networkmode: 'WiFi', board: 'x', hardware_type: 'otgw32', safeboot_available: true, app_ota_available: false } });
      }
      if (u === '/api/v2/filesystem/hash-check') return json({ filesystem_check: { match: d.fsHash === G && d.ver === E, fw_hash: d.ver.split('+')[1], fs_hash: d.fsHash } });
      if (u === '/api/v2/health') {
        if (d.fsUploadAt !== null && d.firstVerifyAt === null) d.firstVerifyAt = d.clock;
        return json({ health: { status: o.degraded && d.fsUploads < (o.degradedUntil || 99) && d.fsWritten ? 'DEGRADED' : 'UP' } });
      }
      if (method === 'POST' && u === '/update' && q.indexOf('cmd=100') >= 0) {
        d.fsUploadAt = d.clock;
        assert.equal(opt.form.field, 'filesystem');
        assert.ok(d.mode === 'new', 'filesystem goes to the main firmware');
        assert.match(q, /size=786432/);
        if (o.fsError && !d.fsErrorServed) { d.fsErrorServed = true; return { status: 200, text: 'Flash error: incomplete filesystem image (version.hash missing)' }; }
        d.fsWritten = true; d.fsUploads++;
        const bad = o.badWrite && d.fsUploads === 1;   // the write silently did not take
        at(o.slowReboot ? 12000 : 3000, () => { d.mode = 'down'; });
        at(o.slowReboot ? 30000 : 20000, () => { d.mode = 'new'; if (!bad) d.fsHash = G; });
        if (o.fsDrop) throw { kind: 'net', sentAll: true };
        return { status: 200, text: '<html>Flashing successful!</html>' };
      }
      return { status: 404, text: 'FileNotFound\r\n' };
    }
    // safeboot
    if (u === '/info') {
      if (o.password && !d.authed) { d.authed = true; return { status: 401, text: '' }; }
      return json({ safeboot: '1.0', hostname: 'OTGW', ip: '192.168.1.5', ap_mode: false, app_valid: !d.rolled, app_rolled_back: !!d.rolled, auth: false });
    }
    if (method === 'POST' && u === '/update') {
      assert.equal(opt.form.field, 'firmware');
      assert.equal(q, '', 'safeboot gets no cmd/size parameters');
      if (o.fwDropEarly && !d.fwDropServed) { d.fwDropServed = true; throw { kind: 'net', sentAll: false }; }
      if (o.fwDropIncomplete && !d.incServed) { d.incServed = true; throw { kind: 'net', sentAll: true }; }   // everything sent, not everything received
      if (o.fwRefuse) return { status: 500, text: 'Update failed: Not enough space. The unit stays in safeboot.' };
      d.flashed = true; d.rolled = false;
      at(500, () => { d.mode = 'down'; });
      at(15000, () => {
        if (o.crash) { d.mode = 'safeboot'; d.rolled = true; }
        else { d.mode = 'new'; d.ver = o.newVer || E; }
      });
      if (o.crash) at(15000 + 1, () => {});
      if (o.fwDrop) throw { kind: 'net', sentAll: true };
      return { status: 200, text: 'Update OK. Restarting into the new firmware...' };
    }
    // safeboot answers every other path with a 302 to its HTML page: XHR sees 200 + HTML
    return { status: 200, text: html302 };
  }
  d.io = {
    now: () => d.clock,
    sleep: (ms) => { d.clock += ms; advance(); return Promise.resolve(); },
    req: async (method, url, opt = {}) => {
      d.inflight++; d.maxInflight = Math.max(d.maxInflight, d.inflight);
      d.clock += 40; advance();
      await Promise.resolve(); await Promise.resolve();
      try {
        const r = handle(method, url, opt);
        if (opt.onProgress && opt.form) opt.onProgress(opt.form.blob.size, opt.form.blob.size);
        return r;
      } finally { d.inflight--; }
    }
  };
  d.ui = { steps: [], says: [], prog: 0, step(s) { this.steps.push(s); }, say(t) { this.says.push(t); }, progress() { this.prog++; } };
  return d;
}
const ctx = (extra = {}) => Object.assign({
  app: { blob: { size: 2023168 }, name: 'app.ino.bin' }, fs: { blob: { size: 786432 }, name: 'fs.littlefs.bin', size: 786432 },
  E, G, host: 'OTGW', ip: '192.168.1.5'
}, extra);

async function happyPath(L) {
  const d = makeUnit();
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.deepEqual(res, { ok: true });
  assert.equal(d.maxInflight, 1, 'one request at a time');
  assert.equal(d.ver, E);
  assert.equal(d.fsHash, G);
  assert.deepEqual(d.ui.steps.filter((s, i, a) => a.indexOf(s) === i), ['handover', 'upload', 'boot', 'settle', 'fs', 'verify']);
  // request order: handover, safeboot poll, firmware upload, main poll, (silence while settling), filesystem upload, verify
  const seq = d.log.filter((r) => r.m === 'POST').map((r) => r.u + (r.q ? '?' + r.q : ''));
  assert.deepEqual(seq, ['/safeboot', '/update', '/update?cmd=100&size=786432']);
  assert.equal(d.log.find((r) => r.m === 'POST' && r.u === '/update' && !r.q).mode, 'safeboot');
  assert.ok(d.fsUploadAt - d.firstNewAnswer >= 35000, 'filesystem step waits >= 35 s after the new firmware answered: ' + (d.fsUploadAt - d.firstNewAnswer));
  const settling = d.log.filter((r) => r.t > d.firstNewAnswer + 100 && r.t < d.firstNewAnswer + 34900);
  assert.equal(settling.length, 0, 'no traffic at all while the new firmware settles');
  // verification never reads the pre-reboot instance: the first health poll is >= 10 s after the upload
  assert.ok(d.firstVerifyAt - d.fsUploadAt >= 10000, 'verify waits for the restart: ' + (d.firstVerifyAt - d.fsUploadAt));
  assert.ok(d.log.some((r) => r.u === '/info' && r.mode === 'old'), 'the 404 from the old firmware was seen and ignored');
  assert.ok(d.log.some((r) => r.u === '/api/v2/device/info' && r.mode === 'safeboot'), 'the updater page (HTML 200) answered a device/info poll and was ignored');
}

async function dropAfterFullUpload(L) {
  const d = makeUnit({ fwDrop: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, true, JSON.stringify(res));
  assert.equal(d.ver, E);
}
async function dropBeforeFullUpload(L) {
  const d = makeUnit({ fwDropEarly: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, false);
  assert.equal(res.code, 'fwnet');
  assert.equal(res.stage, 'upload');
  const again = await L.run(d.io, d.ui, ctx(), res.stage);
  assert.equal(again.ok, true, JSON.stringify(again));
  assert.equal(d.ver, E);
}
async function rollback(L) {
  const d = makeUnit({ crash: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, false);
  assert.equal(res.code, 'rollback');
  assert.equal(d.fsUploadAt, null, 'the filesystem is not touched after a rollback');
  assert.match(res.rec, /upload a working \.ino\.bin/);
}
async function wrongVersion(L) {
  const d = makeUnit({ newVer: '2.0.0-alpha.449+bbbbbbb' });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.code, 'wrongver');
  assert.equal(d.fsUploadAt, null);
}
async function fsFlashError(L) {
  const d = makeUnit({ fsError: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, false);
  assert.equal(res.code, 'fserr');
  assert.equal(res.stage, 'fs');
  assert.match(res.msg, /version\.hash missing/);
  assert.match(res.rec, /do not switch the unit off/);
  const again = await L.run(d.io, d.ui, ctx(), res.stage);
  assert.equal(again.ok, true, JSON.stringify(again));
  assert.equal(d.fsHash, G);
}
async function fsDropAfterUpload(L) {
  const d = makeUnit({ fsDrop: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, true, JSON.stringify(res));
}
async function resumeFsOnly(L) {
  const d = makeUnit({ startVer: E, startFs: OLDG });
  d.mode = 'new';
  const res = await L.run(d.io, d.ui, ctx(), 'fs');
  assert.equal(res.ok, true, JSON.stringify(res));
  assert.ok(!d.log.some((r) => r.u === '/safeboot'), 'no handover on resume');
  assert.equal(d.log.filter((r) => r.m === 'POST').length, 1);
  assert.equal(d.fsHash, G);
}
async function handoverRefused(L) {
  let d = makeUnit({ handover: 409 });
  let res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.code, 'nosb');
  d = makeUnit({ handover: 401 });
  res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.code, 'auth');
}
async function updaterNeverAppears(L) {
  const d = makeUnit();
  const res = await L.run(d.io, d.ui, ctx({ t: { sb: 20000 } }), 'handover');
  // the unit does reach safeboot at +8 s in the model, so force an unreachable updater instead
  const d2 = makeUnit();
  d2.io.req = async (m, u) => { d2.log.push({ m, u }); if (m === 'POST' && u === '/safeboot') return { status: 200, text: 'ok' }; return { status: 404, text: 'FileNotFound' }; };
  const r2 = await L.run(d2.io, d2.ui, ctx({ t: { sb: 20000 } }), 'handover');
  assert.equal(r2.code, 'sbtimeout');
  assert.match(r2.rec, /OTGW-safeboot/);
  assert.match(r2.rec, /192\.168\.4\.1/);
  assert.equal(res.ok, true);
}
async function healthDegraded(L) {
  const d = makeUnit({ degraded: true, degradedUntil: 2 });
  const res = await L.run(d.io, d.ui, ctx({ t: { verify: 30000 } }), 'handover');
  assert.equal(res.code, 'degraded');
  assert.equal(res.stage, 'fs', 'Try again must go back to the filesystem upload, not only re-poll');
  const again = await L.run(d.io, d.ui, ctx({ t: { verify: 30000 } }), res.stage);
  assert.equal(again.ok, true, JSON.stringify(again));
  assert.equal(d.log.filter((r) => r.m === 'POST' && r.q.indexOf('cmd=100') >= 0).length, 2, 'the web files were sent again');
}
// The filesystem identity is only the 7-character githash: a unit whose filesystem already
// reports this hash (a build from the same commit, or a write that did not take) still gets the files.
async function sameHashStillUploaded(L) {
  let d = makeUnit({ startFs: G });
  let res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, true, JSON.stringify(res));
  assert.equal(d.log.filter((r) => r.m === 'POST' && r.q.indexOf('cmd=100') >= 0).length, 1, 'full run: the filesystem upload is not skipped');
  d = makeUnit({ startVer: E, startFs: G });
  d.mode = 'new';
  res = await L.run(d.io, d.ui, ctx(), 'fs');
  assert.equal(res.ok, true, JSON.stringify(res));
  assert.equal(d.log.filter((r) => r.m === 'POST' && r.q.indexOf('cmd=100') >= 0).length, 1, 'resume: the filesystem upload is not skipped');
}
async function silentFailedWrite(L) {
  const d = makeUnit({ badWrite: true });
  const res = await L.run(d.io, d.ui, ctx({ t: { verify: 30000 } }), 'handover');
  assert.equal(res.ok, false);
  assert.equal(res.code, 'fsmismatch');
  assert.equal(res.stage, 'fs');
  const again = await L.run(d.io, d.ui, ctx({ t: { verify: 30000 } }), res.stage);
  assert.equal(again.ok, true, JSON.stringify(again));
  assert.equal(d.fsHash, G);
}
async function incompleteFirmwareUpload(L) {
  const d = makeUnit({ fwDropIncomplete: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, false);
  assert.equal(res.code, 'fwincomplete', JSON.stringify(res));
  assert.equal(res.stage, 'upload', 'Try again must send the firmware again');
  assert.equal(d.fsUploadAt, null);
  const again = await L.run(d.io, d.ui, ctx(), res.stage);
  assert.equal(again.ok, true, JSON.stringify(again));
  assert.equal(d.log.filter((r) => r.m === 'POST' && r.u === '/update' && !r.q).length, 2, 'the firmware went to the updater twice');
}
async function finalReadFlaky(L) {
  const d = makeUnit({ infoFlaky: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, true, 'two failed version reads after a good filesystem check must not fail the update: ' + JSON.stringify(res));
}
async function staleSnapshotPlans(L) {
  const r = { E, G, slug: 'esp32-combo', warns: [], app: { u8: { length: 2023168 } }, fs: { u8: { length: 786432 } } };
  const old = { ok: true, ver: OLD, fsMatch: true, fsHash: OLDG, sb: true, appOta: false, hw: 'otgw32' };
  const fresh = { ok: true, ver: E, fsMatch: false, fsHash: OLDG, sb: true, appOta: false, hw: 'otgw32' };
  assert.equal(L.planFor(old, r, '192.168.1.5').from, 'handover');
  assert.equal(L.planFor(fresh, r, '192.168.1.5').from, 'fs', 'a fresh snapshot of a unit that already has the firmware plans only the web files');
  const same = L.planFor({ ok: true, ver: E, fsMatch: true, fsHash: G, sb: true, appOta: false, hw: 'otgw32' }, r, '192.168.1.5');
  assert.equal(same.from, 'fs');
  assert.equal(same.start, true, 'the user may send the web files again');
  assert.match(same.info, /again anyway/);
  assert.equal(L.planFor(fresh, r, 'otgw.local').start, false);
  assert.equal(L.planFor({ ok: false }, r, '192.168.1.5').start, false);
  assert.match(L.planFor(old, Object.assign({}, r, { slug: 'esp32-classic' }), '192.168.1.5').warns.join(' '), /another board type/);
}
async function passwordPrompt(L) {
  const d = makeUnit({ password: true });
  d.io.req = ((orig) => async (m, u, o) => { const r = await orig(m, u, o); if (u.split('?')[0] === '/info') d.infoTimeouts.push([r.status, o.timeout]); return r; })(d.io.req);
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, true, JSON.stringify(res));
  const i = d.infoTimeouts.findIndex((x) => x[0] === 401);
  assert.ok(i >= 0 && d.infoTimeouts[i + 1][1] >= 30000, 'after a 401 the next poll waits long enough for the login dialog');
  assert.ok(d.infoTimeouts.slice(0, i + 2).every((x) => x[1] >= 15000), 'every updater poll waits long enough for a login dialog that is still open: ' + JSON.stringify(d.infoTimeouts));
}
async function slowRestartStaleHash(L) {
  const d = makeUnit({ slowReboot: true });
  const res = await L.run(d.io, d.ui, ctx(), 'handover');
  assert.equal(res.ok, true, JSON.stringify(res));
  assert.ok(d.log.some((r) => r.u === '/api/v2/filesystem/hash-check' && r.t > d.fsUploadAt && r.t < d.fsUploadAt + 12000), 'the pre-restart instance did answer a hash-check with the old hash');
  assert.equal(d.fsHash, G);
}
async function snapshotReads(L) {
  const d = makeUnit();
  const s = await L.snapshot(d.io);
  assert.equal(s.ok, true);
  assert.equal(s.ver, OLD); assert.equal(s.sb, true); assert.equal(s.appOta, false); assert.equal(s.hw, 'otgw32'); assert.equal(s.fsMatch, false);
  const dead = { now: () => 0, sleep: () => Promise.resolve(), req: () => Promise.reject({ kind: 'net' }) };
  assert.equal((await L.snapshot(dead)).ok, false);
}

const SCENARIOS = { happyPath, dropAfterFullUpload, dropBeforeFullUpload, rollback, wrongVersion, fsFlashError, fsDropAfterUpload, resumeFsOnly, handoverRefused, updaterNeverAppears, healthDegraded, sameHashStillUploaded, silentFailedWrite, incompleteFirmwareUpload, finalReadFlaky, staleSnapshotPlans, passwordPrompt, slowRestartStaleHash, snapshotReads };
for (const [name, fn] of Object.entries(SCENARIOS)) test('engine: ' + name, () => fn(U));

// ---------- 3. mutants: broken copies of the logic must turn a scenario red ----------
function mutate(from, to) {
  const hit = from instanceof RegExp ? from.test(LOGIC) : LOGIC.includes(from);
  assert.ok(hit, 'mutation anchor not found: ' + from);
  return load(LOGIC.replace(from, to));
}
async function fails(fn, L) {
  try { await fn(L); } catch { return true; }
  return false;
}
const MUTANTS = [
  ['a dropped connection after 100% is treated as a failure', /if \(e && e\.sentAll\) return;\s*throw F\('fwnet'/, "throw F('fwnet'", 'dropAfterFullUpload'],
  ['the 35 s settle wait is removed', 'tSettle = t.settle === undefined ? 35000 : t.settle', 'tSettle = 0', 'happyPath'],
  ['rollback is not detected', 's.j.app_rolled_back === true', 'false', 'rollback'],
  ['an HTML 200 from the updater counts as the new firmware (res.ok trap)', 'var v = fwOf(r.j);', 'var v = r.status === 200 ? c.E : fwOf(r.j);', 'happyPath'],
  ['the filesystem Flash error body is ignored', "if (r.text.indexOf('Flash error') >= 0)", 'if (false)', 'fsFlashError'],
  ['verify does not wait for the restart', "t.v0 === undefined ? 10000 : t.v0", '0', 'happyPath'],
  ['the version of the new firmware is not compared', "return { state: v === c.E ? 'up' : 'wrong', ver: v };", "return { state: 'up', ver: v };", 'wrongVersion'],
  ['a 401 does not lengthen the next poll', 'to = 60000;', 'to = 15000;', 'passwordPrompt'],
  ['the updater poll times out before a login dialog can be answered', 'var end = io.now() + ms, to = 15000;', 'var end = io.now() + ms, to = 4000;', 'passwordPrompt'],
  ['a stale hash-check right after the upload fails the update at once', "if (io.now() > end) return stale ? 'mismatch'", "if (stale || io.now() > end) return stale ? 'mismatch'", 'slowRestartStaleHash'],
  ['the safeboot image is no longer recognised as the updater', "if (s.indexOf('[safe' + 'boot] app valid') >= 0) return { kind: 'safeboot' };", '', 'artifact:safeboot'],
  ['the marker is a literal that this page carries itself', "'[safe' + 'boot] app valid'", "'[safeboot] start'", 'artifact:pageApp'],
  ['the filesystem upload is skipped when the unit reports the hash', "ui.say('Sending the web files...');", "if (c.G === '7eb246d') return;", 'sameHashStillUploaded'],
  ['Try again after a failed final check only re-polls', /('Press Try again to send the web files once more\.'), 'fs'\);(\s*if \(res === 'gone'\))/, '$1);$2', 'healthDegraded'],
  ['a mismatch after the write does not send the files again', /(do not match the firmware\.', 'Press Try again to send the web files once more\.'), 'fs'\)/, '$1)', 'silentFailedWrite'],
  ['a safeboot that never restarted is not noticed', 'return { state: \'stuck\' }', 'return { state: \'timeout\' }', 'incompleteFirmwareUpload'],
  ['one failed final version read is a verdict', 'return readVer(5);', 'return readVer(1);', 'finalReadFlaky'],
  ['the unit snapshot says "already there" for a stale plan', 'same = ok && s.ver === r.E', 'same = ok && r.E.length > 0', 'staleSnapshotPlans']
];
for (const [label, from, to, scenario] of MUTANTS) {
  test('mutant goes red: ' + label, { skip: scenario.startsWith('artifact:') ? skip : false }, async () => {
    const M = mutate(from, to);
    let red;
    if (scenario === 'artifact:safeboot') red = M.classify(bytes(A.safeboot)).kind !== 'safeboot';
    else if (scenario === 'artifact:pageApp') red = (await appWithPage(M, true, header.replace(from, to))) === false;
    else red = await fails(SCENARIOS[scenario], M);
    assert.ok(red, 'the scenario "' + scenario + '" did not notice this defect');
  });
}

// ---------- 4. what a filesystem update keeps and loses ----------
// The image replaces the whole LittleFS partition. The firmware writes back only
// settings.ini and dallas_labels.ini (OTGW-ModUpdateServer-esp32.h) and the DHW
// water total (flushDHWWaterMeter() in doRestart). Every other runtime file must
// therefore be absent from the image, which is what the page text says is lost.
test('filesystem image: the runtime files the page lists as "starts fresh" are not in it', { skip }, () => {
  const img = fs.readFileSync(A.fsimg);
  const runtime = ['sat_pid_state.json', 'sat_energy.json', 'sat_energy_estimate.json', 'sat_hcr.json', 'sat_markers.json',
    'ot-boiler.json', 'ot-thermo.json', 'reboot_count.txt', 'reboot_log.txt', 'bootdetect.log', 'topic_cleanup.bin'];
  for (const n of runtime) assert.equal(img.indexOf(n), -1, n + ' is in the image');
  assert.ok(img.indexOf('settings.ini') > 0, 'the default settings.ini ships (the firmware overwrites it from RAM)');
});

// ---------- 5. cross-version contract (docs/guides/WIFI_FIRMWARE_UPDATE.md) ----------
// Release N's page drives release N+1's firmware, so these names are an API.
// The test pins the page's side to the firmware sources: when one is renamed
// the other must follow, and the guide section must be revisited.
test('contract: every endpoint and field the page depends on exists in the firmware sources', () => {
  const read = (...p) => fs.readFileSync(path.join(ROOT, ...p), 'utf8');
  const upd = read('src', 'OTGW-firmware', 'OTGW-ModUpdateServer-esp32.h');
  const rest = read('src', 'OTGW-firmware', 'restAPI.ino');
  const sb = read('safeboot', 'src', 'main.cpp');
  // main firmware: POST /safeboot, POST /update with cmd=100 and size=, 'Flash error' body
  assert.match(upd, /srv\.on\("\/safeboot", HTTP_POST/);
  assert.match(upd, /webSendPStream\(200, PSTR\("text\/html"\), _serverIndex\)/, 'the large page streams from flash');
  assert.match(upd, /getParam\("cmd", false\)/);
  assert.match(upd, /getParam\("size", false\)/);
  assert.match(upd, /"Flash error: %s"/);
  // device/info fields the page reads
  for (const f of ['fwversion', 'hostname', 'ipaddress', 'networkmode', 'board', 'hardware_type', 'safeboot_available', 'app_ota_available']) {
    assert.ok(rest.includes('F("' + f + '")'), 'device/info field ' + f);
  }
  // hash-check and health
  for (const f of ['filesystem_check', 'match', 'fs_hash', 'fw_hash']) assert.ok(rest.includes('F("' + f + '")'), 'hash-check field ' + f);
  assert.match(rest, /"\/api\/v2\/filesystem\/hash-check"|hash-check/);
  // safeboot: /info keys, POST /update, 302 for unknown paths, the log tag the page uses to recognise the image
  for (const k of ['safeboot', 'app_valid', 'app_rolled_back', 'ap_mode']) assert.ok(sb.includes('\\"' + k + '\\"'), 'safeboot /info key ' + k);
  assert.match(sb, /server\.on\("\/info", HTTP_GET/);
  assert.match(sb, /server\.on\("\/update", HTTP_POST/);
  assert.match(sb, /\[safeboot\] app valid/);
  assert.match(sb, /"%s-safeboot"/);
  // the page itself uses exactly these
  for (const needle of ["'/safeboot'", "'/info'", "'/update'", "/update?cmd=100&size=", '/api/v2/device/info', '/api/v2/filesystem/hash-check', '/api/v2/health', 'Flash error']) {
    assert.ok(LOGIC.includes(needle), 'page logic no longer uses ' + needle);
  }
});
