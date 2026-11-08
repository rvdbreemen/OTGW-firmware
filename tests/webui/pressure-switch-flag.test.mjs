// TASK-1734 guard: the web UI shows the CH water pressure exactly as decoded from MsgID 18,
// like MQTT, REST and Home Assistant do (Robert, 2026-11-08). The TASK-1682 "switch?" mark for
// a fixed 0.1 bar is gone: no time window, no dash, no tooltip, no special colour.
//
// Checks every pressure readout in data/v2.js and runs the REAL fmt() and pressClass().
// Point OTGW_V2_JS at an older copy (git show HEAD~1:... > file) to see it red.
//
// Run: node --test tests/webui/pressure-switch-flag.test.mjs
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

test('no pressure-switch mark is left in v2.js', () => {
  assert.ok(!/PRESS_SWITCH|pressFlatSince|trackPressSwitch|pressIsSwitch|pressText|setPress/.test(SRC),
    'TASK-1682 switch code still present');
});

test('every pressure readout shows the decoded value', () => {
  const plain = "fmt(model.pressure, 2, ' bar')";
  assert.ok(SRC.includes("txt('aPress', " + plain + ')'), '#aPress');
  assert.ok(SRC.includes("txt('bPress', " + plain + ')'), '#bPress');
  assert.ok(SRC.includes("['Pressure', " + plain + ", '']"), 'mobile chip');
  assert.ok(SRC.includes("['PRESSURE', " + plain + ', pressClass(model.pressure)]'), 'grid cell keeps the normal colour band');
  assert.ok(/case 18: if \(isAck\) \{ model\.pressure = f88\(u16\); changed = true; \} break;/.test(SRC), 'MsgID 18 stores the decoded value only');
});

test('0.1 bar reads as 0.10 bar in the normal low-pressure band', () => {
  const api = new Function([extractFunction(SRC, 'fmt'), extractFunction(SRC, 'pressClass'),
    'return { fmt, pressClass };'].join('\n'))();
  assert.equal(api.fmt(0.1, 2, ' bar'), '0.10 bar');
  assert.equal(api.pressClass(0.1), 'alert');
});
