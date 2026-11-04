---
id: TASK-1458
title: >-
  SAT: published Summer Simmer Index is computed from the SSI itself when
  Thermal comfort is on (double SSI)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:03:00Z'
updated_date: '2026-10-31T22:25:00Z'
labels:
  - sat
  - wontfix
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 618000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1441 (2026-10-29). The published Summer Simmer Index (sat/ssi, summer_simmer_index, summer_simmer_perception, SATcontrol.ino ~3607-3615) is computed from satGetRoomTemp(). With Thermal comfort on, satGetRoomTemp() already returns the SSI instead of the room temperature (~1700-1710), so the published index is the SSI of an SSI. Pre-existing on HEAD, reporting only (the control path uses the SSI once). Fix: compute the published index from the raw room temperature. Check thermo-nova climate.py:245-246 (summer_simmer_index attribute uses current_temperature, the raw value).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With Thermal comfort on, sat/ssi equals the index of the raw room temperature and humidity (host test red on HEAD, green on fix)
- [ ] #2 With Thermal comfort off the published index is unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 (session otgw-firmware-ea): premise corrected at the pin. thermo-nova does NOT use the raw temperature: climate.py:244-245 publishes SummerSimmer.index(self.current_temperature, ...), and current_temperature returns the index under Thermal comfort (climate.py:276-277), so thermo-nova publishes an index of the index, exactly as the firmware does today. The lane fix (wt-v1458, satGetRoomTempRaw for the published index) is therefore a deviation from thermo-nova and needs Sergeant D's decision (CLAUDE.md SAT rules). Queued as Q-1458a after Q-1502b. Not implemented until answered. AC#1 also names sat/ssi, removed by TASK-1485; the remaining topics are sat/summer_simmer_index and sat/summer_simmer_perception.

2026-10-31 worktree cleanup: unmerged work sits in lane wt-1458 (archived as D:\Users\Robert\Documents\GitHub\RvdB\lane-archive-20261031\wt-1458.uncommitted.patch). Not merged: it changes SAT behaviour and its Sergeant D question (Q-1458 a) is still queued, never asked; ask it, then merge.

2026-10-31 Robert (session, interactive question asked twice): Close as won't fix (parity). thermo-nova publishes the index of the index too (climate.py:244-245 with current_temperature = the index under Thermal comfort, :276-277), so the firmware keeps today's behaviour. The lane fix (satGetRoomTempRaw for the published index) stays archived in D:\Users\Robert\Documents\GitHub\RvdB\lane-archive-20261031\wt-1458.uncommitted.patch; lane wt-1458 removed. Q-1458a withdrawn.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Won't fix by maintainer decision (2026-10-31): the published Summer Simmer Index stays as thermo-nova computes it (index of the control temperature, climate.py:244-245 and :276-277). No code change; ACs not applicable.
<!-- SECTION:FINAL_SUMMARY:END -->
