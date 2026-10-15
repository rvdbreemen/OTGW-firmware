// TASK-1230 guard: the v2 Settings save must refuse a static IP with a bad octet.
//
// A typo in a static IP locks the user out until an AP reset. v2.js checks every
// octet (ipv4Err) and every interface set to static (ipGroupsErr) before
// saveSettings() posts anything.
//
// This test runs the REAL shipped functions: it cuts the source text of ipv4Err and
// ipGroupsErr out of data/v2.js by brace matching and evaluates them. Nothing is
// re-implemented here. Point OTGW_V2_JS at an older copy of v2.js to show the gap
// (the old file has no validator, so every test fails on "not found").
//
// Run: node --test tests/webui/static-ip-validation.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const V2JS = process.env.OTGW_V2_JS || path.join(HERE, '..', '..', 'src', 'OTGW-firmware', 'data', 'v2.js');

// Cut "function <name>(...) { ... }" out of the source by brace matching,
// skipping string literals and comments (same helper as dallas-discovery-toast).
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

function load() {
  const src = fs.readFileSync(V2JS, 'utf8');
  const body = extractFunction(src, 'ipv4Err') + '\n' + extractFunction(src, 'ipGroupsErr') +
    '\nreturn { ipv4Err: ipv4Err, ipGroupsErr: ipGroupsErr };';
  return new Function(body)();
}

test('ipv4Err accepts valid dotted quads', () => {
  const { ipv4Err } = load();
  for (const ip of ['192.168.1.10', '0.0.0.0', '255.255.255.0', '10.0.0.1', ' 172.16.5.4 ']) {
    assert.equal(ipv4Err(ip), '', ip + ' should be accepted');
  }
});

test('ipv4Err rejects bad octets and names the octet', () => {
  const { ipv4Err } = load();
  const bad = {
    '192.168.1.256': /octet 4/,
    '192.168.300.1': /octet 3/,
    '192.168.1.': /octet 4/,
    '192.168..1': /octet 3/,
    '192.168.1.1a': /octet 4/,
    '-1.2.3.4': /octet 1/,
    '1.2.3.1000': /octet 4/,
    '1.2.3': /four numbers/,
    '1.2.3.4.5': /four numbers/,
    '': /four numbers/,
  };
  for (const [ip, why] of Object.entries(bad)) {
    const e = ipv4Err(ip);
    assert.notEqual(e, '', JSON.stringify(ip) + ' should be rejected');
    assert.match(e, why, JSON.stringify(ip) + ' -> ' + e);
  }
});

// Effective-value getter like saveSettings() builds: undefined = key not exposed.
const getter = (o) => (k) => (k in o ? o[k] : undefined);

test('ipGroupsErr: DHCP interfaces are not checked', () => {
  const { ipGroupsErr } = load();
  assert.equal(ipGroupsErr(getter({ wifistaticip: '', wifisubnet: 'junk', ethstaticip: 'false', ethsubnet: 'junk' })), '');
  assert.equal(ipGroupsErr(getter({})), '', 'no network keys at all (e.g. a test fake) must pass');
});

test('ipGroupsErr: a static Wi-Fi group needs a valid IP, mask and gateway', () => {
  const { ipGroupsErr } = load();
  const good = { wifistaticip: '192.168.1.50', wifisubnet: '255.255.255.0', wifigateway: '192.168.1.1', wifidns1: '', wifidns2: '' };
  assert.equal(ipGroupsErr(getter(good)), '');
  assert.match(ipGroupsErr(getter({ ...good, wifisubnet: '' })), /Wi-Fi subnet mask: is required/);
  assert.match(ipGroupsErr(getter({ ...good, wifigateway: '192.168.1.999' })), /Wi-Fi gateway: octet 4/);
  assert.match(ipGroupsErr(getter({ ...good, wifistaticip: '192.168.1' })), /Wi-Fi IP address/);
  assert.match(ipGroupsErr(getter({ ...good, wifidns2: '8.8.8.888' })), /Wi-Fi DNS: octet 4/);
});

test('ipGroupsErr: a static Ethernet group (ethstaticip true) is checked', () => {
  const { ipGroupsErr } = load();
  const good = { ethstaticip: 'true', ethipaddress: '10.0.0.5', ethsubnet: '255.255.255.0', ethgateway: '10.0.0.1', ethdns: '' };
  assert.equal(ipGroupsErr(getter(good)), '');
  assert.match(ipGroupsErr(getter({ ...good, ethipaddress: '10.0.0.256' })), /Ethernet IP address: octet 4/);
  assert.match(ipGroupsErr(getter({ ...good, ethgateway: '' })), /Ethernet gateway: is required/);
});
