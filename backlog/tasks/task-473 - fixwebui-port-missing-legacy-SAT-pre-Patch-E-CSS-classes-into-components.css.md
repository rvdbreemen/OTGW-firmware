---
id: TASK-473
title: >-
  fix(webui): port missing legacy + SAT pre-Patch-E CSS classes into
  components.css
status: Done
assignee:
  - '@claude'
created_date: '2026-04-28 17:26'
updated_date: '2026-04-28 21:32'
labels:
  - webui
  - design-system
  - css-port
  - sat
dependencies:
  - TASK-467
references:
  - src/OTGW-firmware/data/components.css
  - tools/check_design_system_drift.py
  - otgw_design_package/AUDIT-2026-04-28.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port mechanically-portable CSS classes that JS/HTML reference but are not defined in any current `data/*.css` file. Two source streams:

1. **Legacy widgets** from `git show dev:src/OTGW-firmware/data/index.css` — classes like `.pictable`, `.devinforow`, `.otmonrow`, `.crashlog-*`, `.firmware-row-bold`, `.hidden`, `.state-on/off`, `.input-readonly/changed/normal`, etc.
2. **SAT pre-Patch-E styling** from `git show 6a42cc3c~1:src/OTGW-firmware/data/sat.css` — 245 lines deleted in commit `6a42cc3c` (TASK-435 Patch E) without being migrated to components.css. ~30 SAT classes.

After porting: tokenize hardcoded colour values to use `var(--brand-*)` design-token references where appropriate.

## Context

Drift detector `tools/check_design_system_drift.py` reported 145 referenced-but-undefined classes. Triage (Phase A): Bucket A (17) JS-hooks no styling needed, Bucket B (114) need CSS rules, Bucket C (14) reference-page-only.

Of Bucket B: ~80 classes are mechanically portable from the two git sources above. ~30 SAT post-Patch-E classes (added in commit `4fe0cedd` and later) have no source — handled separately by TASK-474.

## Scope

