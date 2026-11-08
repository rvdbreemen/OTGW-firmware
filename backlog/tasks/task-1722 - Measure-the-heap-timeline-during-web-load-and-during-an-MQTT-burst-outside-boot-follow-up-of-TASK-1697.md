---
id: TASK-1722
title: >-
  Measure the heap timeline during web load and during an MQTT burst outside
  boot (follow-up of TASK-1697)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-08T08:57:00Z'
updated_date: '2026-11-08T09:00:00Z'
labels:
  - mqtt
  - heap
  - measurement
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 831000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1697 closed (Robert 2026-11-08) with two gaps: the two-tab heap minimum is a since-boot value, and MQTT bursts outside boot (broker reconnect, HA birth -> markAll, ADR-174) were not measured. Measurement only: a 1 Hz freeheap/maxfreeblock sampler next to the two-tab load and around an MQTT reconnect and an HA birth on .61 (slot via coordinator 6b); settings restored.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Two-tab load with the 1 Hz sampler on .61 (5 runs): free-heap low point timed against the load, min free heap and max block per run
- [ ] #2 MQTT reconnect and HA birth on .61 with the sampler: heap low point, hd_mqtt_queue_hwm and hd_mqtt_drops before/after; settings diff none
- [ ] #3 Findings reported to the coordinator; a follow-up task only if a dip below the ADR-030 5120 B level shows up
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 dry run on .63 (beta.130, MQTT reconnect via mqttenable false->true): the 1 Hz sampler's low point was 53860 B while hd_min_free_heap fell 25140 -> 22968 B in the same window, so the dip lasts well under a second and a 1 Hz REST sampler misses it. Method for the .61 runs: attribute a dip to an event by the hd_min_free_heap delta across the event window (it only moves on a new low), and use the sampler for how long free heap and max block stay down (max block min 18420 B in this dry run). Scripts: twotab_dev.py --sample, mqtt_burst.py (birth | reconnect) in the session tmp. The birth event is not dry-run on .63: it republishes every OTGW on the bench broker, including .61 while others bench it.
<!-- SECTION:NOTES:END -->
