---
id: TASK-1066
title: >-
  Fix: four vh_* status bits never republish on the 60s heartbeat (StatusVH slot
  starvation)
status: To Do
assignee: []
created_date: '2026-08-08 12:55'
labels:
  - bug
  - mqtt
dependencies: []
priority: medium
ordinal: 176000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1065 coverage gate on v1.7.3-beta.3, and independently reported by the TASK-1063 validation review. In a steady-state 700s run the StatusVH byte-level heartbeat fires normally (status_vh_master and status_vh_slave both log 'publish[interval]') and most vh_* bit topics publish, but four never do: vh_bypass_automatic_status, vh_bypass_status, vh_fault and vh_ventilation_mode. In the 20-minute capture taken across a boot they DID publish, at 12:08:05, via a [force] event rather than a heartbeat. So these four appear to reach a consumer only on first-seen or on a force, never on their 60s interval. Suspected cause per the review: a slot-index collision in the per-bit tracker (mqttlastsentstatusvhbit) so these four share slots with other bits and their interval is continuously reset. Pre-existing, not a beta.3 regression. Note the coverage baseline is recorded from a steady-state run and therefore does NOT contain these four topics; fixing this will change the baseline and it must be re-recorded deliberately.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause confirmed in the StatusVH bit fan-out: identify the exact slot indices used and show the collision, or disprove the collision hypothesis and find the real cause
- [ ] #2 All four topics republish on their 60s heartbeat in a steady-state run with no boot and no force
- [ ] #3 No other status or statusVH bit changes its publish cadence (compare gate output before and after)
- [ ] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [ ] #5 Coverage baseline re-recorded deliberately, with the diff reviewed and containing only the four newly-appearing vh_* topics
<!-- AC:END -->
