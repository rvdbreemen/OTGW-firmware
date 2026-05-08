---
id: TASK-592
title: >-
  fix(sat): zone PID integral clamp is positive-only, should be symmetric like
  primary PID
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 17:50'
updated_date: '2026-05-08 17:52'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
satZonePidStep() clamps its integral to [0, curveValue] (SATcontrol.ino:1274-1275). The primary PID was fixed to [-curveValue, +curveValue] in TASK-588 (alpha.28). The zone PID missed the same fix. Without negative integral, a zone that overshoots its setpoint (mild weather) cannot nudge its flow output below the heating curve, mirroring the exact bug TASK-588 fixed for single-zone mode.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Lines 1274-1275 in satZonePidStep() updated to symmetric clamp: if fPidIntegral < -curveValue clamp; if fPidIntegral > curveValue clamp
- [x] #2 Absolute safety cap ±20.0f added to zone integral matching primary PID (SATpid.ino:131-133)
- [x] #3 Build passes (python build.py --firmware exits 0)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Edit SATcontrol.ino satZonePidStep() lines 1274-1275: replace [0, curveValue] with symmetric [-curveValue, +curveValue] + abs cap +-20\n2. Build --firmware to verify\n3. Bump prerelease, commit, push\n4. Mark all ACs checked, add final summary, set Done
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edited SATcontrol.ino:1274-1275. Old clamp: [0, curveValue]. New clamp: [-curveValue, +curveValue] + abs safety cap +-20.0f. Three comment lines added to explain why negative accumulation is needed (zone overshoot correction). Build running in background.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed satZonePidStep() integral clamp from positive-only [0, curveValue] to symmetric [-curveValue, +curveValue] with +-20 hard cap, matching the primary PID fix from TASK-588 (alpha.28).

Changes:
- SATcontrol.ino:1274-1278: replaced 2-line positive-only clamp with 4-line symmetric clamp + absolute safety bounds
- Added 3-comment block explaining why negative accumulation is needed for zone overshoot correction

Build: exit 0 (alpha.30). All 3 ACs verified. Committed 7c3072e4, pushed to origin/feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
