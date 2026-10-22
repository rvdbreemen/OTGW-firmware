// TASK-1295: the trigger-bit selects for the GPIO output and the webhook must label the bit
// numbers the firmware tests. OTcurrentSystemState.Statusflags = (MasterStatus << 8) | SlaveStatus
// (OTGW-Core.ino), and the slave status byte of OT MsgID 0 has bit 0 = fault, 1 = CH active,
// 2 = DHW active, 3 = flame. Before the fix v2.js labelled 0 'Flame' and 3 'Fault'.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
// OTGW_V2_JS points at another copy (e.g. `git show HEAD:src/OTGW-firmware/data/v2.js`) for old-vs-fix runs.
const v2 = readFileSync(process.env.OTGW_V2_JS || path.join(root, 'src', 'OTGW-firmware', 'data', 'v2.js'), 'utf8');
// OTGW_SETTINGS_HELP_H does the same for settingsHelp.h.
const help = readFileSync(process.env.OTGW_SETTINGS_HELP_H || path.join(root, 'src', 'OTGW-firmware', 'settingsHelp.h'), 'utf8');
const fixture = readFileSync(path.join(root, 'tests', 'webui', 'fixtures', 'status-bits-replay.txt'), 'utf8');

function enumOpts(key) {
  const m = v2.match(new RegExp('^\\s*' + key + ':\\s*(\\[\\[.*\\]\\]),?\\s*$', 'm'));
  assert.ok(m, 'ENUM_OPTS entry for ' + key);
  return JSON.parse(m[1].replace(/'/g, '"'));
}

const EXPECTED = [[0, 'Fault'], [1, 'CH active'], [2, 'DHW active'], [3, 'Flame']];

for (const key of ['gpiooutputstriggerbit', 'webhooktriggerbit']) {
  test(key + ' options name the bit the firmware tests', () => {
    assert.deepEqual(enumOpts(key), EXPECTED);
  });
  test(key + ' help text gives the bit numbers', () => {
    const m = help.match(new RegExp('"' + key + '":"([^"]*)"'));
    assert.ok(m, 'help text for ' + key);
    for (const part of ['0 = fault', '1 = CH active', '2 = DHW active', '3 = flame']) {
      assert.ok(m[1].includes(part), key + ' help text names ' + part);
    }
  });
}

// The bench replay for this task: boiler status (MsgID 0) answers whose low byte steps
// 00 -> 08 (flame only) -> 00 -> 01 (fault only) -> 00, six frames each. Keeps the fixture honest.
test('status-bits-replay.txt carries flame-only and fault-only boiler answers with even parity', () => {
  const lines = fixture.trim().split(/\r?\n/);
  assert.equal(lines.length, 120);
  const lbs = [];
  for (const line of lines) {
    assert.match(line, /^[TRBA][0-9A-F]{8}$/);
    const bytes = [0, 2, 4, 6].map(i => parseInt(line.slice(1 + i, 3 + i), 16));
    const ones = bytes.reduce((n, b) => n + b.toString(2).split('1').length - 1, 0);
    assert.equal(ones % 2, 0, 'even parity: ' + line);
    assert.equal(bytes[1], 0, 'MsgID 0: ' + line);
    if (line[0] === 'B') { assert.equal(bytes[0] & 0x70, 0x40, 'Read-Ack: ' + line); lbs.push(bytes[3]); }
  }
  const runs = lbs.filter((v, i) => i === 0 || v !== lbs[i - 1]);
  assert.deepEqual(runs, [0x00, 0x08, 0x00, 0x01, 0x00]);
});
