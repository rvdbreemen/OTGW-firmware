---
id: TASK-1229
title: 'v2: device clock and firmware/filesystem mismatch banner'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:08'
updated_date: '2026-10-15 14:06'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 367000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md rows 4 and 7: v2 shows the browser clock instead of device/time, so a wrong NTP/timezone is invisible; classic shows a banner when firmware and filesystem hashes differ (filesystem/hash-check).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 shows the device time from device/time with a local tick
- [x] #2 v2 shows a mismatch banner with a link to /update when filesystem/hash-check reports a mismatch
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: #clock 15:50:10 vs device/time 15:50:11, no '~', ticking. hash-check match=true -> #fsMismatch hidden. The mismatch-banner positive case is covered by the headless harness (fake device), not on the bench.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 shows the device clock (device/time + local tick) and a firmware/filesystem mismatch banner with a link to /update. Evidence: bench clock match, harness for the banner.
<!-- SECTION:FINAL_SUMMARY:END -->
