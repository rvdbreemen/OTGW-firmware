---
id: TASK-1774
title: >-
  Interrupted filesystem OTA (ConnectionResetError) left .63 in safeboot after
  rollback and reset all settings to defaults (beta.166 -> beta.167)
status: To Do
assignee: []
created_date: '2026-11-10T05:53:00Z'
updated_date: '2026-11-12T18:11:00Z'
labels:
  - bug
  - ota
  - filesystem
  - settings
dependencies: []
priority: high
ordinal: 871000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed by the coordinator on bench .63, 2026-11-10 06:42-06:47, flashing 2.0.0-beta.167+ddf8498 over beta.166 with scripts/ota_flash.py (relayed to 030509; no capture).
Sequence (as reported):
1. App OTA OK.
2. Filesystem OTA ended in ConnectionResetError. /version.hash already showed the new hash, but /v2.html.gz was not valid gzip.
3. The unit was then in SAFEBOOT: 'The firmware failed to start and was rolled back'; /info app_valid=false app_rolled_back=true.
4. Recovery: app via curl -F to safeboot /update, then ota_flash.py --no-app --fs.
5. After that ALL settings were at their defaults (16 diffs incl. hostname, mqttbroker, sat*) and bootcount 178 -> 3.
6. The coordinator restored the settings with POST /api/v2/settings from his backup (150/150 equal; passwords are masked, so they cannot be verified); MQTT reconnected.
Facts checked by 030509 on the saved files:
- The saved 'v2.gz' (1838 B) starts with '<!' and is the OTGW safeboot HTML page, not a damaged gzip. So when it was fetched, the safeboot answered, not the main firmware. The 'not valid gzip' is a symptom of being in safeboot, not proof of a corrupt v2.html.gz.
- ota_flash.py keeps settings by relying on the main firmware keeping settings.ini and its other kept files across the new filesystem image (scripts/ota_flash.py:2-10, :293-307; test tests/test_fs_update_keep.py).
- _get_json (:68-70) does json.load on the answer, so ota_flash.py cannot start while only the safeboot answers (JSONDecodeError).
Guesses (not verified): the fs write was cut off before the kept files (settings.ini, the boot counter) were written back, so the next boot found a fresh image; the rollback to safeboot came from the main app failing to start on a half-written filesystem.
Root cause unknown; no analysis beyond this without a capture.
Files (local, not in git): OTGW-build-archive\flash-logs\63-beta167-fs-ota-reset-20261110\ (settings-63-pre167.json, settings-63-post167.json (0 B), post.json, v2.gz); the beta.167 bins/ELF are in the coordinator's scratchpad flash63-167/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce on a bench unit: interrupt a filesystem OTA (e.g. drop the connection mid-upload) and record what the device does (serial + telnet), with settings and bootcount before/after
- [ ] #2 Find the cause of (1) the settings/bootcount loss and (2) the app rollback to safeboot, with evidence
- [ ] #3 Fix so that an interrupted fs OTA keeps settings.ini and the boot counter, and the main app still starts (or safeboot can restore without data loss); old-vs-fix proof on the reproduction
- [ ] #4 scripts/ota_flash.py detects a unit in safeboot (no JSON from the main firmware) and says so, or can drive the safeboot recovery, instead of failing with JSONDecodeError
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 16:23 cross-reference TASK-1763: the .63 crashlog holds 'reboot cause: Exception/panic (4)' at 2026-11-10 06:43:01 on 2.0.0-beta.167+ddf8498, the exact moment of this task's rollback. The coredump decodes (session 4c6667da) to an ipc0 stack overflow during the BLE controller's interrupt allocation (btdm_intr_alloc -> esp_intr_alloc -> heap_caps_malloc; SP 20 B above the stack bottom, end-of-stack watchpoint, DebugException). Likely chain (inference from the timing, not proven): the new 167 app panicked at boot -> the app was never confirmed -> safeboot rolled it back and settings went to defaults; not a corrupt filesystem. Details and files in TASK-1763 notes.

