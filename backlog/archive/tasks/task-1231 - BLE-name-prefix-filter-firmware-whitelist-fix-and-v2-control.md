---
id: TASK-1231
title: 'BLE name-prefix filter: firmware whitelist fix and v2 control'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:09'
updated_date: '2026-10-15 14:06'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: low
ordinal: 369000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md row 51: the BLE name-prefix and restrict-ingest keys are not in knownSettings (restAPI.ino:4200), so a POST returns 400 even from classic. Fix the firmware first, then add the v2 control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 POST of the BLE filter keys is accepted and the values are exposed in GET settings (host or bench transcript)
- [x] #2 v2 has controls for the filter and the restrict-ingest toggle
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-15 firmware fix: restAPI.ino knownSettings[] gains satblenamefilteringest and satblenameprefix, and the GET emits both (addStr maxlen 23, addBool). Old-vs-fix on the OTGW32 bench: before (alpha.425+a6eb16c) both POSTs answered 400 'Unknown setting name' and the GET did not show them (.tmp/bench-1231/old.txt); after (alpha.425+aaeade1 plus fix, flashed with --update on the safeboot layout) both answer 200, the GET shows value/type, and they restored to ''/false (.tmp/bench-1231/fix.txt). AC2 (v2 control) follows after the concurrent v2.js work.

Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: the prefix field and ingest toggle are in Settings > Sensors > BLE sensors. Saving 'ATC' posted satblenameprefix, then /sat/ble/rescan, and GET showed it; restored to ''/false.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
BLE name-prefix filter works again: the firmware accepts and exposes satblenameprefix/satblenamefilteringest (was 400), and v2 has controls that save and rescan. Evidence: bench old 400 / fix 200, v2 bench run.
<!-- SECTION:FINAL_SUMMARY:END -->
