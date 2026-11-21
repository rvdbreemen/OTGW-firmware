const { chromium } = require('playwright');
(async () => {
  const [,, svgPath, pngPath, scale] = process.argv;
  const fs = require('fs');
  const svg = fs.readFileSync(svgPath, 'utf8');
  const m = svg.match(/viewBox="0 0 (\d+) (\d+)"/);
  const w = +m[1], h = +m[2];
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: +(scale || 1) });
  await page.setContent(`<!doctype html><html><head><link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;700;800&family=Source+Sans+3:wght@400;600;700&family=JetBrains+Mono:wght@400;600;700&display=swap"><style>html,body{margin:0;background:#f4f6f7}</style></head><body>${svg}</body></html>`, { waitUntil: 'networkidle' });
  await page.evaluate(() => document.fonts.ready);
  await page.screenshot({ path: pngPath, clip: { x: 0, y: 0, width: w, height: h } });
  await browser.close();
  console.log('rendered', pngPath);
})();
