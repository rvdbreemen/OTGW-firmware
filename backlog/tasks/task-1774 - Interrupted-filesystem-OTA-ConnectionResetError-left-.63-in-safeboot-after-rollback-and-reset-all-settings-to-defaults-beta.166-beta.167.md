---
id: TASK-1774
title: >-
  Interrupted filesystem OTA (ConnectionResetError) left .63 in safeboot after
  rollback and reset all settings to defaults (beta.166 -> beta.167)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T05:53:00Z'
updated_date: '2026-11-14T08:17:00Z'
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
- [x] #4 scripts/ota_flash.py detects a unit in safeboot (no JSON from the main firmware) and says so, or can drive the safeboot recovery, instead of failing with JSONDecodeError
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
TASK-1774 (b) plan (AC2/AC3), ba, 2026-11-14. Read only, no code yet.

WINDOW, traced in OTGW-ModUpdateServer-esp32.h (origin 5cc1612c0):
- :386-389 _handleUploadStart: _keepFiles() copies the 10 KEEP_LIST files to the HEAP (cap 10240 B, :645),
  LittleFS.end(), bFsImageWriting=true (1909). settings.ini is NOT copied: RAM `settings` is its copy.
- :393 Update.begin(fsSize, U_SPIFFS). The Updater erases lazily per sector (:375), so the old FS
  survives until the first write.
- :445 _handleUploadWrite, first Update.write(): sector 0 of spiffs is erased and rewritten. From here
  on the old settings.ini, reboot_count.txt and reboot_log.txt exist only in RAM/heap.
  The window is OPEN.
- Each chunk until the last one (786432 B; on .61 40 % took 2.6 s, so about 6-7 s in total) stays inside the window.
- :470 Update.end(true) -> :479 _remountAfterFsWrite: LittleFS.end(), bFsImageWriting=false (1909),
  format on a short upload (:529, 61 ms on .61), LittleFS.begin().
- :489 _restoreRuntimeFiles -> :542 writeSettings(false) (since 1913: tmp + rename, atomic)
  -> :545 _restoreKeptFiles (:704 open "w" per file, NOT atomic) -> :546 OT profiles -> :547 SAT.
  The window CLOSES only after the last kept file closes.
- Abort path :567-596: same sequence via onDisconnect.
A reset in the window (panic, the TWDT, the 0x26 watchdog, a power cut) means the next boot mounts a partial or a fresh
image. OTGW-firmware.ino:330 LittleFS.begin() -> :334 readSettings(): "file not found -> created file" with
defaults (settingStuff.ino:571-576). reboot_count.txt is missing, so the count restarts. That is exactly .63:
16 diffs, bootcount 178 -> 3. 09's beta.217 panic during the FS write (TASK-1774 notes) is a reset in this
window. Cause (1) of AC2, settings/bootcount loss: a reset at any point between the first chunk write and the
end of the restore, not only "mid-format". (Code reading; to be proven on the bench, see below.)

CAUSE (2), app rollback (ADR-191 interaction): OTGW-firmware.ino:1175-1179 confirms the app at
millis() > 30 s, outside isFlashing(). ota_flash.py runs the FS OTA as soon as the new app answers, so the FS
write usually falls inside the 30 s PENDING_VERIFY window. A reset there rolls the app back to safeboot
(the bootloader marks PENDING_VERIFY as ABORTED on the next boot). That explains settings loss AND safeboot on
.63 (167). Not verified: on 217, 09 saw the rollback only after a second FS upload; the order of
events there is not explained yet.

FIX OPTIONS for (1):
A. RTC_NOINIT snapshot (recommended). At _handleUploadStart, write the files that matter into one
   RTC_NOINIT buffer with magic + length + CRC32, in priority order: settings.ini (serialized from RAM through
   the existing writer into a memory stream, or read from the file before LittleFS.end()), reboot_count.txt,
   reboot_log.txt, then the rest of KEEP_LIST while it fits. Clear the magic after a successful restore (end and
   abort paths). At boot, after the mount (:330) and before readSettings (:334): if the magic and CRC are valid,
   write the files from RTC (each through tmp + fsCommitTempFile, 1913), then clear it.
   + survives a panic, the TWDT and a software reset, which are the observed failure modes; no flash wear; no
     partition change; fast.
   - lost on a power cut or brownout (RTC slow memory without power); size budget: the 233 ELF uses 0x264 B of
     the 8 KB RTC slow memory (.rtc_noinit 0x44 at 0x50000200 + .rtc.force_slow 0x20), so about 7 KB is free if
     the first 512 B stay reserved (to be confirmed in sdkconfig). That is not enough for all of KEEP_LIST
     (maxima 9824 B) plus settings.ini. Hence the priority order; real sizes must be measured (see below).
   - a static 6-7 KB buffer costs DRAM? No: RTC_NOINIT lives in RTC slow memory, not DRAM.
