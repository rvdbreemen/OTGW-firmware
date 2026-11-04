---
id: TASK-1191
title: Correct ADR-176's premise on an unclean reboot lowering the water total
status: Done
assignee:
  - '@claude'
created_date: '2026-10-01 18:26'
updated_date: '2026-10-01 21:42'
labels:
  - 2.0.0
  - adr
  - water-meter
dependencies: []
priority: low
ordinal: 313000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1123 notes, point 1: ADR-176's answered question on partial regression says 'The counter never decreases, so Home Assistant never sees a reset' and 'Same answer as the 1.x peer'. Neither holds: a power cut restores up to 10 L plus one minute of flow less than the last published value (docs/api/MQTT.md, DHW Water Total), and 1.x keeps the total in RAM only, so a reboot zeroes it. The decision itself, resume from the last persisted value and accept the undercount, stands. Maintainer decision 2026-10-01: record the correction in a short amending ADR (docs only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 An amending ADR, authored through adr-kit and Accepted by the maintainer, states what an unclean reboot does to the published total and how Home Assistant reads it, and that the 1.x line zeroes on reboot; ADR-176's decision to resume from the last persisted value stands
- [x] #2 The ADR is cross-referenced with ADR-176 on both sides (adr relate) and listed in the curated ADR index; adr-lint passes every gate
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-01:
- ADR-183 'Record what an unclean reboot does to the persisted DHW water total' was written through adr-kit and accepted by the maintainer after the acceptance packet.
- adr relate links it to ADR-176 on both sides; it is listed in docs/adr/README.md (ADR-176 marked as amended); adr-lint passes every gate; index-check 0 fail over 183 ADRs.
- Facts checked against their sources:
  - Home Assistant's reset_detected() in homeassistant/components/sensor/recorder.py (fetched from home-assistant/core dev): a reset when the new value < 0.9 x the previous one; warn_dip() at 0.9 x or more;
  - the 1.x header 'The total lives in RAM only' (git show origin/otgw-1.x.x:src/OTGW-firmware/dhwWaterMeter.ino);
  - docs/api/MQTT.md Persistence for the under-10-L bound.
- Shipped in commit 0fdb1e594 together with TASK-1190.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Corrects ADR-176's answered question on a partial regression after an unclean reboot, in the amending ADR-183 (Accepted, docs only).

The published total can drop by under 10 L plus one minute of flow. Home Assistant reads that as a dip at 90% or more of the previous value and as a reset below it. The 1.x line starts from 0 after a reboot. ADR-176's decision to resume from the last persisted value stands.

Evidence:
- adr-lint passes every gate; index-check 0 fail.
- Both facts verified in their primary sources: HA recorder.py, and the 1.x dhwWaterMeter.ino.
- Commit 0fdb1e594.
<!-- SECTION:FINAL_SUMMARY:END -->
