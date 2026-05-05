---
id: TASK-546
title: 'feat-2.0.0: port TASK-534 DHW climate discovery initial fallback removal'
status: Done
assignee:
  - '@codex'
created_date: '2026-05-05 12:21'
updated_date: '2026-05-05 12:38'
labels:
  - bug
  - feature-dev-2.0.0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the TASK-534 Home Assistant MQTT discovery fix to the feature-dev-2.0.0 branch. The DHW climate discovery must not advertise a hardcoded 43 C initial target temperature that HA can display as if it were a real boiler setpoint when TdhwSet has not arrived yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DHW climate discovery in the 2.0.0 worktree no longer includes the hardcoded 43 C initial target temperature
- [x] #2 DHW climate discovery still points current temperature to Tdhw and target temperature state to TdhwSet
- [x] #3 2.0.0 worktree build or equivalent compile verification is run and documented
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Apply the TASK-534 MQTT discovery change in the feature-dev-2.0.0 worktree.
2. Verify DHW climate discovery still references Tdhw and TdhwSet state topics.
3. Build or run the available compile check in the 2.0.0 worktree.
4. Update acceptance criteria, notes, and final summary.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-05: Applied the TASK-534 port in the feature-dev-2.0.0 worktree by removing the DHW climate discovery "initial":"43" fallback from src/OTGW-firmware/MQTTHaDiscovery.cpp. Verified by rg that no initial 43 remains and that DHW climate still references /Tdhw and /TdhwSet. Ran make in the 2.0.0 worktree; build completed successfully. Arduino CLI emitted the existing low-memory warning: global variables use 69624 bytes (84%), leaving 12296 bytes.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the TASK-534 DHW MQTT discovery fix to the feature-dev-2.0.0 worktree. DHW climate discovery no longer publishes the hardcoded 43 C initial target temperature, while the live current/target state topics remain Tdhw and TdhwSet.

Verification:
- rg confirmed the 43 C initial fallback is absent and /Tdhw plus /TdhwSet remain configured.
- make completed successfully in the 2.0.0 worktree. The build reports the existing low-memory warning at 84% global RAM use.
<!-- SECTION:FINAL_SUMMARY:END -->
