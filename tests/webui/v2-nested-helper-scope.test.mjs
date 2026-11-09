// TASK-1768: a helper that v2.js only defines inside another function (for example the local esc()
// of the SAT wizard) must not be called from outside that function. beta.166 called esc() in
// satDrawHistory and the SAT legends, where it does not exist: the ReferenceError was swallowed by a
// fetch promise chain, the SAT page stopped rendering halfway and the TASK-1737 lock never applied.
// Pure static check (reads v2.js only, no Chrome). Point OTGW_V2_JS at an older v2.js to see it red.
//
// Run: node --test tests/webui/v2-nested-helper-scope.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');

// Replace strings, template literals, regex-free comments with spaces so braces and names inside
// them do not count; offsets stay the same.
function codeOnly(src) {
  let out = '', i = 0;
  while (i < src.length) {
    const c = src[i], n = src[i + 1];
    if (c === '/' && n === '/') { const e = src.indexOf('\n', i); const end = e < 0 ? src.length : e; out += ' '.repeat(end - i); i = end; continue; }
    if (c === '/' && n === '*') { const e = src.indexOf('*/', i + 2); const end = e < 0 ? src.length : e + 2; out += src.slice(i, end).replace(/[^\n]/g, ' '); i = end; continue; }
    if (c === "'" || c === '"' || c === '`') {
      let j = i + 1;
      while (j < src.length && src[j] !== c) { if (src[j] === '\\') j++; j++; }
      out += c + src.slice(i + 1, j).replace(/[^\n]/g, ' ') + (j < src.length ? c : ''); i = j + 1; continue;
    }
    out += c; i++;
  }
  return out;
}
function bodyEnd(code, from) {
  let depth = 0;
  for (let i = code.indexOf('{', from); i < code.length; i++) {
    if (code[i] === '{') depth++;
    else if (code[i] === '}' && --depth === 0) return i;
  }
  return code.length;
}

test('helpers defined only inside another function are called only there', () => {
  const code = codeOnly(fs.readFileSync(V2JS, 'utf8'));
  // The whole file is one IIFE: its top-level functions are indented by 2 spaces.
  const defs = new Map();   // name -> [{ start, end, indent }]
  for (const m of code.matchAll(/^( *)function ([A-Za-z_$][\w$]*)\s*\(/gm)) {
    const name = m[2];
    if (!defs.has(name)) defs.set(name, []);
    defs.get(name).push({ start: m.index, end: bodyEnd(code, m.index), indent: m[1].length });
  }
  const bad = [];
  for (const [name, list] of defs) {
    if (list.some((d) => d.indent <= 2)) continue;   // defined at top level: callable everywhere
    // A name that is also a parameter or a variable somewhere (done, p, cb ...) is not only a nested
    // helper: a static check cannot tell which binding a call uses, so it is left out.
    const n = name.replace(/\$/g, '\\$');
    if (new RegExp('\\b(var|let|const)\\s+' + n + '\\b|[(,]\\s*' + n + '\\s*(?=[,)])').test(code)) continue;
    // The nested definition is visible in the function that contains it: find each container span.
    const spans = list.map((d) => {
      let best = null;
      for (const [, others] of defs) for (const o of others) {
        if (o.start < d.start && o.end > d.end && (!best || o.start > best.start)) best = o;
      }
      return best || d;
    });
    const re = new RegExp('(^|[^\\w$.])' + name.replace(/\$/g, '\\$') + '\\s*\\(', 'g');
    for (const m of code.matchAll(re)) {
      const at = m.index + m[1].length;
      if (code.slice(Math.max(0, at - 9), at) === 'function ') continue;
      if (!spans.some((s) => at > s.start && at < s.end)) {
        bad.push(`${name}() called at line ${code.slice(0, at).split('\n').length}, outside the function that defines it`);
      }
    }
  }
  assert.deepEqual(bad, [], 'nested helper called out of scope (ReferenceError at runtime)');
});

// beta.166 also translated a label inside graphReadout(arr, keys, metas, off, t): the time parameter
// t hid the translation helper t(), so the Monitor > Graph readout threw "t is not a function".
test('no function that names a parameter or variable t calls the translation helper t()', () => {
  const code = codeOnly(fs.readFileSync(V2JS, 'utf8'));
  const fns = [];
  for (const m of code.matchAll(/function\s*([A-Za-z_$][\w$]*)?\s*\(([^)]*)\)\s*\{/g)) {
    const open = code.indexOf('{', m.index + m[0].length - 1);
    fns.push({ name: m[1] || '(anonymous)', params: m[2].split(',').map((x) => x.trim()), open, end: bodyEnd(code, open), start: m.index });
  }
  // A function's own text: its body with every nested function body blanked out.
  const own = (f, keep) => {
    let b = code.slice(f.open, f.end + 1).split('');
    for (const g of fns) if (g.open > f.open && g.end < f.end && !(keep && keep(g))) {
      for (let k = g.open - f.open; k <= g.end - f.open; k++) if (b[k] !== '\n') b[k] = ' ';
    }
    return b.join('');
  };
  // A local t: a parameter, var/let/const t, catch (t) or an arrow parameter t in the function's own text.
  const ownT = (f) => f.params.includes('t') ||
    /\b(var|let|const)\s+t\b|\bcatch\s*\(\s*t\s*\)|(^|[^\w$.])t\s*=>|\(\s*t\s*(,[^)]*)?\)\s*=>/.test(own(f));
  const bad = [];
  for (const f of fns) {
    if (!ownT(f)) continue;
    // Calls in this function and in nested functions that do not declare their own t (they see this t).
    const visible = own(f, (g) => !ownT(g));
    if (/(^|[^\w$.])t\s*\(/.test(visible)) bad.push(`${f.name} at line ${code.slice(0, f.start).split('\n').length} shadows t and calls t()`);
  }
  assert.deepEqual(bad, [], 'translation helper t() shadowed');
});