B. NVS snapshot (Preferences blob, own namespace).
   + also survives a power cut.
   - nvs partition is 0x5000 (20 KB, 5 pages; about 16 KB usable after the GC page), shared with the WiFi
     credentials and the safeboot keys host/pw (ADR-191, safebootSyncNvs :128-135). A 5-10 KB blob can
     fail with NOT_ENOUGH_SPACE, or crowd out a later WiFi or safeboot write: risk to exactly what you need
     to recover. Blob write of several 100 ms on the AsyncTCP task at upload start.
Rejected: tail of the safeboot partition (about 70 KB free after the 0.89 MB updater): works, but clever, not
clear, and relies on the bootloader ignoring data after the image; coredump partition: a panic writes it,
exactly when we need it.

For (2), option C (script) and/or D (firmware):
C. ota_flash.py waits before the FS step until the new app is confirmed (uptime > 30 s; device/info
   uptime field, to be checked). Cheap, only helps users of the script.
D. Firmware refuses FS OTA while the running app is PENDING_VERIFY (409 + text "wait 30 s after an app update").
   Protects the web UI too. Trade-off: the web UI /update flow app -> fs at once then gets a refusal; the UI must
   wait or retry.
Recommendation: A + C now, D after Robert's OK (it changes the UI flow).

1909 interaction: the boot restore runs in setup() before startWebserver(), so no FS-OTA can run concurrently;
_remountAfterFsWrite and bFsImageWriting stay unchanged; the RTC clear happens after _restoreRuntimeFiles, within
the existing bESPactive window.
ADR: A changes where the kept files live during the write (architecture of the kept-files contract,
TASK-1266/1286), so probably an ADR (amendment to the KEEP_LIST pattern), via adr-kit.

PROOF PLAN (reproduce, then prove):
- Host: harness on the real _handleUploadStart/_restore blocks with a "reset" at chunk N: old = settings at
  defaults, fix = restored from RTC.
- Bench (09, later, after d3/95): FS OTA on .63 with a forced reset mid-upload (e.g. POST /ReBoot from a 2nd
  client, or a debug command that triggers esp_restart() at chunk N, debug build only), old vs fix: settings diff +
  bootcount. Plus the planned FS-OTA x10.
- Before the code: 09 measures /settings.ini and the sizes of the KEEP_LIST files on .61 and .63 (FSexplorer list) for the
  RTC budget.

ADDENDUM after review (same day):
1. GATE FOR OPTION A, safeboot and RTC: the safeboot ELF (wt-1834 and wt-95w builds, objdump -h) has .rtc.force_slow 0x20 B
   at 0x50000200, the same start as the main app's .rtc_noinit (g_rtcRebootReason 0x34 B at 0x50000200). Safeboot
   therefore overwrites the first 32 B of our RTC_NOINIT at every safeboot start (side finding: the reboot reason
   across a safeboot run is clobbered today). .rtc_noinit and .rtc.bss of safeboot are 0 B. A snapshot that stays
   out of 0x50000200-0x5000021F (placed after it, or pinned at the top of RTC slow) is not touched by
   safeboot's own sections; whether IDF startup, PHY or WiFi touch RTC slow is NOT verified. Precondition before
   A is built: a bench check that an armed RTC snapshot survives the full .63 route (panic -> rollback -> safeboot
   minutes -> curl app -> main). If not, A fails for the safeboot scenario and only B remains.
   Trade-off stated plainly: a power cut during the write OR during the safeboot recovery (user pulls the plug)
   loses the snapshot as well as the settings.
2. Stale snapshot: arm at the FIRST Update.write (:445), not at upload start; disarm on EVERY exit: end (:507),
   abort (:592), too large (:392), Update.begin failed (:395), write error (:448). Boot restore only fills a file
   that is missing or empty; an existing settings.ini is never overwritten from RTC. Host cases for both (stale
   snapshot + existing file = no restore; armed + missing = restore).
