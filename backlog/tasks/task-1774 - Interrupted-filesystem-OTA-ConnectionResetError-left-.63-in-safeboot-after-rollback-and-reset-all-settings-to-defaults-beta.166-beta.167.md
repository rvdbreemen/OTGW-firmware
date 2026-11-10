---
id: TASK-1774
title: >-
  Interrupted filesystem OTA (ConnectionResetError) left .63 in safeboot after
  rollback and reset all settings to defaults (beta.166 -> beta.167)
status: To Do
assignee: []
created_date: '2026-11-10T05:53:00Z'
updated_date: '2026-11-10T15:25:00Z'
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
- [ ] #1 Reproduce on a bench unit: interrupt a filesystem OTA (e.g. drop the connection mid-upload) and record what the device does (serial + telnet), with settings and bootcount before/after
- [ ] #2 Find the cause of (1) the settings/bootcount loss and (2) the app rollback to safeboot, with evidence
- [ ] #3 Fix so that an interrupted fs OTA keeps settings.ini and the boot counter, and the main app still starts (or safeboot can restore without data loss); old-vs-fix proof on the reproduction
- [ ] #4 scripts/ota_flash.py detects a unit in safeboot (no JSON from the main firmware) and says so, or can drive the safeboot recovery, instead of failing with JSONDecodeError
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 16:23 cross-reference TASK-1763: the .63 crashlog holds 'reboot cause: Exception/panic (4)' at 2026-11-10 06:43:01 on 2.0.0-beta.167+ddf8498, the exact moment of this task's rollback. The coredump decodes (session 4c6667da) to an ipc0 stack overflow during the BLE controller's interrupt allocation (btdm_intr_alloc -> esp_intr_alloc -> heap_caps_malloc; SP 20 B above the stack bottom, end-of-stack watchpoint, DebugException). Likely chain (inference from the timing, not proven): the new 167 app panicked at boot -> the app was never confirmed -> safeboot rolled it back and settings went to defaults; not a corrupt filesystem. Details and files in TASK-1763 notes.
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
