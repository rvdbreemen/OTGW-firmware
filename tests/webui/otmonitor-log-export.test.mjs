// TASK-1813 guard: Monitor > Log "OTmonitor log" writes a file that OTmonitor 6.6 itself can read.
// OTmonitor picks the timestamp layout from the column where [ABRT]+8 hex most often starts
// (fileanal, otmonitor.tcl:1790-1842) and knows only columns 16, 17, 18, 21, 22, 23, 26. The plain
// log download puts the frame at column 35 (time, 18-character source label, frame), so OTmonitor
// finds no layout and reads 0 frames. The export writes layout 23, "YYYYMMDD-HHMMSS.uuuuuu <frame>",
// with the date taken from the device clock at receive time (also across midnight), plus Error lines.
//
// Runs the REAL shipped otmonLogText() cut out of data/v2.js. The OTmonitor part feeds the files to
// OTmonitor's REAL fileanal/filedata procs under tclsh (tests/webui/lib/otmon_reader.tcl); it is
// skipped, with the reason, when tclsh (Git for Windows ships one) or the private other-projects
// submodule is missing. Older v2.js has no otmonLogText, so extraction fails (red).
//
// Not covered: the browser download itself and the OTmonitor GUI (manual check, AC 3).
//
// Run: node --test tests/webui/otmonitor-log-export.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(HERE, '..', '..');
const V2JS = process.env.OTGW_V2_JS || path.join(ROOT, 'src', 'OTGW-firmware', 'data', 'v2.js');
const SRC = fs.readFileSync(V2JS, 'utf8');
const OTMON_TCL = process.env.OTGW_OTMONITOR_TCL ||
  path.join(ROOT, 'other-projects', 'otmonitor-6.6', 'otmonitor.vfs', 'otmonitor.tcl');
const READER = path.join(HERE, 'lib', 'otmon_reader.tcl');

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

function loadExport() {
  // eslint-disable-next-line no-new-func
  return new Function(extractFunction(SRC, 'otmonLogText') + '\nreturn otmonLogText;')();
}

// Device-local wall clock as the web UI keeps it (devClockBase: the device time in UTC fields).
const dev = (d, hms) => Date.UTC(2026, 10, d, +hms.slice(0, 2), +hms.slice(3, 5), +hms.slice(6, 8), +(hms.slice(9) || 0));

// A night across midnight, as logBuf holds it: every kind of line the log shows.
const LINES = [
  ['23:59:58.100000 Thermostat         T00000300', dev(11, '23:59:58.300')],
  ['23:59:58.200000 * PS=0 summary off', dev(11, '23:59:58.400')],                       // event: left out
  ['23:59:59.900000 Boiler             B40000300  0 Read-Ack          Status', dev(12, '00:00:00.200')],   // received after midnight
  ['23:59:59.950000 > TT=20.5', dev(12, '00:00:00.250')],                                 // command: left out
  ['00:00:00.400000 Request Boiler     R10013C00', dev(12, '00:00:00.600')],
  ['00:00:00.500000 < TT: 20.50', dev(12, '00:00:00.700')],                               // response: left out
  ['00:00:01.000000 Answer Thermostat  A10013C00', dev(12, '00:00:01.100')],
  ['00:00:02.000000 ! Error 01 [3]', dev(12, '00:00:02.100')],
  ['00:00:03.000000 PS1 00000011/00000000,10.00,1/0,20.50', dev(12, '00:00:03.100')],    // PS1 summary: left out
  ['00:00:04 S SAT: target 20.5 C', dev(12, '00:00:04.100')],                            // SAT narration: left out
  ['DIAG chunk 3/7 0a1b2c', dev(12, '00:00:04.200')],                                    // no timestamp: left out
];
const EXPECT = [
  '20261111-235958.100000 T00000300',
  '20261111-235959.900000 B40000300',
  '20261112-000000.400000 R10013C00',
  '20261112-000001.000000 A10013C00',
  '20261112-000002.000000 Error 01',
];

