---
id: TASK-1927
title: >-
  SAT self-tune signal: replay gruijter field captures through the real
  exclusion checks and the proposed options (4, 3, 1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T08:21:00Z'
updated_date: '2026-11-14T08:23:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 987000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, #dev-sat-mqtt 2026-11-14: 'I agree. Let's try and improve this.' (https://discord.com/channels/812969634638725140/1105556725714649128/1571075495091511307) and 'Are there other runs you can use?' (1571075689166278717), after our options post 1571074438248661063 / 1571074445026525275. Field runs: capture-gruijter-1111 (beta.171, 20 tuner hours, 0 counted), capture-gruijter-20261113-pm (beta.218, 17 hours, 0 counted), graph CSVs of 9-13 Nov. Options: (4) keep the current day's partial counters across a restart (today not kept, SATcontrol.ino:6505); (3) thermo-nova solar-gain event (solar_gain.py:80-102) instead of the 12 deg sun gate (:6297, entry 161); (1) min-mod ticks with the room below target may vote raise only (:6301, entry 174). Analysis only, no firmware change; any change to the checks is Sergeant D's call afterwards.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The real satTuneTickReason() is compiled on the host from src/OTGW-firmware/SATcontrol.ino (no re-implementation) and reproduces the logged SATTUNEH reasons of both transcripts hour by hour
- [ ] #2 Per run a table: hours counted today and per option (4, 3, 1, and combined), labelled FIELD; the graph CSV runs labelled as replays of the checks without tuner lines
- [ ] #3 Inputs the logs do not contain (for example sun elevation, solar-gain state) are named with the substitute used, or the option is reported as not checkable on that run
- [ ] #4 Results posted to Robert in #dev-sat-mqtt with the evidence path; nothing is changed in the firmware
<!-- AC:END -->
