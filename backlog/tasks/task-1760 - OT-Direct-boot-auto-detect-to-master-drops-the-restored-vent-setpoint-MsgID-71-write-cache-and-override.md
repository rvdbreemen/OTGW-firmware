---
id: TASK-1760
title: >-
  OT-Direct: boot auto-detect to master drops the restored vent setpoint (MsgID
  71 write cache and override)
status: To Do
assignee: []
created_date: '2026-11-09T18:35:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 857000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1752 review 2026-11-09 (code reading, not reproduced): initOTDirect() restores the vent setpoint via updateWriteCache(71) and setOverride(71) (~OTDirect.ino:920), then the no-thermostat auto-detect calls applyOTDirectMode(OTD_MODE_MASTER) (~1076), whose resetTransientState() clears all write caches (valueSet=false) and otOverrides. So on a board without a thermostat the persisted vent setpoint is not written after boot. Needs a host test on the real initOTDirect/resetTransientState slices, and a check of which other boot restores are lost the same way.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code shows whether the restored MsgID 71 write survives the boot auto-detect to master (red on current code if lost)
- [ ] #2 If lost: the restore runs after the mode is final; test green
<!-- AC:END -->
