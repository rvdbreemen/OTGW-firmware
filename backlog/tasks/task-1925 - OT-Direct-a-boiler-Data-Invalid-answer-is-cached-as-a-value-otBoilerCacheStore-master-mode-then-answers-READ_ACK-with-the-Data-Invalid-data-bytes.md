---
id: TASK-1925
title: >-
  OT-Direct: a boiler Data-Invalid answer is cached as a value
  (otBoilerCacheStore); master mode then answers READ_ACK with the Data-Invalid
  data bytes
status: To Do
assignee: []
created_date: '2026-11-14T08:07:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 986000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1808. otBoilerCacheStore() (OTDirect.ino) clears the slot only for UNKNOWN_DATA_ID; a DATA_INVALID reply stores its data bytes and sets otBoilerCacheValid. handleMasterModeSlaveFrame() then answers a thermostat READ of that id with READ_ACK and those bytes (host check during TASK-1808: boiler B6:5:0000 then thermostat READ 5 gave type 4 data 0000). Every other cache reader (flow temp 25, flame ratio 0, MsgID 3, bounds 48/49, ret slot) sees the same. Spec v4.2 4.3: Data-Invalid carries no valid value. Fixing it changes all those readers, so it is a separate change: decide whether DATA_INVALID clears the slot or keeps the previous value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A boiler Data-Invalid reply no longer becomes a cached value; the chosen rule (clear or keep previous) is named with its consequence for each cache reader
- [ ] #2 Host test against the real code: red on the old code, green on the fix
- [ ] #3 esp32-combo build, evaluate.py --quick, bench where the hardware allows
<!-- AC:END -->
