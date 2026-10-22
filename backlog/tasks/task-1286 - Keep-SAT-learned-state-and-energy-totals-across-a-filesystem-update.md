---
id: TASK-1286
title: Keep SAT learned state and energy totals across a filesystem update
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T00:11:00Z'
updated_date: '2026-10-22T01:27:00Z'
labels:
  - sat
  - ota
  - filesystem
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 414000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1283 (2026-10-22): a filesystem update over /update?cmd=100 writes a fresh LittleFS image; the firmware writes back only settings.ini (from RAM), dallas_labels.ini (heap copy, LABELS_KEEP_MAX 2 KB) and flushes the DHW water meter. Everything else on LittleFS is lost, among them SAT's learned state: /sat_pid_state.json, /sat_energy.json, /sat_energy_estimate.json, /sat/sat_hcr.json, /sat_markers.json, plus ot-boiler.json/ot-thermo.json, the reboot log, bootdetect.log and topic_cleanup.bin (list from scanning the real littlefs.bin and reading the write-back code; the reset on a device is read from code, not measured). With the one-action update every release now rewrites the filesystem, so energy totals and the PID/curve learning restart at every update. Decide which files are worth keeping (energy totals and PID state at least) and keep them the same way as the labels (RAM/heap copy across the write, written back after the remount), within the heap budget.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision recorded: which LittleFS files survive a filesystem update (at least the SAT energy totals and PID state)
- [x] #2 Bench: energy totals and PID state are equal before and after a filesystem update (red on the current firmware)
- [x] #3 The /update page and the update guide list what is kept and what is lost, matching the firmware
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Decision (main session, 2026-10-22; files are tiny on the bench: sat_pid_state 62 B, sat_energy 13 B, labels 103 B): (a) WRITE BACK FROM RAM after the filesystem image is written and remounted, the same way settings.ini is (OTGW-ModUpdateServer-esp32.h ~455), for everything the firmware holds in RAM with an existing saver: SAT energy total (sat_energy.json), estimated gas energy (sat_energy_estimate.json), PID state (sat_pid_state.json), heating-curve recommendation (sat/sat_hcr.json), cycle/window files and markers if they have savers. RAM is fresher than the file (energy is only saved hourly), so this also keeps the energy counted since the last hourly save. (b) HEAP COPY across the write for files without a RAM model, generalising the TASK-1266 dallas_labels mechanism into a small list with a per-file cap (2 KB) and a total cap (~8 KB): dallas_labels.ini, ot-boiler.json, ot-thermo.json (learned OT profiles), reboot_log.txt, reboot_count.txt. Files over the cap are skipped with a debug line, as labels today. Not kept: bench/test files (otgw_simulation.log), files the user uploaded in the File Explorer (documented). Update the /update page keep/lose text and docs/guides/WIFI_FIRMWARE_UPDATE.md to match.
Verification: bench red (current firmware): accumulate SAT energy with the DHW replay, note REST energy_kwh / energy_estimated_kwh / pid_i, do a filesystem update, values reset to 0; green (fix): same values after the update (energy within one control tick), PID integral kept, OT profile files present; settings diff 0. Host/source test that the keep list and the RAM write-back are wired.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 RED bench (OTGW32 .88.61, alpha.449+a447ff7, filesystem update via /update?cmd=100 the existing way): after 160 s of the DHW replay SAT REST showed energy_kwh 0.683, energy_estimated_kwh 0.743 (files: sat_energy.json {"kwh":0.000} because the total is saved hourly, sat_energy_estimate.json {"kwh":0.743}). After the filesystem update: energy_kwh 0, estimate 0; /sat_energy.json, /sat_energy_estimate.json, /sat_pid_state.json all 404. dallas_labels.ini kept (TASK-1266); reboot_log.txt present (rewritten at boot). Evidence %TEMP%\bench-1286\red-*.txt.

2026-10-22 GREEN round 0 (alpha.449+844dff4 build of the first implementation, OTGW32 .88.61): after the same 160 s DHW replay (RAM energy_kwh 0.683, file 0.000; estimate 0.743) a filesystem update via /update?cmd=100 kept the values: energy_kwh 0.783, estimate 0.852 (higher, not reset: the replayed flame ran a little longer), /sat_energy.json {"kwh":0.783}, /sat_energy_estimate.json {"kwh":0.852}, /sat_pid_state.json present, dallas_labels/reboot_count/reboot_log restored; telnet: '[OTA] kept 219 bytes of runtime files on the heap', 'Restoring /dallas_labels.ini (103 bytes): OK', '/reboot_count.txt (3 bytes): OK', '/reboot_log.txt (113 bytes): OK', 'SAT: learned state written to the filesystem'. Red was 0/0 with the SAT files 404. Review workflow (3 lenses x 2 skeptics, 25 agents): 11 findings, 7 upheld -> 5 issues: HCR write fails because /sat/ does not exist on a fresh image (pre-existing: HCR never survived a fresh image), OT profiles not force-written from RAM when the heap copy is skipped, heap copies leaked on failed uploads, misleading 'too large' log for the total cap, test weaker than its docstring. Sent back for fixes.

2026-10-22 GREEN round 1 (alpha.449 build of the fixed diff, OTGW32 .88.61): before the filesystem update energy_kwh 0.783, estimate 0.852, /sat/sat_hcr.json 404 (pre-existing: no /sat folder on a fresh image), /ot-boiler.json 404, /ot-thermo.json 200. After /update?cmd=100: energy_kwh 0.783 and estimate 0.852 unchanged; /sat_energy.json, /sat_energy_estimate.json, /sat_pid_state.json present; /sat/sat_hcr.json now written (create=true); /ot-boiler.json and /ot-thermo.json written from RAM (forced save), thermostat profile content kept; dallas_labels.ini kept. Tests: test_fs_update_keep 23 OK (19 wiring mutants red), update-orchestrator 58/58 (page Kept/Starts-fresh text pinned), evaluate --quick 72/0, test_static_integrity 33 OK, test_build_safeboot 11 OK (agent run + fresh main-session run of the first three). Bench restored, settings diff vs baseline empty, original simulation log back. Evidence %TEMP%\bench-1286\green2-after.txt, red-*.txt, green-telnet.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A filesystem update (/update?cmd=100, and so every one-action update of TASK-1283) no longer resets SAT's learned state. After the image is written and remounted the firmware writes the SAT energy total, gas estimate, PID state and heating-curve recommendation back from RAM (fresher than the file: energy is only saved hourly), writes both learned OT profiles from RAM, and restores a heap copy of the sensor labels, SAT markers and the reboot log/count (per-file caps, 8 KB total, low-heap guard; copies freed on every failure path). Fixed on the way: the heating-curve file under /sat/ was never written on a fresh image because the folder did not exist. AC1: decision recorded. AC2: bench red (energy 0.683 -> 0, files 404) vs green (energy 0.783 and estimate 0.852 unchanged, files present, sat_hcr.json written). AC3: /update page and guide list what is kept and lost; a test pins the page text. Review: 25-agent workflow, 5 issues fixed. Not verified: a fragmented heap during an update (low-heap skip path), a unit with real learned HCR medians and SAT markers.
<!-- SECTION:FINAL_SUMMARY:END -->
