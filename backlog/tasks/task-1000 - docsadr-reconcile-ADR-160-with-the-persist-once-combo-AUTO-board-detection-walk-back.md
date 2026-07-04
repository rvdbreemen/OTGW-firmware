---
id: TASK-1000
title: >-
  docs(adr): reconcile ADR-160 with the persist-once combo AUTO board-detection
  walk-back
status: Done
assignee: []
created_date: '2026-07-04 15:30'
updated_date: '2026-07-04 16:19'
labels:
  - docs
  - adr
dependencies: []
ordinal: 212000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-160 records that combo AUTO board-detection re-probes the PIC every boot and never persists its verdict. Field reality diverged: TASK-947 (c3afa5f4f) implemented never-persist, TASK-949 (7f69f6b91) tried a settings-first reorder to fix a regression where a plain LOLIN S3 Mini could no longer boot Classic, and that reorder HUNG boot on-device and had already shipped to dev, forcing an urgent revert (d7a34f4ad) back to the alpha.288 persist-once logic plus a 3x detectPIC retry in the AUTO branch. So the shipped code now does persist-once, not never-persist, while the ADR text still says never-persist. This is stale ADR documentation, not a code bug. Reconcile the ADR with the shipped behaviour (amend or supersede ADR-160), and capture the hardware boot-order constraint that forced the walk-back. See also ADR-127 section 3 (the original cache-back). Surfaced by the 2026-07-04 dev-2.0.0 commit audit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Current shipped combo AUTO detection behaviour is read from OTGW-firmware.ino (setup path) and stated plainly: persist-once with 3x detectPIC retry, not never-persist
- [x] #2 ADR-160 is amended or superseded so its Decision matches the shipped persist-once behaviour, with the hardware boot-order rationale for the walk-back documented
- [x] #3 The reconciliation cross-references commits c3afa5f4f (TASK-947), 7f69f6b91 (TASK-949), d7a34f4ad (revert) and ADR-127 section 3
- [x] #4 docs/adr/README.md index is updated if a superseding ADR is created
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Resolved via a superseding ADR (maintainer chose reversal over amendment). Authored ADR-164 documenting the shipped persist-once + 3x detectPIC retry (verified OTGW-firmware.ino:341-405), superseding ADR-160's never-persist. ADR-160 marked Superseded by ADR-164 (status-line exception). README index updated: ADR-160 -> Superseded, ADR-164 added, OTGW32 count 11->12.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reconciled ADR-160 by writing superseding ADR-164 (Accepted 2026-07-04): combo AUTO detection persists its PIC verdict once with a 3x detectPIC retry, reversing ADR-160's never-persist after that hung boot on-device and was reverted (d7a34f4ad). ADR-160 marked Superseded; index updated.
<!-- SECTION:FINAL_SUMMARY:END -->