2026-11-12 01:26 AC1 bench (otgw-bench-tester), .61 OTGW32 on 2.0.0-beta.187+579e0d1 (app already running and valid for minutes): filesystem OTA of the same 187 littlefs.bin (786432 B) via POST /update?cmd=100&size=786432, cut after 314572 B (40 %, 2.6 s) with a TCP RST (scratchpad t1774/cut_fs_ota.py), then 3 min of polling every 10 s; COM4 serial and telnet 23 logged. What the device did (telnet): '[OTA] Filesystem image incomplete: formatting LittleFS' -> 'LittleFS format OK (61 ms)' -> 'Abort: LittleFS remount OK' -> 'Restoring settings to filesystem' -> restored /sat/sat_preset.json, /sat_energy.json, /sat_energy_estimate.json, /sat/sat_hcr.json, /sat_markers.json, /reboot_count.txt, /reboot_log.txt -> 'Filesystem update incomplete (249652 of 786432 bytes): LittleFS was formatted and restored, the web UI files are missing'. No reboot (bootcount 383 before and during all 18 polls), no safeboot (main firmware kept answering /api/v2/device/info), crashlog available=false, settings 518/518 flattened keys equal (0 diff). The web UI then serves 'You first need to upload this file'; recovery: ota_flash.py --no-app --fs <187 fs> -> v2.html.gz complete, settings 164 unchanged, bootcount 384. So the .63 incident (settings lost, bootcount 178 -> 3, app rolled back to safeboot) is NOT reproduced by a cut fs upload on an app that is already valid. Hypothesis for AC2 (not verified): on .63 the fs OTA followed the app OTA within the app's 30 s verify window (verifyRollbackLater, marked valid after 30 s of loop() or in doRestart); a reset or crash during that window rolls the app back to safeboot, and a reset mid-format/before the restore would lose settings.ini and the boot counter. Next reproduction to try: app OTA then an fs cut within 30 s of the new app's boot.

Recurrence on beta.217+a7676dc (otgw-bench-tester 09, relayed by the coordinator, written by otgw-firmware-95, 2026-11-12): filesystem OTA (/update?cmd=100) on the Classic with PIC (.63) panicked at 19:01:29. crashlog: 'reboot cause: Exception/panic (4)', 'Core dump: ipc0 pc=0x4038394f elf=805ace66 bt=0x4038394f 0x40384e98 0x40383c98' (elf SHA matches the build ELF). 09's decode: xPortEnterCriticalTimeout (port.c:478) <- vTaskSwitchContext (tasks.c:3653) <- _frxt_dispatch, on ipc0. Afterwards the UI file was 4096 of 199243 B, the settings were reset to defaults, and after the FS re-upload the device rolled back to safeboot; recovered via the safeboot app upload, stable since. The same FS OTA on .61 (OTGW32) went fine twice. Evidence: C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\b217-63 (63-ota.log, 63-ota-fs2.log, 63-repro-fs.log, 63-safeboot-app.log, crashlog.json, settings pre/post). 09 is trying to reproduce (FS 217->217, then 203->203).

Reading (95), interpretation, not verified: a fault inside xPortEnterCriticalTimeout called from the scheduler's context switch on ipc0 fits a corrupted spinlock/TCB more than a plain stack overflow of the running task; it is compatible with ADR-225's hypothesis (ipc1 stack overflowing into ipc0's TCB), but the crashlog carries no exception cause (EXCCAUSE/EXCVADDR), so a spinlock-timeout abort is not excluded. The port.c line was checked only against the local framework-espidf copy, which may not be the IDF build inside the pinned Arduino libs. Consequence for ADR-225: the evidence is on ipc0, so the stack raise must cover both IPC tasks (CONFIG_ESP_IPC_TASK_STACK_SIZE sets both, to be confirmed in the plan), and the bench for it must include the FS OTA on .63, not only the boot loop on .61. Bench .61 vs .63 difference to keep in view: .63 has a PIC and the serial task running during the FS write.
<!-- SECTION:NOTES:END -->

## Comments

<!-- COMMENTS:BEGIN -->
created: 2026-11-10T15:10:00Z

---

New fact (otgw-firmware-4c, relayed by the coordinator, 2026-11-10): on .63 there is a panic on ipc0 at 06:43:01 (pc 0x4037caec = _xt_lowint1, xtensa_vectors.S:1206, beta.167+ddf8498 ELF, backtrace corrupted). That is the moment of the safeboot rollback during the coordinator's flash (06:42-06:47).
Hypothesis (not confirmed): the beta.167 app panicked at boot with the same kind of IPC-stack overflow as TASK-1763, but on ipc0. The app was then not confirmed, so safeboot rolled it back, and the settings/bootcount reset followed. If so, this task is not (only) an interrupted-filesystem-OTA problem.
Next: 4c reads the coredump after the release; this task is updated then.
Related: TASK-1763 (ipc1 overflow, proven), ADR-225 (covers both IPC stacks).

---
<!-- COMMENTS:END -->
