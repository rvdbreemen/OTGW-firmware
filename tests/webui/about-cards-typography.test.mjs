// TASK-2948 (Robert 2026-11-20): the About/Credits cards at the end of Settings use the same type as every other
// Settings card. The TASK-1893 cards put their text in bare <p> elements, which inherit the 16 px body size, next to a
// .95rem heading and .84rem settings rows. The redesign builds the cards from the settings row classes (srow, slbl,
// shint), in three groups: Makers (Robert first), Credits, Community.
//
// Drives the REAL v2.html/v2.js/v2.css in headless Chrome. Point OTGW_DATA_DIR at an older copy of data/ to see it red.
// SHOT_DIR=<dir> saves screenshots at 1400 px and 390 px.
//
// Run: node --test tests/webui/about-cards-typography.test.mjs
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { startPage } from './lib/page-harness.mjs';

// Every visible text in the cards, with the computed font size and family of the element that holds it.
const PROBE = `(function () {
  var ids = ['setAbout', 'setCredits', 'setContrib'], out = { cards: [], texts: [] };
  ids.forEach(function (id) {
    var card = document.getElementById(id); if (!card) return;
    var h = card.querySelector('h4');
    out.cards.push({ id: id, heading: h ? h.textContent.trim() : null });
    var tw = document.createTreeWalker(card, NodeFilter.SHOW_TEXT);
    while (tw.nextNode()) {
      var n = tw.currentNode, p = n.parentElement, s = n.nodeValue.trim();
      if (!s || !p || !p.getClientRects().length) continue;
      var c = getComputedStyle(p);
      out.texts.push({ card: id, tag: p.tagName, size: c.fontSize, family: c.fontFamily, text: s.slice(0, 40) });
    }
  });
  var row = document.createElement('div'); row.className = 'srow';
  row.innerHTML = '<span class="slbl">x<span class="shint">y</span></span>';
  document.getElementById('setAbout').parentNode.appendChild(row);
  out.ref = { slbl: getComputedStyle(row.querySelector('.slbl')).fontSize, shint: getComputedStyle(row.querySelector('.shint')).fontSize,
    h4: getComputedStyle(document.querySelector('#setAbout h4')).fontSize, family: getComputedStyle(row.querySelector('.slbl')).fontFamily };
  row.remove();
  return out;
})()`;

async function readCards(width) {
  const h = await startPage({ width, api: () => undefined });
  try {
    assert.ok(await h.until(`!!document.querySelector('.seg[data-page="settings"]')`, 30000), 'page loaded');
    await h.evalJs(`document.querySelector('.seg[data-page="settings"]').click()`);
    assert.ok(await h.until(`!!document.getElementById('setAbout') && document.getElementById('setAbout').getClientRects().length > 0`, 20000), 'About card shown');
    await h.sleep(800);
    const r = await h.evalJs(PROBE);
    if (process.env.SHOT_DIR) {
      await h.evalJs(`document.getElementById('setAbout').scrollIntoView({ block: 'start' })`);
      await h.sleep(300);
      await h.shot(path.join(process.env.SHOT_DIR, `about-${width}.png`));
      await h.evalJs(`document.getElementById('setContrib').scrollIntoView({ block: 'end' })`);
      await h.sleep(300);
      await h.shot(path.join(process.env.SHOT_DIR, `about-${width}-end.png`));
    }
    r.sw = await h.evalJs(`document.documentElement.scrollWidth <= document.documentElement.clientWidth`);
    assert.deepEqual(h.exceptions, []);
    return r;
  } finally { await h.close(); }
}

for (const width of [1400, 390]) {
  test(`About/Credits cards at ${width} px: three groups, settings type, no 16 px body text`, async () => {
    const r = await readCards(width);
    const allowed = new Set([r.ref.h4, r.ref.slbl, r.ref.shint]);
    const off = r.texts.filter((x) => !allowed.has(x.size));
    assert.deepEqual(off, [], `every text uses the heading, row label or row hint size (${[...allowed].join(', ')})`);
    assert.deepEqual(r.texts.filter((x) => x.family !== r.ref.family), [], 'one font family, the settings rows\' one');
    assert.deepEqual(r.cards.map((c) => c.heading), ['Makers', 'Credits', 'Community'], 'three cards in this order');
    const makers = r.texts.filter((x) => x.card === 'setAbout' && x.tag === 'B').map((x) => x.text);
    assert.equal(makers[0], 'Robert van den Breemen', `Robert first (${makers})`);
    const credits = r.texts.filter((x) => x.card === 'setCredits' && (x.tag === 'B' || x.tag === 'A')).map((x) => x.text);
    assert.ok(!r.texts.some((x) => /Aandewiel/.test(x.text)), 'Willem Aandewiel is not named (Robert 2026-11-20)');
    for (const n of ['Schelte Bron', 'George Ntellas', 'Alex (Alexwijn)', 'Nodo Shop']) assert.ok(credits.some((c) => c.startsWith(n)), `${n} credited (${credits})`);
    assert.ok(r.sw, 'no horizontal page scroll');
  });
}
