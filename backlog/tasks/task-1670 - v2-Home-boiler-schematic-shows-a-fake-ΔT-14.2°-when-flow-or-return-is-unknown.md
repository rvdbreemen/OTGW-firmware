---
id: TASK-1670
title: >-
  v2 Home: boiler schematic shows a fake 'ΔT 14.2°' when flow or return is
  unknown
status: Done
assignee:
  - '@claude'
created_date: '2026-11-06T06:58:00Z'
updated_date: '2026-11-06T07:32:00Z'
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
- [x] #2 Screenshot before/after on a bench unit with the OT bus offline
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06: v2.js:1137 now falls back to 'ΔT —' like :1212; v2.html ships '—' for all 20 live-value elements (aDhw..cnWifiSub; graph axis labels kept, they are scale ticks). New guard tests/webui/no-sample-values.test.mjs reads the real assets: OLD (HEAD 0b091b9bb data via OTGW_DATA_DIR) fail 2/2, FIX pass 2/2. evaluate --quick 84 passed 0 failed; all 38 tests/webui files pass (3 needed a rerun with a longer timeout: headless Chrome slow at high commit charge, also pass on rerun unchanged). Robert 2026-11-06: 'Verzin geen waarden in UI als de temperaturen er niet zijn. Laat dan — zien.'

2026-11-06 bench .61: esp32-combo build 2.0.0-beta.104+deff449 (2 images, artefacts 08:29), OTA app+fs exit 0, settings 177 keys changed none. Before (beta.103, ui/Home.png 07:54): 'ΔT 14.2°' next to FLOW/RETURN '—'. After (beta.104, ui/Home-after-1670.png 08:31): aDt 'ΔT —', aFlow/aRet/aOut/aRoom '—', aRoomSet 'set 20.0°' (real SAT target). Pushed, zip posted #alpha-testing 1568166126817648650.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: v2.js:1137 falls back to 'ΔT —' like :1212; v2.html ships '—' for all 20 live-value elements. Guard tests/webui/no-sample-values.test.mjs: OLD fail 2/2, FIX pass 2/2. AC2: screenshots on .61 with the OT bus offline, before beta.103 (fake ΔT 14.2°) and after beta.104+deff449 (ΔT —). evaluate --quick 0 failed; all tests/webui pass.
<!-- SECTION:FINAL_SUMMARY:END -->
