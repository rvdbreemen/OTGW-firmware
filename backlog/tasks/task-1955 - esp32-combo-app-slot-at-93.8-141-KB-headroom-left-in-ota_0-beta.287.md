---
id: TASK-1955
title: 'esp32-combo app slot at 93.8 %: 141 KB headroom left in ota_0 (beta.287)'
status: To Do
assignee: []
created_date: '2026-11-15T03:20:00Z'
updated_date: '2026-11-15T13:49:00Z'
labels:
  - build
  - flash-size
dependencies: []
priority: low
ordinal: 997000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Measured on beta.287+9d1510b (build log, 2026-11-15): Flash 2152623 of 2293760 bytes (93.8 %), 141137 B free in the single app slot ota_0 (0x100000, 2240K, ADR-191 safeboot layout). TASK-1639 (GitHub #694) did the size breakdown at 93.5 % (beta.89) and Robert chose only the siscanf reduction then. OTA of 287 on .61 worked (09, changed: none). Safeboot updater writes ota_0 from the factory partition, so the limit is the ota_0 size, not twice the image. Growth since beta.89: +0.3 points. Purpose: keep the headroom visible per beta so a feature that needs more than the remaining ~141 KB is planned (reduction candidates of TASK-1639) instead of found at build time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A table of app size per beta from the build logs since beta.89 (at least 5 points) with the free bytes in ota_0
- [ ] #2 A warning threshold proposed to Robert (e.g. build.py WARN above 95 %), with the consequence of each option
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Coordinator decision (2026-11-15): do options 1 (built-in replay fixture stored deflated, inflated with the ROM tinfl) and 2 (settings-help.json served gzipped), about 22 KB. Both break ADR-194's 'Must Not: gzip or otherwise compress-encode an asset, in firmware or on LittleFS', so ADR-245 (Proposed, amends ADR-194 for these two assets only) is written; the accept goes to Robert through the coordinator; implementation waits for it (an Accepted ADR binds). Trade-off named in ADR-245: a built-in replay then needs ~23 KB of heap while it runs (11.5 KB output + the tinfl decompressor struct; streaming would need a 32 KB dictionary). Options 3-5 parked with thresholds: (3) pack mqttHaSensors (5-8 KB, HA contract) when the headroom falls below 96 KB; (4) replace WiFiManager + Arduino WebServer by an async portal (30-40 KB, onboarding) when the headroom falls below 64 KB or a feature of more than 40 KB is planned; (5) shorten our debug strings (5-10 KB) only alongside other work in the same files.

ADR number: the amendment is ADR-247, not ADR-245 (245 is reserved for 92's 7-day graph ADR, 246 for ba's TASK-1931; renumbered before the push at the coordinator's request).

ADR-247 Accepted: Robert chose A (Discord DM 1571521326060146782, answer to question 1571520520153735261, relayed by the coordinator). Implementation of options 1+2 follows after the TASK-1966 rebuild (number 323).
<!-- SECTION:NOTES:END -->