3. Bootloader claim cited: framework-espidf bootloader_support/src/bootloader_utility.c:396-405 (PENDING_VERIFY ->
   ABORTED at the next boot, under CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) and :446-451 (NEW -> PENDING_VERIFY).
   Caveat: the local framework-espidf, not proven identical to the IDF in the pinned Arduino libs.
4. Cause (2) framing: AC3 accepts "safeboot can restore without data loss". If A survives the safeboot route, the
   rollback is the intended ADR-191 recovery over WiFi and is harmless; then C is only a convenience and D (UI flow
   change) is probably not needed.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 16:23 cross-reference TASK-1763: the .63 crashlog holds 'reboot cause: Exception/panic (4)' at 2026-11-10 06:43:01 on 2.0.0-beta.167+ddf8498, the exact moment of this task's rollback. The coredump decodes (session 4c6667da) to an ipc0 stack overflow during the BLE controller's interrupt allocation (btdm_intr_alloc -> esp_intr_alloc -> heap_caps_malloc; SP 20 B above the stack bottom, end-of-stack watchpoint, DebugException). Likely chain (inference from the timing, not proven): the new 167 app panicked at boot -> the app was never confirmed -> safeboot rolled it back and settings went to defaults; not a corrupt filesystem. Details and files in TASK-1763 notes.

2026-11-12 01:26 AC1 bench (otgw-bench-tester), .61 OTGW32 on 2.0.0-beta.187+579e0d1 (app already running and valid for minutes): filesystem OTA of the same 187 littlefs.bin (786432 B) via POST /update?cmd=100&size=786432, cut after 314572 B (40 %, 2.6 s) with a TCP RST (scratchpad t1774/cut_fs_ota.py), then 3 min of polling every 10 s; COM4 serial and telnet 23 logged. What the device did (telnet): '[OTA] Filesystem image incomplete: formatting LittleFS' -> 'LittleFS format OK (61 ms)' -> 'Abort: LittleFS remount OK' -> 'Restoring settings to filesystem' -> restored /sat/sat_preset.json, /sat_energy.json, /sat_energy_estimate.json, /sat/sat_hcr.json, /sat_markers.json, /reboot_count.txt, /reboot_log.txt -> 'Filesystem update incomplete (249652 of 786432 bytes): LittleFS was formatted and restored, the web UI files are missing'. No reboot (bootcount 383 before and during all 18 polls), no safeboot (main firmware kept answering /api/v2/device/info), crashlog available=false, settings 518/518 flattened keys equal (0 diff). The web UI then serves 'You first need to upload this file'; recovery: ota_flash.py --no-app --fs <187 fs> -> v2.html.gz complete, settings 164 unchanged, bootcount 384. So the .63 incident (settings lost, bootcount 178 -> 3, app rolled back to safeboot) is NOT reproduced by a cut fs upload on an app that is already valid. Hypothesis for AC2 (not verified): on .63 the fs OTA followed the app OTA within the app's 30 s verify window (verifyRollbackLater, marked valid after 30 s of loop() or in doRestart); a reset or crash during that window rolls the app back to safeboot, and a reset mid-format/before the restore would lose settings.ini and the boot counter. Next reproduction to try: app OTA then an fs cut within 30 s of the new app's boot.

Recurrence on beta.217+a7676dc (otgw-bench-tester 09, relayed by the coordinator, written by otgw-firmware-95, 2026-11-12): filesystem OTA (/update?cmd=100) on the Classic with PIC (.63) panicked at 19:01:29. crashlog: 'reboot cause: Exception/panic (4)', 'Core dump: ipc0 pc=0x4038394f elf=805ace66 bt=0x4038394f 0x40384e98 0x40383c98' (elf SHA matches the build ELF). 09's decode: xPortEnterCriticalTimeout (port.c:478) <- vTaskSwitchContext (tasks.c:3653) <- _frxt_dispatch, on ipc0. Afterwards the UI file was 4096 of 199243 B, the settings were reset to defaults, and after the FS re-upload the device rolled back to safeboot; recovered via the safeboot app upload, stable since. The same FS OTA on .61 (OTGW32) went fine twice. Evidence: C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\b217-63 (63-ota.log, 63-ota-fs2.log, 63-repro-fs.log, 63-safeboot-app.log, crashlog.json, settings pre/post). 09 is trying to reproduce (FS 217->217, then 203->203).

