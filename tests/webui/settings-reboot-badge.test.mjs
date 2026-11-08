// TASK-1398: the orange REBOOT badge on Settings > Network explains itself (tooltip and
// aria-label) and does not look clickable (default cursor, not a button/link, no tab stop).
// Drives the REAL v2.html/v2.js in headless Chrome against a fake device.
// Point OTGW_DATA_DIR at an older copy of data/ to see it red.
//
// Run: node --test tests/webui/settings-reboot-badge.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage, CHROME } from './lib/page-harness.mjs';

const S = (value, maxlen = 15) => ({ value, type: 's', maxlen });
const SETTINGS = { wifistaticip: S(''), wifisubnet: S(''), wifigateway: S(''), wifidns1: S(''), wifidns2: S(''), hostname: S('otgw', 32) };

test('REBOOT badge: tooltip, aria-label, not clickable-looking', { skip: !CHROME && 'no Chrome' }, async () => {
  const h = await startPage({ api: (m, p) => (p.endsWith('/v2/settings') && m === 'GET' ? SETTINGS : undefined) });
  try {
    await h.until(`!!document.querySelector('.seg[data-page="settings"]')`);
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    await h.until(`!!document.querySelector('.rail-item[data-cat="network"]')`);
    await h.evalJs(`document.querySelector('.rail-item[data-cat="network"]').click()`);
    await h.until(`!!document.querySelector('.set-cat-title .badge-reboot')`);
    // TASK-1742: renderSettings() rebuilds the rail and the panel when a settings fetch or poll
    // lands, so a read in a separate evalJs after until() could find both badges gone ([null,null])
    // or read the cursor before the badge's styles applied ('auto'). Each read below is one
    // synchronous evaluation (no render can interleave inside it); repeat it until both badges are
    // there with their stylesheet cursor, for at most 5 s, then assert on the last read. A real
    // regression (badge missing, pointer cursor) still fails: it never settles to the right state.
    const READ = `(function(){
      return ['.rail-item[data-cat="network"] .badge-reboot', '.set-cat-title .badge-reboot'].map(function (sel) {
        var b = document.querySelector(sel); if (!b) return null;
        var cs = getComputedStyle(b);
        return { tag: b.tagName, text: b.textContent, title: b.title, aria: b.getAttribute('aria-label'), role: b.getAttribute('role'),
                 tabindex: b.getAttribute('tabindex'), cursor: cs.cursor, select: cs.userSelect };
      });
    })()`;
    let info = await h.evalJs(READ);
    for (let i = 0; i < 50 && !(info.every((b) => b && b.cursor !== 'auto')); i++) {
      await h.sleep(100);
      info = await h.evalJs(READ);
    }
    const shotDir = process.env.OTGW_SHOT_DIR;
    if (shotDir) await h.shot(path.join(shotDir, '1398.png'));
    for (const b of info) {
      assert.ok(b, 'badge present: ' + JSON.stringify(info));
      assert.equal(b.text, 'REBOOT');
      assert.match(b.title, /reboot/i, 'tooltip explains the badge: ' + JSON.stringify(b));
      assert.match(b.aria || '', /reboot/i, 'aria-label explains the badge: ' + JSON.stringify(b));
      assert.equal(b.cursor, 'default', 'badge must not show a pointer cursor: ' + JSON.stringify(b));
      assert.ok(b.tag === 'SPAN' && !b.role && b.tabindex === null, 'badge is a plain label, not a control');
    }
    assert.deepEqual(h.exceptions, []);
  } finally { await h.close(); }
});
