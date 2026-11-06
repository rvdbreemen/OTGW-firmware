---
id: TASK-1670
title: >-
  v2 Home: boiler schematic shows a fake 'ΔT 14.2°' when flow or return is
  unknown
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-06T06:58:00Z'
updated_date: '2026-11-06T07:20:00Z'
labels:
  - webui
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 793000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the web UI check on .61 (beta.103+18e612a, 2026-11-06): Home > System view shows 'ΔT 14.2°' while FLOW and RETURN show '—' (OT bus offline). Cause: v2.html:295 ships the mockup text 'ΔT 14.2°' in #aDt, and v2.js:1137 only overwrites it when flow and return are both known, so the placeholder (or the last value) stays. The other two ΔT renders (v2.js:1212 bDt, v2.js:1287 table) already fall back to '—'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 #aDt shows 'ΔT —' when flow or return is null (v2.js:1137 uses the same fallback as v2.js:1212), and v2.html:295 no longer ships a sample value
- [ ] #2 Screenshot before/after on a bench unit with the OT bus offline
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06: v2.js:1137 now falls back to 'ΔT —' like :1212; v2.html ships '—' for all 20 live-value elements (aDhw..cnWifiSub; graph axis labels kept, they are scale ticks). New guard tests/webui/no-sample-values.test.mjs reads the real assets: OLD (HEAD 0b091b9bb data via OTGW_DATA_DIR) fail 2/2, FIX pass 2/2. evaluate --quick 84 passed 0 failed; all 38 tests/webui files pass (3 needed a rerun with a longer timeout: headless Chrome slow at high commit charge, also pass on rerun unchanged). Robert 2026-11-06: 'Verzin geen waarden in UI als de temperaturen er niet zijn. Laat dan — zien.'
<!-- SECTION:NOTES:END -->
