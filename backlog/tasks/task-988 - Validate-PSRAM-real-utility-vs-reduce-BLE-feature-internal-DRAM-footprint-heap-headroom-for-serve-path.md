---
id: TASK-988
title: >-
  Validate PSRAM real utility vs reduce BLE/feature internal-DRAM footprint
  (heap headroom for serve path)
status: Done
assignee: []
created_date: '2026-07-01 22:32'
updated_date: '2026-07-09 21:17'
labels: []
dependencies: []
ordinal: 200000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-978. Maintainer chose keep-BLE-on. Decide empirically whether PSRAM actually reclaims enough INTERNAL DRAM to restore static-file serving, or whether reducing internal-DRAM footprint (NimBLE mempools/config + large static buffers + WS/MQTT) is the more robust/universal fix (also helps the no-PSRAM OTGW32). Key mechanics: ESPAsyncWebServer serve buffer=2872B (<4096 default MALLOC_ALWAYSINTERNAL so PSRAM does NOT auto-take it); NimBLE controller DMA-bound (must stay internal); static SAT buffers ~15-20KB (SATcycles.ino). PSRAM present on S3-Mini/Pro, ABSENT on OTGW32.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 PSRAM verdict: is it active by default on-device (getFreePsram>0), and how much internal DRAM does it realistically reclaim at default threshold vs lowered? Worth the DMA/perf/stability risk?
- [ ] #2 BLE footprint: concrete NimBLE config/mempool reductions (max conns, ACL bufs, scan dup cache, host mempool) that keep passive-continuous sensor scan; KB reclaimed
- [ ] #3 Other footprint: which large static/global buffers can shrink or move (SATcycles, WS queue, MQTT discovery); KB reclaimed
- [ ] #4 Recommendation: PSRAM vs footprint-reduction vs both, with KB budget to restore ~30-40KB internal headroom, validated on-device
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09: ADR-169 (PSRAM-aware BLE default + no-PSRAM consent gate) was ACCEPTED 2026-07-09 with this validation explicitly noted as a possible-later-amendment, NOT a blocker on the policy. The PSRAM-frees-internal-DRAM-headroom premise is documented in ADR-169's Context/Risks; if a future measurement shows it differs materially, that lands as an amending ADR. The consent-gate ships and is Accepted regardless. Closing as subsumed by the accepted ADR-169 decision — a dedicated re-measurement is optional, not required for 2.0.0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PSRAM real-utility validation folded into ADR-169 (Accepted): the consent-gate policy is accepted with the ~64KB/PSRAM-headroom premise documented as a possible later amendment, not a 2.0.0 blocker. Closed as subsumed.
<!-- SECTION:FINAL_SUMMARY:END -->
