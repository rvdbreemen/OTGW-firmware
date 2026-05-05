---
id: TASK-514
title: 'UI: SAT dashboard graphs above-the-fold (move charts up, no scroll)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-02 18:31'
updated_date: '2026-05-05 08:10'
labels:
  - ui
  - sat
  - dashboard
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeantd reports (Discord #dev-sat-mqtt 2026-04-28 06:03 UTC, msg 1498565287757484102) that the SAT dashboard charts sit too far down the page; users have to scroll before seeing them. Charts are the highest-information element on the SAT dashboard and should be visible immediately.

**Goal:** reorder the SAT dashboard so the chart cluster lands within the first viewport on common laptop sizes (1366x768 and 1920x1080) without forcing the user to scroll.

**Touches:** `src/OTGW-firmware/data/index.html` (SAT panel structure), `src/OTGW-firmware/data/components.css` (panel ordering / grid), possibly `sat.js` if init order matters.

**Reference screenshot:** Discord attachment on the source message — fetch via the Discord client when picking this up.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On 1920x1080 viewport the SAT dashboard charts are fully visible without scrolling
- [x] #2 On 1366x768 viewport at least the top chart is fully visible without scrolling
- [x] #3 Chart ordering preserved (heating curve / flow / output, in current order)
- [x] #4 No regression on mobile (≤480px) — charts stack below tiles, not above (mobile users scroll anyway)
- [x] #5 Layout change verified in Chrome and Firefox latest
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Plan

**Pure DOM reorder, geen CSS @media trickery.**

### Huidige volgorde op simple view (DOM = visueel)
1. Header (`sat-header`, ~50 px)
2. Tiles 2x2 (`sat-tiles`, ~250 px) — Outside / Boiler / Room / Target
3. Status summary (`sat-status-summary` data-view="simple", ~30 px) — "Idle"
4. Controls (preset + mode buttons, ~150 px)
5. DHW (slider + HW switch, ~80 px)
6. Weather (data-view="expert diag", hidden op simple)
7. **Temperature History chart** (`#sat-chart`, height:300px)
8. Heating Curve chart (data-view="expert diag")

Op 1366x768 = ~560 px content vóór charts; charts vallen buiten de viewport.

### Doel-volgorde
1. Header
2. Tiles
3. **Temperature History chart** ← omhoog gehaald
4. **Heating Curve chart** ← omhoog gehaald (nog steeds expert/diag-only)
5. Status summary
6. Controls
7. DHW
8. Weather
9. Control Info, PID, PWM, ... (expert/diag-only)

### Wijziging
Eén Edit op `src/OTGW-firmware/data/index.html`: het chart-blok (regels 417-432, twee `<div class="sat-section">` met chart-titels) verplaatsen naar net na het sluitende `</div>` van de `sat-tiles` (regel 351). Geen CSS-aanpassingen, geen JS-aanpassingen — `sat.js` queryt elements via id/class, niet via DOM-positie.

### AC-verdict per criterium
- **AC #1** (1920x1080: charts fully visible): waarschijnlijk ja — Header (50px) + Tiles (~250px) + Chart (300px) = 600px, ruim binnen 1080px. Visuele verificatie nodig.
- **AC #2** (1366x768: top chart fully visible): mikpunt — Header + Tiles + Chart = 600px ≤ 700px viewport. Krap maar mogelijk.
- **AC #3** (chart ordering preserved): ✓ Temperature History blijft vóór Heating Curve.
- **AC #4** (mobile ≤480px: charts onder tiles, niet boven): ✓ DOM-reorder zet charts direct ná tiles. Mobile DOM = visuele volgorde, dus charts onder tiles ✓.
- **AC #5** (Chrome + Firefox latest): kan ik niet verifiëren — vereist hardware/browser test door user.

### Build / verify
- `python evaluate.py --quick` — geen failures verwacht, geen relevant binding rule raakt deze wijziging.
- `python build.py` (zonder --firmware) — herbouwt LittleFS image + bumpt `data/version.hash`. Firmware-binary verandert niet, maar de filesystem-image wel.

### Risico's
- Lage kans op subtiele JS-init volgorde issue als ECharts in `#sat-chart` afhankelijk is van een eerdere DOM-element. Geverifieerd: `sat.js` initialiseert via `document.querySelector('.sat-dashboard')` en `document.getElementById('sat-chart')`, geen volgorde-aannames.
- Data-view filtering blijft werken (filter is op `data-view` attribuut, niet op DOM-positie).

### Bewust niet gedaan
- Geen CSS `order:` met @media. Onnodig — DOM-reorder volstaat voor zowel desktop als mobile per AC's.
- Geen compactie van bestaande secties (kleinere tiles, minder padding). Out-of-scope; user kan dat later evalueren.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-05: Work was already shipped on feature-dev-2.0.0-otgw32-esp32-sat-support in commit 743dbfab9 (2026-05-03 18:47, "feat(satui): move SAT dashboard charts above-the-fold for laptop viewports"). Task was left In Progress. User confirmed visual/browser verification — closing.

Implementation landed in commit 743dbfab on feature-dev-2.0.0-otgw32-esp32-sat-support (2026-05-03 18:47): pure DOM reorder in data/index.html — chart block (Temperature History + Heating Curve) moved from below Controls/DHW/Weather to right after the 2x2 tile grid.

Layout math (per commit message):
- 1366x768: top of Temperature History at ~300px, well within viewport (AC #2 satisfied).
- 1920x1080: charts entirely above the fold (AC #1 satisfied).
- Mobile ≤480px: DOM order preserved → charts stack below tiles (AC #4 satisfied).
- Chart ordering preserved: Temperature History → Heating Curve (AC #3 satisfied).
- Chrome + Firefox latest: visually verified by user (AC #5 satisfied).

LittleFS image rebuilt by the build script; firmware binary unchanged.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT dashboard charts now sit above-the-fold on common laptop viewports (1366x768, 1920x1080) without scroll. Pure DOM reorder in data/index.html — the chart block moves from below Controls/DHW/Weather to immediately after the 2x2 tile grid. No CSS @media trick required; mobile DOM order keeps charts stacked below tiles, so accessibility (WCAG 1.3.2) and screen-reader / tab-key navigation stay aligned with visual order.

Landed as commit 743dbfab on feature-dev-2.0.0-otgw32-esp32-sat-support (2026-05-03). LittleFS image rebuilt; firmware binary unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
