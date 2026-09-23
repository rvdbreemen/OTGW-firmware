---
id: TASK-1160
title: App-only USB flash resets the SAT section of settings.ini to compiled defaults
status: To Do
assignee: []
created_date: '2026-09-23 18:15'
labels:
  - settings
  - flash
  - bug
dependencies: []
priority: high
ordinal: 292000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reproduced twice on the OTGW32 bench 2026-09-23: after flash_otgw.bat --update --app (alpha.372), every SAT key in /settings.ini is back at its compiled default (e.g. satsensormaxage 120 -> 21600, satenabled true -> false) while OTD and MQTT keys in the same file survive, and the file itself is rewritten. A plain RTS reset (esptool read_mac) does NOT reproduce it (marker survived). This is the sanctioned firmware update path on 4MB boards. Root cause unknown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause identified with on-device evidence that separates 'SAT keys not applied at parse' from 'applied, then reset and flushed'
- [ ] #2 An app-only flash preserves every settings key, verified on the bench with a marker value
- [ ] #3 If testers are affected before the fix ships, the alpha channel gets a warning
<!-- AC:END -->
