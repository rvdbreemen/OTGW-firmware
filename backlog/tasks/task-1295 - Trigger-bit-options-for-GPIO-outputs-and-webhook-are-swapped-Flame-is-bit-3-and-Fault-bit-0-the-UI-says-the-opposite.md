---
id: TASK-1295
title: >-
  Trigger-bit options for GPIO outputs and webhook are swapped: Flame is bit 3
  and Fault bit 0, the UI says the opposite
status: To Do
assignee: []
created_date: '2026-10-22T07:40:00Z'
labels:
  - webui
  - gpio
  - webhook
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 420000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1281 review (2026-10-22), pre-existing. OTcurrentSystemState.Statusflags = (MasterStatus<<8)|SlaveStatus (OTGW-Core.ino:2461); in the slave status byte bit 0 = fault and bit 3 = flame (OT spec MsgID 0 LB). outputs_ext.ino:85 and webhook.ino evalTriggerBit test (1U << iTriggerBit). ENUM_OPTS in data/v2.js (~2743) maps 0 to 'Flame' and 3 to 'Fault' for both gpiooutputstriggerbit and webhooktriggerbit, so a user who picks Flame gets a relay or webhook that follows the fault bit, and the other way round. Decide whether the labels or the bit numbers are wrong (check the 1.x classic UI and docs/api for the documented meaning before changing stored values), fix, and prove it with a replay that raises the flame bit and the fault bit separately.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The option labels match the bit the firmware tests; existing stored values keep their documented meaning or are migrated, with the choice recorded
- [ ] #2 Bench or replay evidence: a frame with only the flame bit set triggers the output/webhook configured as Flame and not the one configured as Fault, and the reverse
- [ ] #3 settings-help.json texts for gpiooutputstriggerbit and webhooktriggerbit name the bits correctly
<!-- AC:END -->
