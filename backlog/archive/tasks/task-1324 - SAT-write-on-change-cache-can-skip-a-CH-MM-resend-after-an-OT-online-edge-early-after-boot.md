---
id: TASK-1324
title: >-
  SAT write-on-change cache can skip a CH/MM resend after an OT online edge
  early after boot
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T15:57:00Z'
updated_date: '2026-10-23T23:16:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 447000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1316 review (pre-existing): satResetCmdCache() zeroes iLastSentCHMs but keeps iLastSentCH; within the same tick satEnqueueIfChangedCS sets the shared bLastSentValid=true before the CH helper runs. With uptime under SAT_CMD_REFRESH_MS (5 min) the CH value equal to the cached one is not 'stale' and is skipped, e.g. CH=1 after an OT offline->online edge shortly after boot, although the reset meant to force a resend. Fix: per-field valid flags, or set the *Ms fields to now - SAT_CMD_REFRESH_MS on reset.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test reproduces the skipped resend after a cache reset early after boot (red), fix sends it (green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: satResetCmdCache() now sets iLastSent{CS,MM,CH,TC}Ms to millis()-SAT_CMD_REFRESH_MS so every command is stale after a reset (unsigned wrap safe). tests/test_sat_cmd_cache.py compiles the real helpers cut from SATcontrol.ino: HEAD copy 'FAILED (failures=3)' (early boot, millis()=0, across the uint32 wrap: 'CS=40.0;' != 'CS=40.0;MM=80;CH=1;'), fix 'Ran 4 tests OK'; test_sat_pwm_override 63 OK; hosttest_ch 0 failures; evaluate 0 fail; combined build SUCCESS; bench unit runs the working tree build (no boiler: resend not observable on the bus). Review (opus): no defect; readers of the *Ms fields are only the staleness checks and satPicCSKeepalive (gated on bLastSentValid), nothing exposes them; values sent unchanged, entries 5 and 9 agree. Not a SAT control change (transport cache).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A SAT command-cache reset now really forces the next CS/MM/CH/TC resend, also within the first 5 minutes of uptime and across the millis() wrap. Evidence: AC1 host test of the real helpers red on HEAD (3 fails) / green. Not observed on a boiler.
<!-- SECTION:FINAL_SUMMARY:END -->
