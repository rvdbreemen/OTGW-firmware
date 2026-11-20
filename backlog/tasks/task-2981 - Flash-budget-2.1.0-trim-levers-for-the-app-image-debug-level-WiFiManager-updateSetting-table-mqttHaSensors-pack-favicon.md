---
id: TASK-2981
title: >-
  Flash budget 2.1.0: trim levers for the app image (debug level, WiFiManager,
  updateSetting table, mqttHaSensors pack, favicon)
status: To Do
assignee: []
created_date: '2026-11-20T17:07:00Z'
updated_date: '2026-11-20T17:26:00Z'
labels:
  - build
  - flash-size
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 1113000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Umbrella for the 2.1.0 app-image trims, from the flash-consumers measurement of beta.404+9604698 (architect, 2026-11-20; same image as beta.401) and TASK-1955's parked options. Estimates, each to be measured before and after: (1) compile-time debug level for our telnet/debug literals: tens of KB of ~134 KB of our string pool, debug output only in debug builds, medium risk; measure the debug share first; (2) async portal instead of WiFiManager + the sync Arduino WebServer: 30-40 KB, high onboarding risk (TASK-1955 option 4); (3) updateSetting() if-else strcasecmp chain (13.5 KB symbol) to a table per the ADR-078 dispatch pattern: 5-8 KB, medium risk; (4) pack mqttHaSensors (11.3 KB, TASK-1955 option 3): 5-8 KB, medium risk (HA contract; combine with UX H1/TASK-2971); (5) favicon from LittleFS: 3.6 KB, low to medium risk. No candidate: cJSON/float sscanf (gc-sections leaves 88 B), NimBLE, HTTPClient, SHA1, mdns, update page. diagRec retirement is ADR-268 (swap with the telemetry recorder), not this task. Each lever becomes its own subtask when picked; uniq_id/topics and the settings contract stay unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each lever picked becomes a subtask with a before/after .ino.bin size on the same commit base and its own tests
- [ ] #2 The sum of the landed trims and the new headroom recorded against the TASK-1993 thresholds (WARN 95 %, FAIL 98 %)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20: lever (1), the compile-time debug level, moved to TASK-2980 (2.0.0, Robert's A1 trim task, together with the stable measurement, DER roots and the debug port as its own feature). Levers (2)-(5) stay here for 2.1.0.

Condition for an mbedTLS trim (otgw-dev, relayed by otgw-poc-dev 2026-11-20): not before the 2.0.0 stable. The WiFi supplicant (WPA2, WPA3-SAE) and the OTA sha256 use mbedTLS, and a hybrid IDF rebuild means a long build in release week. After the stable it is a separate subtask, with its own ADR, measured bytes before and after, and a bench WiFi matrix (WPA2 / WPA3 / mixed) on .61 and .63. Debug gate: it stays inline. Moving the flag check out of line would evaluate the arguments while debug is off. The BOL+printf merge per call site (about 4-8 KB) is already on otgw-poc-dev's lane (-5.5 KB measured), so little is left for the debug-level lever in TASK-2980. Gate change: TASK-2983.
<!-- SECTION:NOTES:END -->
