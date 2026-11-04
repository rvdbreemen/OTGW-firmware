---
id: TASK-1003
title: >-
  docs(adr): author ADR for the PSRAM-aware BLE default and no-PSRAM consent
  gate
status: Done
assignee: []
created_date: '2026-07-04 15:55'
updated_date: '2026-07-09 20:37'
labels:
  - docs
  - adr
dependencies: []
ordinal: 215000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR audit flagged a missing architectural record. TASK-995 (commit 2f3bd23d3) introduced a capability/consent policy that is currently code-only: one combined firmware sets -DBOARD_HAS_PSRAM globally and uses psramFound() as the sole runtime discriminator in satBleActive() (= bBleEnable && (psramFound() || bBleRiskAck)). Boards with PSRAM run BLE by default; boards without PSRAM (OTGW32) keep BLE dormant behind a Web UI instability-risk consent dialog backed by a new persisted setting bBleRiskAck. This is architectural (new persisted setting, a user-consent contract, a memory-headroom policy tied to the ADR-147/149 concurrent-serve heap gate) and should have an ADR. Author it via the adr-generator / /adr-kit:adr flow.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A new ADR documents the PSRAM-aware BLE default: the psramFound() discriminator, the bBleRiskAck consent setting, and the no-PSRAM instability rationale
- [ ] #2 The ADR references the related heap/serve ADRs (147/149) and the NimBLE observer-only footprint trims
- [ ] #3 docs/adr/README.md index includes the new ADR
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09: authored ADR-169 (PSRAM-aware BLE default with a no-PSRAM instability-consent gate), Proposed. Documents the TASK-995 code-only decision: gate settings.sat.bBleEnable && (psramFound() || settings.sat.bBleRiskAck) at SATble.ino:629-631, persisted bBleRiskAck (key SATbleriskack), one combined image with -DBOARD_HAS_PSRAM global + runtime psramFound() discriminator. Declarative enforcement require_pattern keeps the psramFound()||bBleRiskAck discriminator in SATble.ino. All 4 gates PASS. Open validation (TASK-988: PSRAM real utility / ~64KB footprint) flagged in the ADR's Risks so acceptance is gated on it.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The PSRAM-aware BLE default + no-PSRAM consent gate (TASK-995, previously code-only) now has ADR-169 (Proposed): BLE runs by default only where psramFound() is true; no-PSRAM boards keep it dormant behind the persisted bBleRiskAck consent flag, one combined image, psramFound() the sole runtime discriminator.
<!-- SECTION:FINAL_SUMMARY:END -->
