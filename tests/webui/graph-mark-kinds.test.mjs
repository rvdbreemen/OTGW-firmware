// TASK-1914: Monitor > Graph names its marks. The dashed red/green lines are the live connection
// (WebSocket) dropping and coming back (TASK-1821), which a sleeping tab or a Wi-Fi hiccup also
// causes; a reader took them for restarts. Now each line has a tooltip and the graph has a legend
// line that say "not a restart", and a real restart (the device's boot counter went up, read from
// device/info when the live connection comes back) gets its own solid amber mark.
//
// Runs the REAL helpers cut out of data/v2.js (noteDeviceBoot, graphMarkKind, graphMarkStyle,
// graphMarkLegendKinds) and reads the English strings from the shipped I18N_EN. An older v2.js has
// none of these helpers, so extraction fails (red).
//
// Not covered here: the browser painting of the <line>/<title> elements and the legend spans (screenshot).
//
// Run: node --test tests/webui/graph-mark-kinds.test.mjs
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

const fns = ['noteDeviceBoot', 'graphMarkKind', 'graphMarkStyle', 'graphMarkLegendKinds'].map(n => extractFunction(SRC, n)).join('\n');
const make = () => new Function('var gMarks = [], gBootSeen = null;\n' + fns +
  '\nreturn { boot: noteDeviceBoot, kind: graphMarkKind, style: graphMarkStyle, kinds: graphMarkLegendKinds, marks: function () { return gMarks; } };')();

const en = key => {
  const m = SRC.match(new RegExp('"' + key.split('.')[1] + '": "([^"]*)"'));
  if (!m) throw new Error('no English string for ' + key);
  return m[1];
};

test('the dashed connection marks carry a tooltip and a legend label that say they are not a restart', () => {
  const g = make();
  for (const k of ['lost', 'back']) {
    const st = g.style(k);
    assert.ok(st.dash, `${k} stays dashed`);
    assert.match(en(st.tip), /not a restart/i, `${k} tooltip: ${en(st.tip)}`);
    assert.match(en(st.label), /live link/i, `${k} legend label: ${en(st.label)}`);
  }
  assert.equal(g.kind({ up: 1 }), 'back');
  assert.equal(g.kind({ up: 0 }), 'lost');
});

test('a restart mark is its own kind, solid and in another colour than both connection marks', () => {
  const g = make();
  const r = g.style('reboot'), a = g.style('lost'), b = g.style('back');
  assert.equal(g.kind({ reboot: 1 }), 'reboot');
  assert.equal(r.dash, '', 'restart mark is solid');
  assert.notEqual(r.stroke, a.stroke); assert.notEqual(r.stroke, b.stroke);
  assert.match(en(r.tip), /restarted/i);
});

test('simulated uptime reset: the first boot count is the baseline, only a higher count adds a restart mark', () => {
  const g = make();
  assert.equal(g.boot(41, 1000), false, 'first reading is the baseline');
  assert.equal(g.boot(41, 2000), false, 'same count after a reconnect: no restart');
  assert.equal(g.boot(42, 3000), true, 'count went up: restart');
  assert.deepEqual(g.marks(), [{ t: 3000, reboot: 1 }]);
  assert.equal(g.boot(undefined, 4000), false, 'missing count adds nothing');
  assert.equal(g.boot(3, 5000), false, 'a lower count (counter reset) is a new baseline, not a restart');
  assert.equal(g.boot(4, 6000), true, 'and the next restart after it is seen');
  assert.equal(g.marks().length, 2);
});

test('the legend lists only the kinds on screen, in a fixed order', () => {
  const g = make();
  assert.deepEqual(g.kinds([]), []);
  assert.deepEqual(g.kinds([{ kind: 'back' }, { kind: 'lost' }]), ['lost', 'back']);
  assert.deepEqual(g.kinds([{ kind: 'reboot' }, { kind: 'back' }]), ['back', 'reboot']);
});
