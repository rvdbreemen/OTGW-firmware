// Render a plate SVG to a one-page vector PDF with the Google Fonts embedded (TASK-1984).
// Usage: NODE_PATH=$(npm root -g) node tools/agentic-plates/render_pdf.js <plate.svg> <out.pdf>
const { chromium } = require('playwright'); const fs = require('fs');
(async () => {
  const [,, svgPath, pdfPath] = process.argv;
  const svg = fs.readFileSync(svgPath, 'utf8');
  const [, w, h] = svg.match(/viewBox="0 0 (\d+) (\d+)"/);
  const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: +w, height: +h } });
  await p.setContent(`<!doctype html><html><head><link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;700;800&family=Source+Sans+3:wght@400;600;700&family=JetBrains+Mono:wght@400;600;700&display=swap"><style>@page{size:${w}px ${h}px;margin:0}html,body{margin:0;background:#f4f6f7}svg{display:block}</style></head><body>${svg}</body></html>`, { waitUntil: 'networkidle' });
  await p.evaluate(() => document.fonts.ready);
  await p.pdf({ path: pdfPath, width: w + 'px', height: h + 'px', printBackground: true, pageRanges: '1' });
  await b.close(); console.log('pdf', pdfPath);
})();
