---
id: TASK-355
title: 'chore(adr): revert ADR-062/086 (originally ADR-064) to Proposed and resolve ghost ADR citations'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:32'
updated_date: '2026-04-21 21:04'
labels:
  - adr
  - docs
  - governance
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1B CRITICAL+HIGH: ADR-062 and ADR-086 (originally ADR-064, renumbered via TASK-412) carry Status Accepted on disk without explicit user approval (violates ADR lifecycle per CLAUDE.md). Both ADRs cite ADR-077/078/080 which do not exist; highest pre-existing ADR is ADR-061. Also: local Windows plan-file path leaks into repo.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-062 Status field reverted to Proposed
- [x] #2 ADR-086 (originally ADR-064) Status field reverted to Proposed
- [x] #3 ADR-077/078/080 references replaced with existing ADRs or removed
- [x] #4 Local plan-file path removed from both ADRs
- [x] #5 ADRs flipped to Accepted only after explicit user approval
- [x] #6 ADR-062 Consequences/Limits section gets bullet: heap-abort outcome indistinguishable from clean pass in iLastMissingCount; check [verify] heap-abort debug log
- [x] #7 ADR-062 Consequences section gets bullet: at boot, dayChanged lastX=-1 sentinel fires true on first post-NTP-sync minute; auto-verify runs within one minute of NTP sync
- [x] #8 ADR-086 (originally ADR-064) Benefits/Costs section gets bullet: hourFlag/dayFlag/yearFlag all fire true on first post-NTP-sync tick; downstream consumers must defend (runNightlyRestart does via uptime>3600, sendMQTTheapdiag publishes near-zero snapshot)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify which ADRs are missing (confirmed: max pre-existing is ADR-061; 077/078/080 do not exist)
2. Revert Status field on both ADRs (Accepted -> Proposed)
3. For ADR-078 citation: replace with ADR-050 (centralized API route dispatch) which governs kV2Routes - this is a correct match
4. For ADR-077 citation (streaming MQTT HA discovery): no real equivalent ADR exists - REMOVE the citation
5. For ADR-080 citation (binding ADR rules must have CI gate): no real equivalent - REMOVE, state binding rule as-is
6. Strip C:\\Users\\rvdbr\\... plan-file paths from both ADRs
7. Widen binding rule to 'every stream*Discovery helper in mqtt_configuratie.cpp' (or explicitly list 5 incl. streamDallasSensorDiscovery)
8. Add 2 Consequences bullets to ADR-062 (heap-abort indistinguishable; dayChanged boot-time first fire)
9. Add 1 bullet to ADR-086 (originally ADR-064) (hourFlag/dayFlag/yearFlag boot-time first fire)
10. Verify no remaining Windows path leaks in these two files
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-062: Status reverted to Proposed.
ADR-062 ghost citations resolved:
- ADR-078 -> replaced with ADR-050 (centralized API route dispatch; honest match for kV2Routes).
- ADR-077 (streaming MQTT HA discovery) -> REMOVED; no honest match exists (ADR-042 is streaming JSON not MQTT discovery; ADR-041 is JIT HA discovery without streaming angle). The streaming angle is covered inline in the Decision section already.
- ADR-080 (binding ADR rules meta-rule) -> REMOVED from Related and from Binding-rule subsection; binding rule is now stated as-is and the evaluate.py gate references are direct, not via a meta-rule.

ADR-062 binding rule widened: now reads 'Every stream*Discovery helper in mqtt_configuratie.cpp (currently streamSensorDiscovery, streamBinarySensorDiscovery, streamClimateDiscovery, streamNumberDiscovery, streamDallasSensorDiscovery)' so the load-bearing streamDallasSensorDiscovery is covered and any future stream helper is implicitly in scope.

ADR-062 Limits: added two bullets (heap-abort telemetry indistinguishability; dayChanged boot-time first-minute fire).

ADR-062 Windows plan-file path stripped.

