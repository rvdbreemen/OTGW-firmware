---
id: TASK-3108
title: >-
  Tester request: keep the Monitor graph history across a firmware upgrade
  (indigo_light, #alpha-testing 1575559464507936870)
status: To Do
assignee: []
created_date: '2026-11-26T17:11:00Z'
labels:
  - needs-maintainer-ok
  - tester-request
  - webui
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1032000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester request, input only (needs-maintainer-ok): indigo_light asks to retain graph data when loading new firmware. Facts from origin/dev: the Monitor graph history is RAM only (ADR-193; ADR-245: 7 days on PSRAM boards, 5046 slots of 120 s, a reboot empties it); ADR-245 line 150 lists 'a flash copy every 15 minutes' as Option B, not chosen, no reason recorded. A firmware or filesystem OTA therefore empties the graph today. ADR-233 shows a precedent for keeping small state across a filesystem OTA (RTC snapshot), but the ring is about 70 KB, too large for RTC slow memory; a flash copy before the OTA restart is a candidate, flash wear and the filesystem OTA overwriting LittleFS are open questions. Reply posted by the communication officer: noted, no promise.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Maintainer decision recorded: do we want it (yes or no) and for which boards (PSRAM only?)
- [ ] #2 If yes: options compared with measured sizes and flash writes per upgrade (RTC snapshot not possible at about 70 KB; flash copy at OTA time vs every 15 min), with an ADR amending ADR-245
- [ ] #3 Implementation with a host test and a bench check that the graph survives an app OTA and a filesystem OTA (or the documented limit)
<!-- AC:END -->
