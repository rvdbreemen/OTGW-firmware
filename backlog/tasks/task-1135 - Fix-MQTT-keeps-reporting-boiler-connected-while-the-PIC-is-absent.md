---
id: TASK-1135
title: 'Fix: MQTT keeps reporting boiler connected while the PIC is absent'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-17 20:21'
updated_date: '2026-09-18 04:46'
labels:
  - bug
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning / tranquil_kiwi_32924 / 2026-09-17'
priority: medium
ordinal: 218000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by tranquil_kiwi_32924 in Discord #nederlandse-ondersteuning, 2026-09-17. On 1.7.4 with PIC 6.7 the web interface reported no PIC present, while MQTT kept publishing that the boiler was connected - with both the boiler and the thermostat physically disconnected from the gateway.

The web-side half of his report (PIC detection never recovering when diagnose or interface firmware is loaded) is already fixed by TASK-1126 and shipped in 1.7.6-beta.2. This task covers the half that is not: connection flags published to MQTT are not retracted when their precondition goes false. Same class as the unsupported_msgids bug from GH #677, whose fix pattern was that contrary evidence retracts an earlier verdict.

Note the reporter has ordered a replacement PIC, so his own device may stop reproducing; the staleness is verifiable on any gateway by pulling the PIC connection.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With no PIC detected, the boiler/thermostat connected topics publish false rather than retaining their last true value
- [ ] #2 The retraction is published, not only held in RAM, so a Home Assistant that reconnects sees the false state
- [ ] #3 Reproduced before the fix and verified after, on a device with the PIC absent
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->
