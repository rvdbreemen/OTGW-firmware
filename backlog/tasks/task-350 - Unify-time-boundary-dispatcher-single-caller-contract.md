---
id: TASK-350
title: Unify time-boundary dispatcher (single-caller contract)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 19:32'
updated_date: '2026-04-21 17:03'
labels:
  - refactor
  - time-boundary
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Behavior-preserving refactor to consolidate hourChanged, dayChanged, yearChanged into a single dispatcher inside doTaskMinuteChanged. Closes the latent consume-on-read bug where adding a second consumer silently steals the event. Moves nightly restart and hourly heapdiag from doTaskEvery60s (boot-relative, drifts) to doTaskMinuteChanged (wall-clock aligned). sendtimecommand signature changes to take dayFlag and yearFlag as parameters. Every helper has EXACTLY ONE call site after refactor, enforced by new evaluate.py gate per ADR-080/ADR-064.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 doTaskMinuteChanged captures hourFlag/dayFlag/yearFlag at top via one call each
- [x] #2 sendtimecommand(bool dayFlag, bool yearFlag) new signature; internal dayChanged/yearChanged calls removed
- [x] #3 Nightly restart block moved from doTaskEvery60s to if(hourFlag) block in doTaskMinuteChanged
- [x] #4 sendMQTTheapdiag call moved from doTaskEvery60s to if(hourFlag) block
- [x] #5 runNightlyRestartCheck helper extracted for readability
- [x] #6 hourChanged called from EXACTLY ONE location
- [x] #7 dayChanged called from EXACTLY ONE location
- [x] #8 yearChanged called from EXACTLY ONE location
- [x] #9 minuteChanged call site stays at OTGW-firmware.ino:366 with ADR-064 comment anchor
- [x] #10 evaluate.py check_time_boundary_single_caller added: exactly 1 call site per helper
- [x] #11 ADR-064 accepted before merge
- [x] #12 Build passes and evaluate.py 100% including new check
- [x] #13 Behavior regression: SR=21/SR=22/nightly restart/heapdiag unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Lift hourChanged/dayChanged/yearChanged calls from current sites into doTaskMinuteChanged as ONE call each at the top. 2. Change sendtimecommand signature from () to (bool dayFlag, bool yearFlag); remove internal dayChanged/yearChanged calls. 3. Move nightly restart block from doTaskEvery60s to if(hourFlag) in doTaskMinuteChanged; extract runNightlyRestartCheck helper. 4. Move sendMQTTheapdiag call from doTaskEvery60s to if(hourFlag) block. 5. minuteChanged call at OTGW-firmware.ino:366 keeps single call site, add ADR-064 comment anchor. 6. Add evaluate.py check_time_boundary_single_caller: grep all .ino/.cpp/.h, exactly 1 call per helper. 7. Build + evaluate.py + commit + push + close task. ADR-064 already Accepted.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Unified the time-boundary dispatcher per ADR-064. doTaskMinuteChanged now captures hourFlag/dayFlag/yearFlag via exactly ONE call each at the top, and downstream consumers read the flags. sendtimecommand signature changed from () to (bool dayFlag, bool yearFlag) with internal dayChanged/yearChanged calls removed. Nightly restart moved from doTaskEvery60s to if(hourFlag) block as runNightlyRestartCheck helper. sendMQTTheapdiag call moved from doTaskEvery60s to if(hourFlag) block. ADR-064 comment anchor added at all 4 call sites. evaluate.py check_time_boundary_single_caller added as CI gate per ADR-080 meta-rule; 4 new checks (one per helper) all PASS. Build verified clean, evaluate.py 100% (27/27 checks). Behavior preserved: SR=21 on day-flip, SR=22 on year-flip, nightly restart at iRestartHour, hourly heapdiag publish. Wall-clock alignment improved by moving from boot-relative timer60s to minuteChanged-gated path.
<!-- SECTION:FINAL_SUMMARY:END -->
