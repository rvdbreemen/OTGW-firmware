---
id: TASK-1980
title: >-
  OT-Direct: pinned OpenTherm library to the fork branch fix-timer-wrap
  (upstream PR #1), OTDMasterLine workaround removed
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T16:03:00Z'
updated_date: '2026-11-15T21:07:00Z'
labels:
  - otdirect
  - upstream
  - bug
dependencies: []
priority: medium
ordinal: 1012000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert DM 1571553430923911252 (answer B to 1571553264380547104), relayed by the coordinator: explicit exception to the no-library-changes rule. Put rvdbreemen/opentherm_library branch fix-timer-wrap (commit 51aa68e, = Phunkafizer master 720cee9 + the TASK-1977 fix, PR Phunkafizer/opentherm_library#1) into src/libraries/OpenTherm (submodule URL to the fork) and remove OTDMasterLine/otdMasterGuard (TASK-1966). The branch also brings 4 upstream commits over our pin f7d8254: 3431b30 requestLowPower, c6a8dae setRequestDelay (default 50 ms = old constant), feb1459 setResponseTimeout (default 1 s = old), 720cee9 rxIdleLevel always follows the connected slave.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Library copy equals the fork branch exactly (git diff against rvdbreemen/fix-timer-wrap empty); source commit recorded in the submodule pointer, .gitmodules and the commit message, with the DM id
- [x] #2 OTDMasterLine and otdMasterGuard removed; device/info keeps otdrequestactive and otdcmdringdropped; otdlinestalefixes/otdlinetxrestarts removed or fed from the library, choice noted
- [x] #3 TX stall 10 ms checked against frame length and ISR latency (CONFIG_GPTIMER_ISR_IRAM_SAFE, flash writes), reasoning + evidence in the notes
- [x] #4 test_otd_master_line_stale_delay rewritten on the real library code with fake micros() across 2^31 and 2^32 us: red on the old pin f7d8254, green on the new pin
- [ ] #5 Full test sweep and evaluate --quick green
- [ ] #6 Bench by the bench tester on an OT-Direct unit: >= 80 min without a boiler past the 2^31 us mark, frames keep going, commands run
- [x] #7 Bench limit stated: the boiler-side rxIdleLevel change of upstream 720cee9 is NOT testable without a boiler; it is listed as untested in the alpha note
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC3 analysis (session 95, 2026-11-15, code + config read, no bench yet). Our libs sdkconfig (framework-arduinoespressif32-libs/esp32s3/qio_qspi/include/sdkconfig.h, memory_type qio_qspi per platformio.ini:26): CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM=1, CONFIG_GPTIMER_OBJ_CACHE_SAFE=1, CONFIG_GPTIMER_ISR_CACHE_SAFE not set, no SPI_FLASH_AUTO_SUSPEND. So the tx alarm interrupt is not IRAM-flagged and is deferred while the flash cache is off (a LittleFS sector erase, typically tens of ms). In that window process() cannot run either (loop task stalled). Normal operation: one tick per 500 us half bit, a frame is 68 ticks = 34 ms; the 10 ms limit is 20 missed ticks, far above ISR latency. A flash write in the middle of a frame stretches its bits, so the boiler gets a broken frame either way; with 10 ms the library ends it right after the write and reports TIMEOUT, with our 500 ms guard the stretched tail is sent and the 1 s WAIT_RESPONSE timeout follows. Both lose one frame; 10 ms recovers faster. Race to keep in mind: sendRequestAsync() sets requestTimestamp only after sendFrame() starts the timer; a process() on ANOTHER task in that gap would see TX_DATA with an old requestTimestamp and end the frame at once. Not reachable for us: sendRequestAsync() and process() both run on the loop task (ADR-242). To verify on the bench: otd frames during settings saves (flash writes) with the new library.

Robert chose A (DM 1571560593712746617, relayed by the coordinator): 'A and validate it on the bench best you can right now.' The fork branch is taken as is, including the 4 upstream commits. The boiler-side change of 720cee9 (idle-level detection also without Smart Power) cannot be tested on our bench (.61 has no boiler): that stays a field item.

Sweep (scripts/run_tests.py --changed origin/dev, 228 files, -j 4, 1951 s): 223 passed. Failed: test_capture_device_data (graphTrim not defined, TASK-1963 web UI; red on origin 2c4b462fd too), test_sat_no_return_startup (MsgID 25 gap, red on origin since fff3604f4 TASK-1954, d3), test_override_reply (ran before the commit: its pointer check compares the OpenTherm checkout with HEAD; after the commit 11/11 with the stale-delay test), test_otd_slave_reply_gate (sliced the library line 'delayTimestamp = ts + RESPONSE_TIME', which upstream c6a8dae changed to 'newTs + (isSlave ? RESPONSE_TIME : requestDelay)': harness accepts both forms now, 6/6). test_sat_lowload_contract hit the 1200 s timeout under -j 4; run alone next.

Build (session 95, 19:04): stack a1a1e4175 (beta.325) + 9dba9706b on origin/dev 0fa19cab1 plus 754243ca3/b807b7ba3; esp32-combo exit 0, OpenTherm.cpp recompiled from the new pin (lib6a8/OpenTherm/OpenTherm.cpp.o 19:01). App 2 136 464 B; sha256 ino 638853aa2d441317..., littlefs 696717fb747edd9e..., zip 2c56f93fa2bd03cb.... AC1: the committed submodule pointer is 51aa68e = rvdbreemen/opentherm_library origin/fix-timer-wrap, git diff against the branch empty; .gitmodules URL https://github.com/rvdbreemen/opentherm_library; source and DM ids in the commit message and in docs/c4/c4-code-otdirect.md (library.properties lives in the fork itself and stays as the PR has it). AC2: OTDMasterLine/keepFresh/otdMasterGuard removed, otdMasterStatus() mirrors the in-flight flag; device/info keeps otdrequestactive and otdcmdringdropped, drops otdlinestalefixes/otdlinetxrestarts (the library repairs silently, nothing left to count). AC4: test_otd_master_line_stale_delay 7/7 on the real library, old pin f7d8254 red in the same file. AC5 not ticked: the sweep is green except 3 files that are red on origin too (test_sat_no_return_startup/ll018h TASK-1954, test_capture_device_data fixed separately in b807b7ba3) and evaluate --quick 0 FAIL. Bench request (AC6/AC7) sent to 09 via the coordinator.

Bench 09 interim (beta.325 on .61, check #1): PASS. TT=/MM= REST 202 with correct replies, no 503; GW=R 3x each one ~3.27 s gap then MsgID 0 continues (no stall); 2 settings saves 200 while frames run; otdrequestactive toggles; otdcmdringdropped 0; bootcount stable 791; otdlinestalefixes/otdlinetxrestarts absent from device/info as intended. Check #2 ~20:47 (36-72 min window), final ~21:26.

Bench 09 check #2 (beta.325 on .61): PASS. Past 2^31 us (uptime 2147 s, ~20:30:30) the master keeps sending 49-50 R frames/min; PR=T answered every sample; otdcmdringdropped 0; otdrequestactive toggles. At uptime ~52 min TT=/MM=/GW=R all 202 with correct replies, frames continue (one 3.26 s gap at GW=R). Old code stopped sending at 2147.5 s on the same unit, so this is the old-vs-fix proof for fix 1 on hardware. Final check past 2^32 us ~21:26.

Bench 09 FINAL (beta.325+9dba970 on .61, 89 min, uptime 5337 s, past 2^31 and 2^32 us): PASS. R frames >=49/min in every sample except the three GW=R minutes (46-48); PR=T answered 96/96; otdcmdringdropped 0; otdrequestactive toggles; bootcount 791 constant; no new crashlog; settings diff empty. Not testable on the bench: the boiler-side idle-level change of upstream 720cee9 (no boiler), gateway mode with a thermostat (no thermostat sim). Evidence: 09 scratchpad b325/ (watch.log, telnet.raw).

Push (21:4x): restacked on origin/dev e22c59ed1, renumbered beta.325 -> beta.328 (9918e9b88 + b6c91b3f4). Content checked against the benched a1a1e4175 minus version banners: only build-stamp lines differ; submodule 51aa68e. After the rebase: test_capture_device_data + test_capture_tool + test_otd_master_line_stale_delay + test_otd_slave_reply_gate 86 passed 2 skipped; all tests/test_otd_*.py + test_pic_pm_chain green; evaluate --quick 87 passed 0 failed. Bench ran on beta.325+9dba970 (same firmware code under a different label, below it origin moved by d3/ba lanes).

Posted beta.328+c89f9b6 to #alpha-testing (Discord 1571632219645149195): zip sha256 44ef87d9b8d9ef47..., ino.bin 00d9408d610e75af...; app 2137699 B of 2293760 (93.2 %), safeboot 931648 B; build exit 0, built from pushed HEAD c89f9b6e6. AC7: the post line states as NOT tested the library's boiler-side idle-level change (upstream 720cee9, no boiler on the bench) and gateway mode with a thermostat (no thermostat sim). AC5 open: the full sweep has not been rerun on c89f9b6e6 (the targeted set and all test_otd_* are green; earlier 223/228 with the 5 red also red on origin). AC6 is 09's tick (final PASS in the notes above).
<!-- SECTION:NOTES:END -->
