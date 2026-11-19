---
id: TASK-1642
title: >-
  GitHub #10: human readable ASF, RBP flags, TSP, FHB and OEM fault/service
  codes
status: Done
assignee: []
created_date: '2026-11-04T23:53:00Z'
updated_date: '2026-11-15T13:15:00Z'
labels:
  - github
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 768000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/10 by STemplar (2022). Feature request: decode fault/flag MsgIDs into readable text. Needs Robert's go.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): parked for 2.1.0.

Robert 2026-11-05 (session, AskUserQuestion): keep waiting (label needs-maintainer-ok stays).

Released by Robert (DM 1571454228763779093, relayed by the coordinator 2026-11-15): needs-maintainer-ok removed; queued for session 95 after the lane-95-next build/push.

Scope check (session 95, 2026-11-15, code read on the 2.x line): most of GitHub #10 is already there. ASF flags (MsgID 5 HB) are decoded into six bit topics service_request, lockout_reset, low_water_pressure, gas_flame_fault, air_pressure_fault, water_over_temperature (OTGW-Core.ino:3213-3218, TASK-401) plus ASF_flags; RBP flags (MsgID 6) into rbp_dhw_setpoint, rbp_max_ch_setpoint, rbp_rw_dhw_setpoint, rbp_rw_max_ch_setpoint (:2810-2819); the OEM fault code (MsgID 5 LB) is published as a number, OEMFaultCode (:3202). What is not there: readable text for OEM fault/diagnostic codes (MsgID 5 LB, 115) and for TSP/FHB entries (MsgID 10-13). Those meanings are manufacturer-specific (the OpenTherm spec defines only the number), so text needs per-brand tables with a cited source per brand; the equipment matrix (otgw.tclcode.com/matrix.cgi) shows which IDs a unit answers, not what its codes mean. Proposal: close the generic part as done (ASF/RBP decoded, OEM code published), and leave only 'per-brand OEM code / TSP text' as a 2.1.0 design item (feature design in #dev-sat-mqtt per the project rule) with ACs once a first brand table and its source are chosen. Not started; waiting for the coordinator's call on the split.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Generic part of GitHub #10 done on the 2.x line (code read, session 95, 2026-11-15): ASF flags decoded into six bit topics service_request, lockout_reset, low_water_pressure, gas_flame_fault, air_pressure_fault, water_over_temperature plus ASF_flags (OTGW-Core.ino:3198-3218, TASK-401); RBP flags into four bit topics (:2810-2819); the OEM fault code published as OEMFaultCode (:3202). The manufacturer-specific remainder (readable OEM code / TSP / FHB text, per-brand tables) moved to TASK-1973 (2.1.0, design in #dev-sat-mqtt). Coordinator's decision 2026-11-15.
<!-- SECTION:FINAL_SUMMARY:END -->
