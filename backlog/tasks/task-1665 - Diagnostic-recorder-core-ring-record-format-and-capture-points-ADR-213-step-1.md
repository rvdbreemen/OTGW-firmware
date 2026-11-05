---
id: TASK-1665
title: >-
  Diagnostic recorder core: ring, record format and capture points (ADR-213 step
  1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:34:00Z'
updated_date: '2026-11-05T23:44:00Z'
labels:
  - diagnostics
milestone: 2.0.0
dependencies:
  - TASK-1470
priority: medium
ordinal: 788000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement ADR-213 decisions 1-4: versioned 4 KiB-block format (16-byte block header, type u8 + LEB128 dt + payload, repeat cache reset per block, topic id table), the 1.5 MB PSRAM ring via platformPsramAllocOnce() and the 4 KB no-PSRAM ring allocated on start / freed on stop with the refuse-below-5120 B rule, overwrite-oldest with gap records, and the capture points listed in ADR-213 (not HA discovery, not port 25238 bytes). Resume after reboot with the Reboot record first. Redaction of secrets, location (0.1 deg) and BLE MAC slots.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host tests: format round-trip, block-independent decode, refuse-below-5120 B red on an unguarded start and green with the rule, resume record first
- [ ] #2 Bench .61 (no PSRAM) and a PSRAM board: measured RAM/flash cost and append cost recorded in the task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 23:10 implementation in wt-1665 (lane-1665, uncommitted): diagRecorderStuff.h/.ino, platform shims platformHasPsram/platformMutexTryLock, settings DiagRecordingOn, one-line hooks (processOT, sendPICSerial, OTDirect command, sendMQTTData, MQTT inbound, REST, weather, BLE, NTP, boot). Host test test/host/test_diag_recorder.py --mutant: real F1-F14 pass, guard-removed mutant fails F7/F9, RESULT PASS; evaluate --quick 85 passed 0 failed; run_tests 159/165 with the same 6 failing on the 0954494d4 baseline. NOT yet compiled for ESP32, no bench (AC2 open). Robert decided (AskUserQuestion 23:10): keep the 16-bit FNV-1a name hash for topic/setting ids instead of the ADR's fixed table, with a host collision check over the real topic list (TASK-1666/1667) and the names in the file header; needs an amending ADR. Must Not conformance: OT-Direct port 25238 bridge lines pass sendPICSerial/handleOTDirectCommand and must be suppressed (ADR-213 Must Not).

2026-11-05 23:40: lane commit 5344f039a on lane-1665 (wt-1665): recorder core + bridge-line suppression (F15) + ADR-214 Accepted. Next: esp32-combo build (wt-1665 has no .build-venv: create it first, see reference_fresh_worktree_build_venv), then bench AC2 on .61 (no PSRAM) and on a PSRAM board after the .63 soak (~2026-11-06 15:00). Build deferred tonight: commit charge 85.5 %.

2026-11-06 00:45: ADR-214 (Accepted) brought onto dev docs-only ahead of the firmware, to reserve the number before PR #698 (whose ADR becomes 215). Robert confirmed both PR merges in order (#697 then #698) after green gates.
<!-- SECTION:NOTES:END -->
