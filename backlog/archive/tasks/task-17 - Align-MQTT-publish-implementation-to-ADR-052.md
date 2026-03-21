---
id: TASK-17
title: Align MQTT publish implementation to ADR-052
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-17 22:19'
updated_date: '2026-03-17 22:29'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bring the current MQTT publish gating implementation and reconnect refresh behavior into explicit alignment with ADR-052. Cover normal message IDs, combined status topics, and per-bit status topics without changing topic names or payload contracts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Normal OpenTherm message IDs follow the ADR-052 first-seen, value-change, or stale-refresh publish rule.
- [x] #2 MQTT reconnect resets first-seen state for all tracked message IDs and all tracked status-bit topics.
- [x] #3 Combined status topics status_master and status_slave follow ADR-052 and do not bypass the publish eligibility contract.
- [x] #4 Per-bit status topics such as flame and centralheating follow ADR-052 consistently on first-seen, bit change, stale refresh, and reconnect.
- [x] #5 Implementation is validated with a firmware build and documented in backlog notes.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Audit the current normal-message, combined-status, and per-bit publish gates against ADR-052 and map every mismatch.
2. Refactor status publish sequencing so no yield can occur before previous-state snapshots, force-republish flags, and publish tracking are consistent.
3. Bring combined status topics status_master and status_slave under the same first-seen, change, stale-refresh, and reconnect-reset contract as the per-bit topics.
4. Verify MQTT reconnect resets publish eligibility for all tracked message IDs and all status-bit slots, then build firmware and document the results.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented ADR-052 publish eligibility alignment in OTGW-Core and MQTT reconnect flow.

Changes:
- Added explicit first-seen tracking for normal MQTT publish slots, status bytes, and per-bit status topics.
- Added requestMQTTRepublishAll() and wired MQTT reconnect to clear all tracked publish eligibility before the next publish cycle.
- Brought status_master and status_slave under dedicated combined-byte eligibility checks instead of unconditional publishes.
- Reordered master/slave status publishing so previous-state snapshots and force flags are stabilized before any publish path can yield.

Validation:
- VS Code error scan reported no errors in OTGW-Core.ino, MQTTstuff.ino, or OTGW-Core.h.
- Firmware build succeeded with python build.py --firmware.
- Final build size: 672428 bytes flash, 59684 bytes RAM globals.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Aligned MQTT publish gating with ADR-052 for normal OpenTherm message IDs, combined status topics, and per-bit status topics.

Changes:
- Added explicit first-seen tracking instead of relying only on last-published timestamps.
- Added full reconnect reset support through requestMQTTRepublishAll() so all tracked topics become eligible again after MQTT reconnect.
- Applied dedicated eligibility gating to status_master and status_slave and kept per-bit status topics on independent tracking.
- Stabilized master/slave status state updates before publish calls to avoid inconsistent behavior across yield windows.

Validation:
- get_errors reported no issues in the touched files.
- python build.py --firmware completed successfully.
<!-- SECTION:FINAL_SUMMARY:END -->
