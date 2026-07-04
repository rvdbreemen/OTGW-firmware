---
id: TASK-1003
title: >-
  docs(adr): author ADR for the PSRAM-aware BLE default and no-PSRAM consent
  gate
status: To Do
assignee: []
created_date: '2026-07-04 15:55'
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
