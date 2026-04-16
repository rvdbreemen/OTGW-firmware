---
id: TASK-275
title: 'Validate: heap stability after stap-1 fixes — decide on core downgrade'
status: To Do
assignee: []
created_date: '2026-04-15 19:59'
labels:
  - validation
  - stability
  - stap-4
dependencies:
  - TASK-270
  - TASK-272
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After stap-1 (TASK-270/271/272) and stap-2 (TASK-273) are deployed to beta testers, collect telemetry for one week and evaluate whether the remaining heap fragmentation still causes observable problems.

Evaluation criteria:
- Are MQTT_drops and WS_drops in logHeapStats structurally zero (or near-zero) during normal operation?
- Does heap level stay HEALTHY during PS=1 mode with an active WebSocket client?
- Are there any new reboot-loop reports from beta testers?

If YES to all: stap-4 is complete, core stays on 3.1.2. No downgrade needed.

If NO (drops persist or stability issues remain): escalate to core 2.7.4 downgrade. Key risks to assess before downgrade:
- AceTime 4.1.0 compatibility with GCC 4.8.x / C++11: needs test build
- If AceTime 4.x is incompatible: evaluate downgrading AceTime to 2.x (was in use before the core upgrade sprint)
- All other libraries (SimpleTelnet, WebSockets, PubSubClient): confirmed compatible with 2.7.4

Decision point: this task is Done when either (a) beta confirms stability on 3.1.2 or (b) the decision to downgrade is made and a downgrade task is created.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Beta telemetry collected for minimum 7 days after stap-1 deployment
- [ ] #2 logHeapStats data shows MQTT_drops=0 and WS_drops=0 during representative workload
- [ ] #3 Decision documented: stay on 3.1.2 OR create downgrade task with explicit scope
<!-- AC:END -->