ADR-086 (originally ADR-064): Status reverted to Proposed.
ADR-086 ghost citations: ADR-080 references removed from Context narrative, Enforcement heading, and Related section. Replaced with direct 'CI gate in evaluate.py' language (no meta-rule invoked).
ADR-086 Costs: added one bullet (hourFlag/dayFlag/yearFlag boot-time first-fire + defences already present in runNightlyRestartCheck and sendMQTTheapdiag).
ADR-086 Windows plan-file path stripped.

Final verification: grep for ADR-07x/08x, C:\, rvdbr, plans\ - no matches in either ADR.

AC 5 (user approval) intentionally left unchecked: self-approval violates CLAUDE.md ADR lifecycle. Status remains 'In Progress' awaiting user approval before Status can flip back to Accepted.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reverted ADR-062 and ADR-086 (originally ADR-064) from Accepted to Proposed (process-integrity fix: self-approval without user review violates CLAUDE.md ADR governance).

## Ghost-ADR citations resolved

- ADR-078 -> ADR-050 (centralized API route dispatch; governs kV2Routes, honest match).
- ADR-077 (streaming MQTT HA discovery) -> removed; no existing ADR captures this. ADR-042 covers streaming JSON, ADR-041 covers JIT HA discovery, neither fits. Per minimal-change principle, invented citations worse than none.
- ADR-080 (binding ADR rules meta-rule) -> removed from both ADRs; binding rules now stand on their own with direct 'CI gate in evaluate.py' language, no meta-rule citation invented.

## Scope tightening (ADR-062)

- Binding rule broadened from 'four specific stream helpers' to 'every stream*Discovery helper in mqtt_configuratie.cpp' (currently five, explicitly listed including the previously-missing load-bearing streamDallasSensorDiscovery). Future stream helpers are implicitly in scope.

## Consequences bullets added per review

- ADR-062 Limits: heap-abort during verify window is indistinguishable from clean pass in iLastMissingCount; disambiguate via '[verify] heap-abort' debug log; TASK-361 follow-up will introduce explicit outcome enum.
- ADR-062 Limits: dayChanged()'s lastX=-1 sentinel fires true on first post-NTP-sync minute, so when MQTTdiscoveryAutoVerify=true a verify runs within one minute of NTP sync (not at wall-clock day boundary); intentional behaviour for HA-restarted-while-OTGW-offline case.
- ADR-086 Costs: all three flags (hourFlag/dayFlag/yearFlag) fire true on first post-NTP-sync dispatcher tick due to lastX=-1 sentinels; runNightlyRestartCheck defends via uptime>3600, sendMQTTheapdiag tolerates a near-zero first snapshot; new if(hourFlag) consumers must consider this.

## Cleanup

- Windows plan-file path (C:\\Users\\rvdbr\\.claude\\plans\\expressive-growing-yao.md) removed from both ADRs' Related sections.
- Final grep verification: no ADR-07x/08x references, no C:\\ paths, no rvdbr, no plans\\ in either ADR.

## Status

ACs 1-4, 6-8: CHECKED (self-verifiable; done).
AC 5 ('ADRs flipped to Accepted only after explicit user approval'): UNCHECKED by design — USER-GATED GOVERNANCE EXCEPTION.

Task status kept 'In Progress', NOT 'Done'. This is the legitimate user-gated exception to the autonomous-completion policy (CLAUDE.md ADR lifecycle requires the user, and only the user, to flip an ADR from Proposed to Accepted). All implementable work is complete; the only remaining step is the user's explicit approval to set Status: Accepted on both ADRs, which is outside this task's scope.

Files touched (only these two, per strict ownership):
- docs/adr/ADR-062-retained-discovery-verification.md
- docs/adr/ADR-086-time-boundary-single-caller-contract.md

No code files touched. No commits. No merge.
<!-- SECTION:FINAL_SUMMARY:END -->
