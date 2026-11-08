---
id: TASK-1722
title: >-
  Measure the heap timeline during web load and during an MQTT burst outside
  boot (follow-up of TASK-1697)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T08:57:00Z'
updated_date: '2026-11-08T10:47:00Z'
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
- [x] #1 Two-tab load with the 1 Hz sampler on .61 (5 runs): free-heap low point timed against the load, min free heap and max block per run
- [x] #2 MQTT reconnect and HA birth on .61 with the sampler: heap low point, hd_mqtt_queue_hwm and hd_mqtt_drops before/after; settings diff none
- [x] #3 Findings reported to the coordinator; a follow-up task only if a dip below the ADR-030 5120 B level shows up
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 dry run on .63 (beta.130, MQTT reconnect via mqttenable false->true): the 1 Hz sampler's low point was 53860 B while hd_min_free_heap fell 25140 -> 22968 B in the same window, so the dip lasts well under a second and a 1 Hz REST sampler misses it. Method for the .61 runs: attribute a dip to an event by the hd_min_free_heap delta across the event window (it only moves on a new low), and use the sampler for how long free heap and max block stay down (max block min 18420 B in this dry run). Scripts: twotab_dev.py --sample, mqtt_burst.py (birth | reconnect) in the session tmp. The birth event is not dry-run on .63: it republishes every OTGW on the bench broker, including .61 while others bench it.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Bench .61 beta.143+0f8ffd4, 2026-11-08 ~12:40-13:00. AC1: two-tab load with a 1 Hz freeheap/maxfreeblock sampler, 5 runs (reboot each): 500/500 HTTP 200, 0 timeouts, device web counters 0. Sampler low point during the load 46.2-48.9 KB, while hd_min_free_heap was 21-27 KB: the dips are sub-second. r4 hd_min 25312->25312 across the load (low came at boot, before the load); r5 35232->26992 (the load made an 8 KB deeper low the sampler did not see). Max block min 24.5-31.7 KB. AC2: MQTT reconnect (mqttenable false->true): hd_min 26992->17496 (deepest seen), sampler 62.8 KB at the event, hd_mqtt_queue_hwm 17->17, hd_mqtt_drops 0->0. HA birth (homeassistant/status online on the bench broker): hd_min 17496->16808, queue hwm 17, drops 0. Settings diff none after all runs. AC3: reported to the coordinator (6b); lowest seen 16.8 KB, far above the ADR-030 5120 B level, no drops, so no follow-up task. Method note: a 1 Hz REST sampler misses these dips; attribute them via the hd_min_free_heap delta across an event window. Not measured: a reconnect stacked on a web load. Scripts: twotab_dev.py --sample, twotab_ab.sh, mqtt_burst.py (session tmp).
<!-- SECTION:FINAL_SUMMARY:END -->
