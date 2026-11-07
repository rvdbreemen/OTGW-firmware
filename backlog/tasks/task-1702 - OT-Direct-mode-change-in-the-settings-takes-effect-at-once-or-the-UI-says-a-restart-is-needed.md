---
id: TASK-1702
title: >-
  OT-Direct mode change in the settings takes effect at once or the UI says a
  restart is needed
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T17:05:00Z'
updated_date: '2026-11-07T20:17:00Z'
labels:
  - otdirect
  - webui
  - field-report
dependencies: []
priority: medium
ordinal: 815000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-07 14:01 UTC: after switching OT-Direct mode to Gateway the monitor page still showed MODE: MASTER; after a reboot it was in Gateway mode. Verify in code whether the mode change is applied live; then either apply it live or show that a restart is needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Code path for a live OT-Direct mode change verified and documented in the notes
- [x] #2 After saving a new OT-Direct mode the monitor page shows the mode the bus actually runs in, or the settings page states that a restart is needed
- [x] #3 Bench evidence on an OT-Direct unit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause (code read, lane-1701 7942e30ab): the v2 Settings save POSTs /api/v2/settings per dirty key (v2.js saveSettings) -> updateSetting("OTDmode") in settingStuff.ino only wrote settings.otd.iMode. The running mode otCurrentMode (OTDirect.ino) is read from the setting only in initOTDirect() at boot, so the bus kept its mode until the reboot. The monitor MODE chip follows otdirect/status / device-info otdmode, both from state.otd.eMode = otCurrentMode (updateOTDirectStatus, 1 s), so it correctly showed MASTER. Only the GW= command path (setOTDirectMode -> applyOTDirectMode) switched live.

Fix (live apply, uncommitted in wt-1702): updateSetting OTDmode branch now calls otdModeSettingChanged(), which only raises a volatile flag (the POST runs on the async_tcp task). loopOTDirect() (loop task, only when isOTDirectEnabled(), so a combo board on PIC is untouched) consumes the flag when no master request is in flight, maps the stored value with the new otdModeFromSetting() (same bypass-needs-relay rule as boot, now shared with initOTDirect) and calls applyOTDirectMode() only if it differs from otCurrentMode. It compares against the running mode, not the stored one, so choosing Gateway after the boot auto-detect picked Master switches back (the no-op skip in updateSetting does not hide that, the flag is raised in the cascade before it). readSettings() clears the flag so the boot file read never applies (TASK-1694 auto-detect stays RAM-only). setOTDirectMode() (GW=) applies once; the loop then finds no difference. Live apply is the same applyOTDirectMode() the GW= command already uses at runtime, so no new hardware path.

Evidence: new tests/test_otd_mode_live_apply.py compiles real code cut from the sources (OTDmode branch of updateSetting, readSettings OTD tail, head of loopOTDirect, setOTDirectMode, the new helpers) with a recording applyOTDirectMode stub. Fix: python tests/test_otd_mode_live_apply.py -> Ran 7, OK. Old: OTGW_SRC_DIR=<dir with git show lane-1701 OTDirect.ino + settingStuff.ino> -> Ran 7, FAILED (failures=5): saved mode not applied (3 != 1), Gateway after auto-detect Master not honoured, bypass-without-relay, in-flight wait, apply-once. Mutants: removing the readSettings cancel -> 1 fail (test_boot_read_does_not_apply); removing the in-flight guard -> 1 fail (test_waits_for_a_master_request_in_flight).

Gates: python evaluate.py --quick -> 91 checks, 84 passed, 0 failed, 0 warnings, 7 info. Related tests OK: test_otd_autodetect_no_persist (3), test_otd_gateway_own_status (6), test_otd_ch_override (9), test_otd_controller_removed (2), test_telnet_console_setting (10), test_fs_update_keep (37), test_board_detect_persist_gate (7), test_telnet_setup_gate (4). test_otd_status_relay: 9 errors, identical with my changes stashed (pre-existing on lane-1701, the compiled exe fails at run time; not caused by this change).

Not verified: no firmware build (forbidden this run, memory 86%), so the cross-file prototype/ordering of otdModeSettingChanged/Cancel is only checked by reading; no bench (AC3 open: save Gateway on an OT-Direct unit in Master, watch the MODE chip switch within ~1-2 s without reboot, and telnet shows 'OTD: mode -> gateway'). For Robert: with OTDautodetect on and no thermostat, an explicit Gateway choice still turns into Master on the next boot (TASK-1694 behaviour); the settings select then shows Gateway while the bus runs Master. Whether that honours the user's choice enough is his call; not changed here.

AC2 left open on purpose: the host test proves the bus mode follows the saved setting; that the monitor MODE chip then shows it (otdirect/status poll -> setOtdMode) is only checked by reading v2.js and needs the bench/browser check together with AC3.

Robert 2026-11-07: (1) the UI shows the running mode when it differs from the stored one and lets the user apply the stored mode; (2) every OTDmode write applies live, also with the same value (accepted). Lead added: v2.js otdRunningNote()/otdRunningDiffers() and markDirty keeps otdmode dirty while the running mode differs; tests/webui/otd-mode-running-note.test.mjs (real v2.html/v2.js in headless Chrome, fake device running Master with Gateway stored): old 4 of 5 FAIL, fix 5 of 5 PASS, control (running = stored) 3 of 3 PASS. Verifier minors fixed: prototypes in OTGW-firmware.h under HAS_DIRECT_OT; the pending flag uses __atomic release/acq_rel (the project idiom, SATweather.ino), MSVC stand-ins in the host test. tests/test_otd_mode_live_apply.py after that: fix 7 OK, old 5 failures. evaluate --quick 0 failed.

Pushed origin/dev 54757840a (beta.129). Build at 6d0307e (pre-rebase, rebase only added CI commit a3ff23645): Build completed successfully. Bench .61 (OT-Direct, stored and running Master): OTA app+fs OK, 164 settings unchanged; POST otdmode=1 -> device/info otdmode gateway within 3 s, POST otdmode=3 -> master within 3 s, bootcount 113 throughout (no reboot), no crashlog, settings identical to before afterwards. AC3 met. Zip post: covered by the next zip (beta.130 from otgw-firmware-6b), memory 86%.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: saved OTDmode applies live (loopOTDirect flag, applyOTDirectMode); tests/test_otd_mode_live_apply.py old 5/7 fail, fix 7/7. AC2: settings page shows the running mode and a Switch button when it differs; tests/webui/otd-mode-running-note.test.mjs old 4/5 fail, fix 5/5. AC3: bench .61 beta.129 live master->gateway->master within 3 s each, no reboot, settings unchanged. Commit 54757840a.
<!-- SECTION:FINAL_SUMMARY:END -->