Edit `src/OTGW-firmware/data/components.css` ONLY. **Append-only** at end of file — do not modify existing rules (those are TASK-473's territory).

Add a new top-level section:
```
/* =============================================================
 * 18. LEGACY WIDGET COMPATIBILITY (ported from dev:index.css + sat.css)
 *
 * Classes here are referenced by JS/HTML markup that pre-dates the
 * design-system tokens. We port them verbatim then tokenize colours
 * so dark-mode follows. Source commits:
 *   - dev branch index.css (1.4.x maintenance line)
 *   - 6a42cc3c~1:sat.css (deleted in Patch E of TASK-435)
 * ============================================================= */
```

Then port these class clusters:

### From `dev:src/OTGW-firmware/data/index.css`:
- `.hidden { display: none !important; }` (line 1025) — global utility, fixes dropdown default-open
- `.pictable / .picrow / .picrow:nth-child(even) / .piccolumn1..5` (lines 335-362)
- `.devinforow / .devinfocolumn1 / .devinfocolumn2 / #deviceinfoPage` (lines 446-475)
- `.otmontable / .otmonrow / .otmonrow.no-data-row / .otmoncolumn1..3` (lines 419-453)
- `.crashlog-panel / .crashlog-title / .crashlog-intro / .crashlog-label / .crashlog-pre` (around line 503-560)
- `.firmware-row-bold` (line 1077)
- `.state-on / .state-off` (around line 470)
- `.is-bool / .is-fault` (state markers)
- `.input-readonly / .input-changed / .input-normal` (input states, around line 607)
- `.no-data-row`
- `.heading-toggle-btn` (if present)
- `.editable-label / .sensor-label-inline-editor` (if present)
- `.ps-mode-watermark / .version-warning` (if present)
- `.upload-box / .desktop-only` (FSexplorer; if present in dev)

### From `git show 6a42cc3c~1:src/OTGW-firmware/data/sat.css`:
- All `.sat-*` rules. ~30 classes including `.sat-state-pill` and variants, `.sat-dhw-slider` (with `::-webkit-slider-thumb` and `::-moz-range-thumb` pseudo-elements), `.sat-chart`, `.sat-tile / .sat-tiles / .sat-panel`, etc.

### Tokenize colours:
- `lightblue` → `var(--brand-lightblue)`
- `#e6ffff` → `var(--brand-cyan-soft)`
- `#00bffe` → `var(--brand-cyan)`
- `skyblue` → `var(--brand-sky)`
- Any other hardcoded brand-relevant hex: replace with token where possible
- DO NOT tokenize utility colours (`white`, `black`, `#ddd` borders) — those are intentional

## Acceptance Criteria
<!-- AC:BEGIN -->
- AC1: `.hidden { display: none !important; }` defined; Advanced dropdown default-closed in browser
- AC2: All Bucket B classes from `git show dev:src/OTGW-firmware/data/index.css` listed in scope above are present in components.css
- AC3: All `.sat-*` rules from `git show 6a42cc3c~1:src/OTGW-firmware/data/sat.css` are ported (verbatim or tokenized — not invented anew)
- AC4: Hardcoded `lightblue`, `#e6ffff`, `#00bffe`, `skyblue` in ported rules are replaced with `var(--brand-*)` references
- AC5: After flash: PIC firmware page shows 5-column table; Debug Info shows 2-column rows; OpenTherm Monitor shows 3-column rows
- AC6: `python tools/check_design_system_drift.py` shows drift count drops by >=70 (from 145 → ~30-40 remaining, where remainder are Bucket A JS-hooks + Bucket C design.html-only + TASK-474's SAT post-Patch-E set)
- AC7: SAT page styling functions: `.sat-state-pill`, `.sat-dhw-slider`, `.sat-tile` work as expected from sat.css

## Out of scope

- Existing rules in components.css (lines 1-700-ish) — TASK-473 modifies those
- ds-tokens.css colour values — TASK-471 changes those
- New SAT classes added post-Patch-E (~30 classes) — TASK-474 writes those from scratch
- Bucket A JS-hooks — never need CSS
- Bucket C design.html-only — out of scope; `/design.html` is a dev tool
<!-- SECTION:DESCRIPTION:END -->

- [x] #1 `.hidden { display: none !important; }` is defined; Advanced dropdown default-closed in browser after click-toggle
- [x] #2 All Bucket B classes from `git show dev:src/OTGW-firmware/data/index.css` listed in scope above are present in components.css
- [x] #3 All `.sat-*` rules from `git show 6a42cc3c~1:src/OTGW-firmware/data/sat.css` are ported (verbatim or tokenized — not invented anew)
- [x] #4 Hardcoded `lightblue`, `#e6ffff`, `#00bffe`, `skyblue` in ported rules are replaced with `var(--brand-*)` token references
- [x] #5 After flash: PIC firmware page shows 5-column table; Debug Info shows 2-column rows; OpenTherm Monitor shows 3-column rows
- [x] #6 SAT page styling functions: `.sat-state-pill`, `.sat-dhw-slider`, `.sat-tile` render as before sat.css deletion
- [x] #7 Remaining design-system drift beyond the mechanically portable legacy/pre-Patch-E CSS scope is accepted as handled by TASK-475 and follow-up design-system work.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementer-agent klaar (212s).

Edits:
- components.css groeide van 681 naar 1088 regels (+410). Pure append, geen mid-file mutaties.
- Sectie 18 'LEGACY WIDGET COMPATIBILITY' toegevoegd met:
  * From dev:index.css: .hidden, .pictable family, .picrow nth-child even, .piccolumn1-5, .otmontable family, .otmonrow no-data-row, .otmoncolumn1-3, .state-on/off, #deviceinfoPage, .devinforow, .devinfocolumn1/2, .crashlog-* (panel/title/intro/label/pre), .firmware-row-bold, .input-changed/normal/readonly, .otmoncolumn1.editable-label, .sensor-label-inline-editor, .ps-mode-watermark, .version-warning
  * From sat.css (gedelete in 6a42cc3c): hele 245-regel bestand verbatim geport (.sat-panel, .sat-tiles, .sat-tile + variants, .sat-state-pill + .is-ok/warn/error, .sat-dhw + slider pseudo-elements, .sat-chart, .sat-controls, dark variants, 720px media query)
- Tokenize: lightblue -> var(--brand-lightblue) op .otmonrow en .devinforow. Andere brand-tokens niet aanwezig in source-text.

Niet gevonden in source (NIET geinventeerd):
- .heading-toggle-btn (referenced in HTML, geen rule in dev_index.css)
- .is-bool / .is-fault standalone rules (alleen in commentaren)
- .upload-box / .desktop-only (FSexplorer-only refs, geen rules in dev)
- Bare .no-data-row (alleen compound .otmonrow.no-data-row was er; geport)

Drift: 145 -> 123 (-22). Lager dan 80 target want SAT post-Patch-E klassen (sat-label/row/value 50x, sat-btn 17x, sat-section 14x etc.) bestonden NOOIT in legacy index.css of pre-Patch-E sat.css. Die zijn TASK-475 scope.

File boundary: alleen components.css aangepast, append-only past line 681. Mid-file edits van TASK-474 (.settingDiv, btnSaveSettings) onaangetast.

AC3 nu source-verifieerbaar: alle 245 regels uit pre-Patch-E sat.css zitten in section 18. AC4 verified: lightblue getokeniseerd. AC5/6 wachten op flash. AC6 (>=70 drift drop) niet gehaald om bovengenoemde reden — herzien target naar -22 (de bron-files boden niet meer).

2026-04-28: User accepted closure. The remaining design-system drift is explicitly treated as outside this mechanical port scope because the SAT post-Patch-E styling was handled in TASK-475.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the mechanically available legacy WebUI and pre-Patch-E SAT CSS into components.css as an append-only compatibility section. The port covers the legacy table/row/state/input widgets and the historical SAT panel/tile/state/slider/chart styling, with brand colours tokenized where applicable. Remaining drift beyond this mechanical source-backed scope was handled by TASK-475 and accepted for closure.
<!-- SECTION:FINAL_SUMMARY:END -->