Reading (95), interpretation, not verified: a fault inside xPortEnterCriticalTimeout called from the scheduler's context switch on ipc0 fits a corrupted spinlock/TCB more than a plain stack overflow of the running task; it is compatible with ADR-225's hypothesis (ipc1 stack overflowing into ipc0's TCB), but the crashlog carries no exception cause (EXCCAUSE/EXCVADDR), so a spinlock-timeout abort is not excluded. The port.c line was checked only against the local framework-espidf copy, which may not be the IDF build inside the pinned Arduino libs. Consequence for ADR-225: the evidence is on ipc0, so the stack raise must cover both IPC tasks (CONFIG_ESP_IPC_TASK_STACK_SIZE sets both, to be confirmed in the plan), and the bench for it must include the FS OTA on .63, not only the boot loop on .61. Bench .61 vs .63 difference to keep in view: .63 has a PIC and the serial task running during the FS write.

2026-11-12 rate on the Classic bench (.63), otgw-bench-tester 09's repro, relayed by the coordinator: 9 filesystem writes, 1 panic. 19:00 203->217 app+fs PANIC (ipc0, crashlog 19:01:29); 19:09 217 FS-only OK; 19:10 FS-only x3 OK; 19:11 217->217 app+fs OK; 19:12 217->203 OK; 19:13 203 FS-only OK; 19:13 203->217 OK. Per build: 217 1 of 7, 203 0 of 2. Evidence: 09's scratchpad b217-63\63-repro-fs.log, 63-repro-full.log, 63-repro-203.log (comms checked: every later read shows lastreset Software reset and only the 19:01:29 panic in the crashlog). Used in the beta.218 known issues as '1 of 9 filesystem writes on the Classic bench unit on 12 Nov'.

2026-11-14 AC4 (ba lane, coordinator order: (a) first, script only, no bump): scripts/ota_flash.py now checks at start that the main firmware answers (_require_main_firmware). Read from safeboot/src/main.cpp: the updater redirects every unknown path, /api/v2/* included, with a 302 to its HTML page (onNotFound :224), so _get_json() failed with JSONDecodeError; with an admin password set the page asks for Basic Auth (authorized() :100), so HTTPError 401. On either failure the script reads the updater's own GET /info (with auth) and stops with exit code 3, printing app_valid / app_rolled_back and the recovery steps (curl the app to /update, then --no-app --fs). A 401 on /info too reports 'probably in safeboot ... Check --password'. Normal path unchanged (same CLI, exit 0/2). Side fix: GET /api/v2/settings needs the admin password when one is set (restAPI.ino handleSettings :400-406), but _get_json() sent none, so ota_flash.py could not start on a unit with a password; it now sends --password on the settings reads.

AC4 evidence: new test class OtaFlashUnitInSafeboot in tests/test_ota_flash.py (fake unit with the updater's routes: 302 to /, HTML at /, JSON at /info, optional Basic Auth). Old script (origin/dev fe6d35b59 copy, OTGW_OTA_FLASH_PY): 3 of 3 ERROR (JSONDecodeError; HTTPError 401; HTTPError 401). Fix: 23 of 23 OK (the 20 existing tests unchanged and green). run_tests.py ota_flash fs_update_keep: 2/2 PASS. evaluate --quick: 85 PASS, 0 FAIL. Not verified on a real unit in safeboot: needs a bench run by 09.

AC4 addendum (coordinator: same commit): flash_app() waited for the updater with _safeboot_up(), which sent no credentials to GET /info; with an admin password the updater answers 401 on every route, so the wait ran into 'timeout waiting for safeboot updater' and the app was never uploaded. _safeboot_up() now sends --password. Test test_app_ota_waits_for_safeboot_with_password: old script SystemExit 'timeout waiting for safeboot updater', fix OK. Full file 24/24 OK.

Fix option C (coordinator decision, in the same script commit): ota_flash.py waits APP_CONFIRM_WAIT = 35 s from the new app's first answer before the filesystem POST, so the FS write never falls inside the 30 s PENDING_VERIFY window (OTGW-firmware.ino:1175-1179; IDF bootloader_utility.c:396-405). device/info has no usable uptime (upTime() is minutes only, helperStuff.ino:858), so the script times it itself. Test test_filesystem_waits_until_the_new_app_confirmed_itself: origin script and the (a) commit both FAIL (5 s between app and fs), fix OK; file 25/25 OK.

ADR-233 drafted (Proposed, gated on the RTC survival probe): RTC_NOINIT snapshot of settings.ini, reboot_count.txt, reboot_log.txt above 0x50000220; armed at the first Update.write, cleared on every exit and right after the boot restore; boot restores only missing or empty files; settings.ini whole or not at all (logged when it does not fit); secrets named (ADR-056). Not protected: power cut, SAT/Dallas/markers files (heap copies only), oversized settings.ini. adr-lint: 0 FAIL, 1 advisory. Number 233 checked free on origin/dev and in the other worktrees at creation.

RTC survival probe result (otgw-bench-tester 09, .63, 2.0.0-probe1774+370b324, 2026-11-14 04:50-04:57, evidence in 09's scratchpad b1774\: telnet-keys.log, r1-info.json, ota logs). R1 PASS: armed seed 0xb2f06eeb at 04:50:46 (img=PENDING_VERIFY), panic, panic again, about 3.7 min in safeboot (/info app_valid=false app_rolled_back=true), curl app upload HTTP 200, main boot line 'addr=0x50000244 size=7184 len=7168 magic=ok crc=ok seed=0xb2f06eeb img=PENDING_VERIFY reset=3'. R2 PASS: after C (img=VALID), seed 0xf9060ac3, panic, boot line magic=ok crc=ok same seed img=VALID reset=4. R3 (power cut) not run, pending Robert; RTC loss on power-off is expected, not the claim under test. The ADR-233 gate passes: the snapshot survived two panics, an ABORTED boot, minutes in safeboot and the app re-upload.

Deviation seen in R1 (09): the FIRST panic of the PENDING_VERIFY image did not go to safeboot. ota_0 booted again and reported img=ABORTED reset=4; only the SECOND panic rolled back to safeboot. Reading of the local framework-espidf bootloader_utility.c: :396-405 marks a PENDING_VERIFY entry ABORTED at the next boot (matches img=ABORTED); :407-416 then defaults to factory only when BOTH otadata entries are invalid, otherwise :440-443 boots the active valid entry, which with one OTA slot (app_count 1, seq % 1 = 0) is ota_0 again. So the observation fits a second otadata entry that was still valid (an older VALID entry of the previous app); the code alone does not show who left it there (not verified; the bootloader in the pinned Arduino libs is a precompiled binary, not proven identical to this source). Consequence for cause (2): after the first reset the app runs ABORTED; safebootMarkAppValid() (OTGW-ModUpdateServer-esp32.h:154-161) only confirms a PENDING_VERIFY image, so an ABORTED app never confirms and the next reset of any kind rolls back to safeboot. That explains the beta.217 .63 sequence (panic during the FS write, unit came back on main, rollback only after the 2nd FS upload). To confirm: dump otadata (0xE000, 2 x 4 KB) in each state; proposed as a probe key, or esptool read_flash by 09.

AC3 implementation (option A, ADR-233, coordinator GO 2026-11-14): fsOtaRtcSnapshot marker block in OTGW-ModUpdateServer-esp32.h (RTC_NOINIT, magic FSOT, 3 slots settings.ini / reboot_count.txt / reboot_log.txt, 7424 B data, CRC32 via ROM esp_rom_crc32_le, address check >= 0x50000220). Filled in _handleUploadStart() before LittleFS.end(), armed at the first successful chunk in _handleUploadWrite(), wiped on too large / Update.begin() failed, wiped in _restoreRuntimeFiles() when the settings write succeeded, settings slot dropped after every successful writeSettings() (settingStuff.ino), restored in setup() between the mount and readSettings(). Rule change found while coding: the FS image ships a default settings.ini (data/settings.ini, 400 B, hostname OTGW2; present in the 233 littlefs.bin), so after a reset during the write settings.ini usually exists with that default (fits .63: 16 diffs incl. hostname). An armed, valid snapshot therefore always wins for settings.ini; the counter and log restore only when missing or empty. ADR-233 updated (still Proposed).

Host evidence: new test/host/test_fs_ota_rtc_snapshot.py (+ .cpp). FIX PASS (T1-T8, S1-S6). OLD 08f42d625: fails T1 (reset during the write boots on the image's default settings.ini, count 1, log 9 B), T4, T6 and S1-S6. Mutants MB1 (settings only when missing) T1, MB2 (no drop in writeSettings) S5/T3/T4, MB3 (no boot wipe) T8, MB4 (no CRC check) T7, MB5 (arm at upload start) S2: each caught. Adjusted: tests/test_fs_upload_boot_writes.py regex allows fsOtaRtcFill() between _keepFiles() and LittleFS.end(); test_settings_atomic_write.cpp gets a no-op fsOtaRtcDropSettings(). run_tests on 21 patterns: 23 files PASS after those two. evaluate --quick 85/0. Not yet: full run_tests, build, ELF placement check, bench old-vs-fix with the abort-at-chunk-N probe build.

docs/adr/README.md had no list entry for ADR-232 (Accepted) and none for ADR-233 (pushed in 4bcf241fa): tests/test_adr_governance.py test_live_index_complete failed on origin with both. Both entries added in a docs-only commit on origin (coordinator order); the test is OK.

AC3 bench round 1 (otgw-bench-tester 09, .63, 2026-11-14 ~06:30-06:55, evidence 09 scratchpad b1774c\): panic at 300000 B of a filesystem OTA (key K) on a VALID image, both builds came back on main (not safeboot), otadata unchanged (entry0 seq=1 VALID, entry1 seq=1 NEW). OLD probe1774c.old+483b285: 16 settings diff, hostname OTGW2, bootcount 27 -> 1, reboot_log 20 -> 2 lines, FS usedBytes = totalBytes = 786432 (freeBytes 0) and every new open(w) failed (507) until a clean FS OTA. FIX probe1774c.fix+ce53698: hostname kept, bootcount 30 -> 31 (kept), but 14 settings defaulted, reboot_log 20 -> 2 lines, FS again full; after the boot writeSettings() kept failing ('short write or rename failed (1533 bytes intended)', fix-keys.log 06:48:51 and 06:48:53). Verdict: OLD red as expected, FIX only partly green.

Cause analysis (code reading against 09's measurements; the boot-time restore lines themselves were not captured, telnet connects after setup). (1) Coordinator hypothesis 2 is refuted: fsOtaRtcRestoreAtBoot() runs at OTGW-firmware.ino:333, updateRebootLog() at :541, so the log is not written before the restore. (2) The real cause: after a reset in the middle of the image write the boot mounts a HALF-WRITTEN LittleFS image (new superblock and metadata, the rest of the partition still old blocks). It reads, but it is full (freeBytes 0) and accepts no write that needs a new block. The in-session abort path handles exactly this by formatting first (_remountAfterFsWrite(partial), TASK-1495); the boot path never did. On that full FS my restore of settings.ini (5.6 KB, temp file + rename) fails, so readSettings() reads whatever the broken image shows: here a truncated file whose keys stop right after MQTTseparatesources (the first differing key, LegacyPort25238Enabled, is the next one written by writeSettings(), settingStuff.ino:315-316). reboot_count.txt (4 B) fits a littlefs inline write and came back (30 -> 31); reboot_log.txt (1.4 KB) did not. The hostname survived only because it is the first key of that truncated file; in the OLD run the broken image showed the default file instead. That difference is chance, not the fix. (3) My host harness missed it: its 'imageLanded' replaces the partition with a clean, writable image; the real partial image is not writable. Gap admitted; T9 to add.

Rework after bench round 1 (coordinator GO): the snapshot gets a 'consistent' field (0 when armed, 1 right after a successful Update.end() of a complete image and after the in-session format of a partial image, both in OTGW-ModUpdateServer-esp32.h). A boot that finds it armed and NOT consistent does LittleFS.end/format/begin before the restore and writes the TASK-1495 marker FS_UPDATE_INCOMPLETE_FILE with the format time ('reset during the filesystem write, formatted at boot in N ms'): settings, counter and log come back, the web UI files are gone until the filesystem is uploaded again (the same end state as an in-session aborted upload). The settings slot is now dropped only by a settings write on a consistent filesystem. Format time: in-session 61 ms on .61 (AC1 note); partition is 0xC0000 = 768 KB (not 1.5 MB); setup()'s watchdog is initialised later (OTGW-firmware.ino:564, after the restore at :333) and the loop TWDT is 30 s, so no watchdog can fire there; the boot value is to be read from the marker file on the bench.

Not protected (separate line, also in ADR-233 Consequences): a POWER CUT during the image write leaves no snapshot; the next boot mounts the half-written image as before (runs on whatever it shows, takes no write, needs a filesystem upload). Unchanged by this task.

Host evidence of the rework: test_fs_ota_rtc_snapshot.py with new T9 (a reset mid-write leaves a half-written, full FS: model writes beyond 256 B inline fail until a format) and T10 (a reset after Update.end() of a complete image: no format, web UI kept). FIX PASS (T1-T10, S1-S8). OLD 5aafcf705 fails S1-S8, T1, T4, T6, T9, T10. The benched first implementation fa5853104 as --old-rev fails T9 with 'other (600 B), count 42, log 9 B, fs still broken, formats 0', i.e. exactly 09's round-1 result. Mutants MB1-MB7 each caught (MB6 no boot format: T6,T9; MB7 Update.end not marking consistent: S7). T4/T6/T8 expectations changed with the rule: a half image is formatted, so T6 now expects settings.ini missing (firmware defaults, never a partial copy) and T4 the snapshot's pre-OTA settings (a save on a half-written FS does not drop the slot).

ota_flash.py, from 09's real-unit report (2026-11-14): an --app path that no longer existed crashed with FileNotFoundError AFTER the safeboot handover and left .63 in safeboot (app_valid=true, app_rolled_back=false) until a manual app upload. Now both image paths are checked (exists, non-empty) before anything is sent. Test test_missing_app_image_is_refused_before_the_handover: origin script FileNotFoundError, fix OK; test_ota_flash 26/26.

Full test suite on 5aab53d53 in foreground chunks (-j 2, coordinator GO): 43+39+40+44+20+20+21+22 files, 248 PASS, 0 FAIL, 1 SKIP (test/host/test_stream_real_cbuf.py, TASK-1854), test_capture_otframes PASS this time. Logs in the ba scratchpad r1774/r2c*.log.

AC3 bench round 2 (otgw-bench-tester 09, .63, 2026-11-14 09:02-09:11, 2.0.0-probe1774c.fix2+d3a2537 = rework 5aab53d53 + probe hook, sha256 ino c32384f08ec9...3650b, littlefs aa1dfeb8e681...41fbd matched; evidence 09 scratchpad r1774b\ev\): PASS on every step. Before: bootcount 53, reboot_log 20 lines, settings.ini 5659 B. App OTA, 60 s, O: app0 VALID. K armed, FS OTA panicked at 09:04:32 ('reboot cause: Exception/panic (4)', async_tcp). After the boot: main firmware, bootcount 55 (+1, the app OTA counted one), hostname kept, settings API diff vs S0 [] and raw settings.ini diff [], reboot_count.txt 55, reboot_log 20 -> 21 lines (the panic and core dump on top, older lines kept, the oldest trimmed by the cap), fsupdateincomplete true, /.fs_update_incomplete = 'reset during the filesystem write, formatted at boot in 62 ms', FS usedBytes 21504 / freeBytes 764928 (round 1: full, 507), a settings save works (darktheme true -> false, ini back to S0). GET / served the built-in Helper page (466 B, asks for FSexplorer.html.gz), /update HTTP 200 (16132 B); every static GET showed Helper until FSexplorer.html.gz was uploaded (FSexplorer.ino:572, existing TASK-1495 behaviour). A clean FS OTA then: marker 404, fsupdateincomplete false, settings diff [], web UI back (204387 B). End: beta.237 app + FS back, diff [], probe gone. Round 1 vs round 2: 14 settings at defaults, log 20 -> 2, FS full -> 0 settings lost, log kept, FS writable. AC3 bench by 09; AC3 ticked by 09.

Correction to the note above: AC3 is NOT ticked yet; it is the bench AC and 09 ticks it (after the real build is benched and pushed). AC2 (cause, with evidence) and AC3 (fix with old-vs-fix proof) are the developer's to close only through 09's tick for AC3.

Lane rebuilt on origin/dev 97eacc37b (coordinator BUILD GO): only the five 1774 commits not yet on origin cherry-picked (the README fix, the TASK-1913 note and TASK-1922 went out as c312d9ff2 and 877476947), the task file merged by hand (my notes plus origin's wording of the README note, nothing of origin dropped), version beta.238 by bump-prerelease.sh. Re-verified: harness --old-rev 97eacc37b FIX PASS, OLD fails S1-S8/T1/T4/T6/T9/T10; 9 targeted test files PASS; evaluate --quick 85/0.
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
