---
id: TASK-1001
title: >-
  docs(adr): accept the verified Proposed 2.0.0 ADRs (153/154/159/161/162) and
  reconcile ADR-153/154 index
status: Done
assignee: []
created_date: '2026-07-04 15:55'
updated_date: '2026-07-04 16:06'
labels:
  - docs
  - adr
dependencies: []
ordinal: 213000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR audit (2026-07-04) found five Proposed ADRs whose decisions are already implemented and on-device-verified, stranded in Proposed only because the maintainer acceptance checkpoint (adr-kit: Proposed->Accepted needs the four verification gates plus maintainer sign-off) was never run: ADR-153 (plaintext MiBeacon, TASK-930 P1), ADR-154 (encrypted MiBeacon AES-CCM + per-slot bindkey, KAT-verified), ADR-159 (symmetric 0x26 watchdog gating, amends ADR-135), ADR-161 (BLE roster REST endpoint + write-only bindkey), ADR-162 (SAT force-boiler test hook). Separately, the README index currently labels ADR-153/154 Accepted while the files say Proposed; the index was corrected to match the files (Proposed), so accepting the ADRs here closes that gap. This is a maintainer decision, not an automated edit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each of ADR-153/154/159/161/162 is reviewed against its four verification gates (Completeness, Evidence, Clarity, Consistency) and either Accepted with the Status flipped Proposed->Accepted, or an explicit reason recorded for keeping it Proposed
- [x] #2 For any ADR flipped to Accepted, the README index status label is updated to match
- [x] #3 ADR-153/154 README-vs-file status conflict is resolved (both index and file agree)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Verified all five in code (SATble.ino MiBeacon plaintext+AES-CCM/bindkey; OTGW-Core.ino symmetric 0x26 gating; restAPI.ino ble/roster + write-only bindkey; SATcontrol.ino force-boiler hook). Flipped ADR-153/154/159/161/162 Proposed->Accepted (2026-07-04) with Status History entries (commit aec3db6). README index reconciled to Accepted for all five (commit bf405c4). ADR-153/154 README-vs-file conflict resolved. ADR-142 (unbuilt) and ADR-160 (code contradicts decision) left Proposed by design.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Accepted the five implemented+verified Proposed 2.0.0 ADRs (153/154/159/161/162) after code verification, on maintainer instruction; updated Status + Status History in each ADR and the README index. ADR-142 stays Proposed (deferred, TASK-1002); ADR-160 stays Proposed pending supersession (TASK-1000).
<!-- SECTION:FINAL_SUMMARY:END -->
