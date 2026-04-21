---
id: TASK-354
title: >-
  fix(otgw): wrap VH status publishers in beginStatusBurst/endStatusBurst
  quiesce
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-21 07:31'
updated_date: '2026-04-21 16:58'
labels:
  - code-review
  - mqtt
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1A HIGH#1 and Phase 2B HIGH-2: VH (ventilation) status publishers at OTGW-Core.ino:1667-1733 lack the burst-quiesce wrappers present on non-VH publishers. On VH-equipped boilers the heap-pressure-reduction benefit of this branch is negated; drip runs freely during and after VH Status fanouts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 publishMasterStatusVHState wraps its full fanout in beginStatusBurst/endStatusBurst
- [x] #2 publishSlaveStatusVHState wraps its full fanout in beginStatusBurst/endStatusBurst
- [x] #3 publishStatusVHBitMQTT calls incrementStatusBurstPublishCount on publish path (mirrors non-VH sibling)
- [ ] #4 VH-hardware tester confirms drip no longer collides with VH Status fanouts
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read non-VH wrap pattern (publishMasterStatusState/publishSlaveStatusState) and non-VH bit helper (publishStatusBitMQTT)
2. Mirror in publishMasterStatusVHState: add beginStatusBurst, incrementStatusBurstPublishCount in gated block, endStatusBurst after last bit publish
3. Mirror in publishSlaveStatusVHState: same wrapping
4. Mirror in publishStatusVHBitMQTT: add incrementStatusBurstPublishCount on allowPublish path
5. Build with python build.py --firmware and verify
6. Check ACs 1-3; AC 4 requires VH hardware tester, leave unchecked
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Mirrored the non-VH burst-wrap pattern exactly onto the VH side:
- publishStatusVHBitMQTT: added `if (allowPublish) incrementStatusBurstPublishCount();` between OTPublishGate and publishMQTTOnOff (mirror of publishStatusBitMQTT at line 1496).
- publishMasterStatusVHState: added beginStatusBurst() before the gated sendMQTTData block, `if (publishCombined) incrementStatusBurstPublishCount();` inside the gate, endStatusBurst() after the four vh_* bit publishes.
- publishSlaveStatusVHState: same wrap around the six vh_* bit publishes.
Build: python build.py --firmware → OK (OTGW-firmware-1.4.1-beta+deaddd8.ino.bin, 0.69 MB).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wrap VH status publishers in beginStatusBurst/endStatusBurst so they behave symmetrically with their non-VH counterparts.

## Changes (src/OTGW-firmware/OTGW-Core.ino)

- publishStatusVHBitMQTT: added `incrementStatusBurstPublishCount()` on the allowPublish path (mirror of publishStatusBitMQTT, line 1496) so real VH bit sends arm the post-burst cooldown.
- publishMasterStatusVHState: wrapped the four-bit fanout (`vh_ventilation_enabled`, `vh_bypass_position`, `vh_bypass_mode`, `vh_free_ventilation_mode`) with beginStatusBurst() / endStatusBurst() and moved the existing OTPublishGate block inside the window. Added `if (publishCombined) incrementStatusBurstPublishCount();` before `sendMQTTData(F("status_vh_master"), ...)`.
- publishSlaveStatusVHState: same wrap around the six-bit slave fanout (`vh_fault`, `vh_ventilation_mode`, `vh_bypass_status`, `vh_bypass_automatic_status`, `vh_free_ventliation_status`, `vh_diagnostic_indicator`). Gated `sendMQTTData(F("status_vh_slave"), ...)` now increments the burst counter on real sends.

Semantics of the VH publish path are unchanged. The only additions are the begin/end/increment calls so the MQTT discovery drip is suppressed during VH Status fanouts and the post-burst cooldown is armed on real sends, matching the non-VH path (TASK-342/347).

## Build
- `python build.py --firmware` → OK (0.69 MB image, 1.4.1-beta+deaddd8). No new warnings. No other files changed.

## ACs
- AC #1, #2, #3: checked (code-level, verifiable against the diff + build).
- AC #4 (VH-hardware tester confirms drip no longer collides with VH Status fanouts): UNCHECKED. Requires field test on a VH-equipped boiler, which this agent cannot self-perform. Per CLAUDE.md §7 autonomous-completion exception for hardware-verification-blocked ACs, task status stays "In Progress" pending tester confirmation.

## Risks / follow-ups
- Symmetry with non-VH means the same invariants apply: beginStatusBurst/endStatusBurst must pair on every path through the function. Both VH publishers are straight-line (no early returns), so the pairing is trivially preserved; the existing timeout self-heal in endStatusBurst provides the same safety net as on the non-VH side.
<!-- SECTION:FINAL_SUMMARY:END -->
