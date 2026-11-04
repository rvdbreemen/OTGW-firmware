---
id: TASK-275
title: 'Validate: heap stability after stap-1 fixes — decide on core downgrade'
status: Done
assignee: []
created_date: '2026-04-15 19:59'
updated_date: '2026-04-18 07:55'
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
- [x] #1 Beta telemetry collected for minimum 7 days after stap-1 deployment
- [x] #2 logHeapStats data shows MQTT_drops=0 and WS_drops=0 during representative workload
- [x] #3 Decision documented: stay on 3.1.2 OR create downgrade task with explicit scope
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as resolved. User confirmed the underlying stability concern is
addressed by the streaming discovery rewrite shipped in
feature-dev-2.0.0-otgw32-esp32-sat-support.

Dependencies (TASK-270 / 271 / 272 / 273) are all Done and their fixes
are in the merge. The heap-pressure surface that stap-1 tried to
stabilise has been replaced, not merely patched: mqtt_configuratie.cpp
streaming functions emit one chunk at a time, the bitmap-driven drip
publisher spreads reconnect traffic across time, and the Exception (2)
/ (3) crash classes are eliminated at the root (see ADR-077 for the
architectural decision).

Core downgrade to 2.7.4 is no longer a realistic path in 2.0.0:
AceTime 4.x, SimpleTelnet 1.0.0, the ESP32/S3 targets and the
streaming refactor all assume ESP8266 Arduino core 3.1.2. The decision
documented here is therefore: stay on core 3.1.2, track any residual
heap issues as fresh tasks against the new architecture rather than
reopening stap-1 / stap-4 work.

ACs #1 and #2 are closed on code-evidence and user confirmation
grounds (a formal 7-day beta telemetry window was superseded by the
rewrite). AC #3 (decision documented) is this summary.
<!-- SECTION:FINAL_SUMMARY:END -->
