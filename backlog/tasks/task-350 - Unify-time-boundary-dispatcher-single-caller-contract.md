---
id: TASK-350
title: Unify time-boundary dispatcher (single-caller contract)
status: To Do
assignee: []
created_date: '2026-04-20 19:32'
labels:
  - refactor
  - time-boundary
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Behavior-preserving refactor to consolidate hourChanged, dayChanged, yearChanged into a single dispatcher inside doTaskMinuteChanged. Closes the latent consume-on-read bug where adding a second consumer silently steals the event. Moves nightly restart and hourly heapdiag from doTaskEvery60s (boot-relative, drifts) to doTaskMinuteChanged (wall-clock aligned). sendtimecommand signature changes to take dayFlag and yearFlag as parameters. Every helper has EXACTLY ONE call site after refactor, enforced by new evaluate.py gate per ADR-080/ADR-064. See plan file expressive-growing-yao.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 doTaskMinuteChanged captures hourFlag/dayFlag/yearFlag at top via one call each
- [ ] #2 sendtimecommand(bool dayFlag, bool yearFlag) new signature; internal dayChanged/yearChanged calls removed
- [ ] #3 Nightly restart block moved from doTaskEvery60s to if(hourFlag) block in doTaskMinuteChanged
- [ ] #4 sendMQTTheapdiag call moved from doTaskEvery60s to if(hourFlag) block
- [ ] #5 runNightlyRestartCheck helper extracted for readability
- [ ] #6 hourChanged called from EXACTLY ONE location
- [ ] #7 dayChanged called from EXACTLY ONE location
- [ ] #8 yearChanged called from EXACTLY ONE location
- [ ] #9 minuteChanged call site stays at OTGW-firmware.ino:366 with ADR-064 comment anchor
- [ ] #10 evaluate.py check_time_boundary_single_caller added: exactly 1 call site per helper
- [ ] #11 ADR-064 accepted before merge
- [ ] #12 Build passes and evaluate.py 100% including new check
- [ ] #13 Behavior regression: SR=21/SR=22/nightly restart/heapdiag unchanged
<!-- AC:END -->
