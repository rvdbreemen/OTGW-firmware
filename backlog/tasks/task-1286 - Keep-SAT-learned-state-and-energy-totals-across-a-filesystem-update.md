---
id: TASK-1286
title: Keep SAT learned state and energy totals across a filesystem update
status: To Do
assignee: []
created_date: '2026-10-22T00:11:00Z'
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
- [ ] #1 Decision recorded: which LittleFS files survive a filesystem update (at least the SAT energy totals and PID state)
- [ ] #2 Bench: energy totals and PID state are equal before and after a filesystem update (red on the current firmware)
- [ ] #3 The /update page and the update guide list what is kept and what is lost, matching the firmware
<!-- AC:END -->
