---
id: TASK-1665
title: >-
  Diagnostic recorder core: ring, record format and capture points (ADR-213 step
  1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:34:00Z'
updated_date: '2026-11-14T15:50:00Z'
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
- [x] #1 Host tests: format round-trip, block-independent decode, refuse-below-5120 B red on an unguarded start and green with the rule, resume record first
- [ ] #2 Bench .61 (no PSRAM) and a PSRAM board: measured RAM/flash cost and append cost recorded in the task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 23:10 implementation in wt-1665 (lane-1665, uncommitted): diagRecorderStuff.h/.ino, platform shims platformHasPsram/platformMutexTryLock, settings DiagRecordingOn, one-line hooks (processOT, sendPICSerial, OTDirect command, sendMQTTData, MQTT inbound, REST, weather, BLE, NTP, boot). Host test test/host/test_diag_recorder.py --mutant: real F1-F14 pass, guard-removed mutant fails F7/F9, RESULT PASS; evaluate --quick 85 passed 0 failed; run_tests 159/165 with the same 6 failing on the 0954494d4 baseline. NOT yet compiled for ESP32, no bench (AC2 open). Robert decided (AskUserQuestion 23:10): keep the 16-bit FNV-1a name hash for topic/setting ids instead of the ADR's fixed table, with a host collision check over the real topic list (TASK-1666/1667) and the names in the file header; needs an amending ADR. Must Not conformance: OT-Direct port 25238 bridge lines pass sendPICSerial/handleOTDirectCommand and must be suppressed (ADR-213 Must Not).

2026-11-05 23:40: lane commit 5344f039a on lane-1665 (wt-1665): recorder core + bridge-line suppression (F15) + ADR-214 Accepted. Next: esp32-combo build (wt-1665 has no .build-venv: create it first, see reference_fresh_worktree_build_venv), then bench AC2 on .61 (no PSRAM) and on a PSRAM board after the .63 soak (~2026-11-06 15:00). Build deferred tonight: commit charge 85.5 %.

2026-11-06 00:45: ADR-214 (Accepted) brought onto dev docs-only ahead of the firmware, to reserve the number before PR #698 (whose ADR becomes 215). Robert confirmed both PR merges in order (#697 then #698) after green gates.

2026-11-06 08:40: lane-1665 rebased on origin/dev (fe969a5ba, only conflict the task file, dev version kept), .build-venv created in wt-1665, esp32-combo build OK (BUILD_EXIT=0, Build completed successfully). Static cost vs dev beta.104 build: RAM 122284 -> 122412 B (+128 B), app flash 2150799 -> 2156955 B (+6156 B, 94.0 % of 2240 KiB). Not yet measured: ring heap and append cost on the device. A recording cannot be started in this lane: diagRecorderStart() is only called from the TASK-1666 REST route and DiagRecordingOn is not in isKnownSetting(), so the runtime part of AC2 needs TASK-1666 (or a bench-only hook). .61 not flashed: it carries the 1 h soak of public-beta candidate beta.104 (Robert 08:40).

2026-11-06 landing on dev (one commit with TASK-1666, hooks on): test/host/test_diag_recorder.py --mutant RESULT PASS (real F1-F15 pass, guard and bridge mutants fail); evaluate --quick 85/0. Bench .61 lane build beta.103+fe969a5 (no PSRAM): ring 1 block x 4096 B, start costs -5124 B heap (71936 -> 66812), static +128 B RAM / +6156 B flash vs dev. AC2 still open: append cost and the PSRAM board (.63, after its overnight curve ends 2026-11-07 08:00).

2026-11-06 security review finding (commit review): diagRedactSetting copied non-secret free-text values verbatim, so a WebhookPayload template with "token":"..." or key=... reached the ring. Fix: diagRedactBody() over the copied value. Test F10 (two payload cases) FAIL on the old code, PASS on the fix; test_diag_recorder.py --mutant and test_diag_recording_routes.py --mutant RESULT PASS.

Stale audit 2026-11-14 (coordinator decision): AC2 needs a bench on .63 (PSRAM) by 09; the coordinator queues it.

2026-11-14 (09) bench toward AC2 (not ticked: append cost not measured). .63 PSRAM (2.0.0-beta.248+5e8c416): start -> ring 384 x 4 KB = 1 572 864 B; psram_free 2 057 092 -> 451 188 B = 1 605 904 B (ring + 33 040 B incl. the 768 x 6 B repeat caches); internal heap unchanged (46 932 B free, maxblock 31 732). After stop the PSRAM stays reserved by design (allocPsramOnce, reused by the next start, diagRecorderStuff.h:339-343 and 387): psram_free 450 640 B until a reboot. .61 no PSRAM (2.0.0-beta.240+a3dff10): ring 1 x 4 KB; freeheap ~63 020 -> ~57 604 B (~5.4 KB: 4096 ring + 768 B caches + overhead, noise +-250 B), maxblock unchanged 31 732; after stop the heap returns (~61.9 KB, normal drift). Flash, estimated from the beta.248 ELF symbols (xtensa nm): ~6 KB recorder code (diag* t/T, minus the unrelated sendMQTTheapdiag and diag_log_add) and ~587 B data/bss; a lower bound because of LTO inlining. Not measured: the append cost per record (needs on-device timing instrumentation). Fill rate on .63 with its current traffic: the first 4 KB block took more than 3 min, so a full ring takes many hours. Settings diff [] on both units after stop. Evidence: 09 scratchpad b1665/.
<!-- SECTION:NOTES:END -->
