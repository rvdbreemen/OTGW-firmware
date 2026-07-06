---
id: TASK-995
title: >-
  feat(ble): PSRAM-aware BLE default + no-PSRAM consent gate + memory
  optimization
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-03 20:38'
updated_date: '2026-07-06 05:34'
labels: []
dependencies: []
ordinal: 207000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PSRAM present -> activate PSRAM, BLE default ON, verify system memory offloads to PSRAM (internal freed). No PSRAM -> BLE default OFF; enabling it shows an instability-risk consent dialog (accept=on, decline=off); apply host-pool trims to reduce internal use. Validate on S3-Mini (PSRAM) and OTGW32 (no PSRAM) with on-device evidence + prove PSRAM saves internal memory.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 esp32-classic + esp32-combo build with PSRAM active (psramFound=1 on S3-Mini)
- [ ] #2 BLE default = psramFound() at fresh settings init
- [ ] #3 No-PSRAM: v2 UI consent dialog gates BLE-enable (accept persists, decline reverts off)
- [ ] #4 Host-pool trims applied; internal-memory saving measured with BLE on, no PSRAM
- [ ] #5 Evidence: internal free/maxblk + ramp with BLE on, PSRAM off vs on, on S3-Mini
- [ ] #6 OTGW32 physical validation (pending board connection)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Blocked on TASK-994's AC#5 (maintainer decision between options A/B/C). This task's ACs (PSRAM-aware BLE default, consent-gate UI, host-pool trims) presuppose a specific implementation direction that TASK-994's research has not yet been decided on by the maintainer. Also AC#6 explicitly needs OTGW32 hardware ('pending board connection') which is not available in this environment. No action taken this pass -- correctly sequenced behind TASK-994, not re-litigating scope here.
<!-- SECTION:NOTES:END -->
