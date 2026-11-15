---
id: TASK-1781
title: >-
  MQTT: an empty unsupported_msgids at every connect overwrites the retained CSV
  on the broker
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T14:32:00Z'
updated_date: '2026-11-15T07:27:00Z'
labels:
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 877000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of the TASK-1757 analysis (2026-11-10). onMqttConnect calls publishBoilerUnsupportedMsgids() (src/OTGW-firmware/MQTTstuff.ino, post-subscribe block), which publishes the CSV of boiler Unknown-Data-Id msgIDs RETAINED to otgw-firmware/boiler/unsupported_msgids (publishBoilerUnsupportedMsgids). Right after boot the bitmap is still empty, so every connect publishes '' retained and replaces the CSV learned before the reboot; the value is rebuilt only as the boiler answers Unknown-Data-Id again. Question for Robert: is that intended (fresh state per boot) or should the empty map not be published at connect? Priority low: diagnostic topic only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decide with Robert whether an empty map at connect should overwrite the retained CSV
- [ ] #2 If not: publish only once the map is non-empty (or on change); a host test on the real function shows no retained '' at connect with an empty map
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 decision (Robert, Discord DM 1571425442894971032, relayed by the coordinator): "A". Option A = keep the behaviour as it is: otgw-firmware/boiler/unsupported_msgids shows only what this boot has seen, so the empty map published retained at connect after a reboot is intended (fresh state per boot). No code change.

AC2 not applicable: it applies only if the empty map should not be published; Robert chose to keep publishing it. Closed as won't-change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Won't change. AC1: Robert decided A by DM 1571425442894971032 ("A"): keep the retained '' at connect after boot; the topic reflects the current boot only. AC2: not applicable (conditional on the other option). No firmware change, no build, no test needed.
<!-- SECTION:FINAL_SUMMARY:END -->