function findTclsh() {
  const cands = [process.env.OTGW_TCLSH, 'tclsh',
    'C:\\Program Files\\Git\\ucrt64\\bin\\tclsh.exe', 'C:\\Program Files\\Git\\mingw64\\bin\\tclsh.exe'];
  for (const c of cands) {
    if (!c) continue;
    const r = spawnSync(c, [], { input: 'puts ok\n', encoding: 'utf8' });
    if (r.status === 0 && /ok/.test(r.stdout)) return c;
  }
  return null;
}
const TCLSH = findTclsh();
const SKIP = !TCLSH ? 'no tclsh found (set OTGW_TCLSH)' :
  (!fs.existsSync(OTMON_TCL) ? 'OTmonitor source not found: ' + OTMON_TCL + ' (private other-projects submodule)' : false);

function otmonRead(text) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'otmon-'));
  const f = path.join(dir, 'otlog.txt');
  fs.writeFileSync(f, text, 'latin1');
  // End time = the file's download moment; only time-only layouts use it (base = end - 6 h).
  const r = spawnSync(TCLSH, [READER, OTMON_TCL, f, '20261112-090000'], { encoding: 'utf8' });
  fs.rmSync(dir, { recursive: true, force: true });
  assert.equal(r.status, 0, 'tclsh failed: ' + r.stderr + r.stdout);
  const out = r.stdout.split(/\r?\n/);
  assert.ok(out.includes('END'), 'reader did not finish: ' + r.stdout);
  return {
    layout: (out.find(l => l.startsWith('LAYOUT ')) || '').slice(7),
    msgs: out.filter(l => l.startsWith('MSG ')).map(l => l.slice(4)),
  };
}

test('export: one line per OT frame plus Error lines, dated by the device clock across midnight, CRLF', () => {
  const otmonLogText = loadExport();
  const txt = otmonLogText(LINES.map(l => l[0]), LINES.map(l => l[1]));
  assert.equal(txt, EXPECT.join('\r\n') + '\r\n');
});

test('export: the date follows the line time, not the receive time, when they straddle midnight', () => {
  const otmonLogText = loadExport();
  // Device clock 1 s behind the line stamp (received 23:59:59.500 for a line stamped 00:00:00.500).
  const ahead = otmonLogText(['00:00:00.500000 Boiler             B40000300'], [dev(11, '23:59:59.500')]);
  assert.equal(ahead, '20261112-000000.500000 B40000300\r\n');
  // A line stamped 23:59:59.900 received at 00:00:00.200: still the day before.
  const behind = otmonLogText(['23:59:59.900000 Boiler             B40000300'], [dev(12, '00:00:00.200')]);
  assert.equal(behind, '20261111-235959.900000 B40000300\r\n');
  // Nothing exportable: an empty file, not a lone line end.
  assert.equal(otmonLogText(['00:00:02.000000 * boot'], [dev(12, '00:00:02.000')]), '');
});

test('export: lower-case hex or a frame outside the label column is not taken as a frame', () => {
  const otmonLogText = loadExport();
  const txt = otmonLogText([
    '00:00:01.000000 Boiler             b40000300',
    '00:00:02.000000 < PR: A=B40000300',
  ], [dev(12, '00:00:01.000'), dev(12, '00:00:02.000')]);
  assert.equal(txt, '');
});

test('OTmonitor 6.6 reads the plain log download: no layout, 0 frames (the bug)', { skip: SKIP }, () => {
  const plain = LINES.map(l => l[0]).join('\n');   // what downloadLog() writes: logBuf.join('\n')
  const r = otmonRead(plain);
  assert.equal(r.layout, '{}', 'fileanal found a layout in the plain download: ' + r.layout);
  assert.deepEqual(r.msgs, []);
});

test('OTmonitor 6.6 reads the OTmonitor log export: layout 23, every frame and Error line with its date', { skip: SKIP }, () => {
  const otmonLogText = loadExport();
  const r = otmonRead(otmonLogText(LINES.map(l => l[0]), LINES.map(l => l[1])));
  assert.equal(r.layout, '{a15xa6xa* %Y%m%d-%H%M%S}');
  assert.deepEqual(r.msgs, EXPECT);
});
