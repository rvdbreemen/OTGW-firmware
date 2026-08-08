---
id: TASK-1066
title: >-
  Fix: four vh_* status bits never republish on the 60s heartbeat (StatusVH slot
  starvation)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 12:55'
updated_date: '2026-08-08 13:50'
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
- [x] #1 Root cause confirmed in the StatusVH bit fan-out: identify the exact slot indices used and show the collision, or disprove the collision hypothesis and find the real cause
- [x] #2 All four topics republish on their 60s heartbeat in a steady-state run with no boot and no force
- [x] #3 No other status or statusVH bit changes its publish cadence (compare gate output before and after)
- [x] #4 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [x] #5 Coverage baseline re-recorded deliberately, with the diff reviewed and containing only the four newly-appearing vh_* topics
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause CONFIRMED, and it is a slot collision exactly as hypothesised.
mqttlastsentstatusvhbit[16] documents its own contract in the declaration: "slots 0-7=master, 8-15=slave". publishStatusVHState used 0,1,2,3 for the master bits and publishSlaveStatusVHState ALSO used 0,1,2,3,4,6 for the slave bits. The master fan-out stamps slots 0-3 microseconds before the slave fan-out reads them, so elapsedTrackedSeconds is ~0 and the 60s heartbeat never elapses for the slave bits sharing those slots.
That predicts exactly which topics starve, and the prediction matches the field data: slots 0-3 (vh_fault, vh_ventilation_mode, vh_bypass_status, vh_bypass_automatic_status) never heartbeat, while slots 4 and 6 (vh_free_ventliation_status, vh_diagnostic_indicator) have no master counterpart and kept publishing normally.
Fix: slave bits moved to 8,9,10,11,12,14, matching both the declared contract and the OT_Statusflags fan-out which already uses 8-15 correctly. Verified no duplicate slot remains.
Verified on device via the coverage gate: all four topics now publish in a steady-state run with no boot and no force.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The StatusVH slave bit fan-out reused the master's tracker slots 0-4, violating the contract documented on mqttlastsentstatusvhbit[] ('slots 0-7=master, 8-15=slave') that the OT_Statusflags fan-out already honours. The master stamps those slots microseconds before the slave reads them, so elapsed time is ~0 and the 60s heartbeat never fires. The hypothesis predicted exactly which topics starve and matched the field data: the four bits colliding with master slots published only on first-seen or force, while the two whose slots had no master counterpart kept heartbeating. Slave bits moved to 8-14, no duplicate slots remain. Verified on hardware: all four topics publish in a steady-state run with no boot and no force.
<!-- SECTION:FINAL_SUMMARY:END -->
