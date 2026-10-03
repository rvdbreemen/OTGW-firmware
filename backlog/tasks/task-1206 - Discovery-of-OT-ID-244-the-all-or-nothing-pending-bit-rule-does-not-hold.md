---
id: TASK-1206
title: 'Discovery of OT ID 244: the all-or-nothing pending-bit rule does not hold'
status: To Do
assignee: []
created_date: '2026-10-03 17:55'
labels:
  - ha-discovery
  - mqtt
dependencies: []
priority: low
ordinal: 328000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier. The comment at MQTTstuff.ino:2764-2767 says the pending bit for ID 244 clears only when all nine configs published. But the two ID-244 binary-sensor rows (MQTTHaDiscovery.cpp:1459-1460) set result=true in the binary section (MQTTstuff.ino:2697) before the piccontrols block (:2768-2777), which can only add true. A failed Reset Gateway button or GPIO/LED select publish is therefore never retried by the drip.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Either the rule holds (the pending bit stays set until every ID-244 config published, shown by a host test with an injected publish failure) or the comment is corrected to describe the actual rule
- [ ] #2 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
