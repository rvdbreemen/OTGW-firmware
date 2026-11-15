---
id: TASK-1980
title: >-
  OT-Direct: pinned OpenTherm library to the fork branch fix-timer-wrap
  (upstream PR #1), OTDMasterLine workaround removed
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T16:03:00Z'
updated_date: '2026-11-15T16:41:00Z'
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
- [ ] #1 Library copy equals the fork branch exactly (git diff against rvdbreemen/fix-timer-wrap empty); source commit recorded in the submodule pointer, .gitmodules and the commit message, with the DM id
- [ ] #2 OTDMasterLine and otdMasterGuard removed; device/info keeps otdrequestactive and otdcmdringdropped; otdlinestalefixes/otdlinetxrestarts removed or fed from the library, choice noted
- [ ] #3 TX stall 10 ms checked against frame length and ISR latency (CONFIG_GPTIMER_ISR_IRAM_SAFE, flash writes), reasoning + evidence in the notes
- [ ] #4 test_otd_master_line_stale_delay rewritten on the real library code with fake micros() across 2^31 and 2^32 us: red on the old pin f7d8254, green on the new pin
- [ ] #5 Full test sweep and evaluate --quick green
- [ ] #6 Bench by the bench tester on an OT-Direct unit: >= 80 min without a boiler past the 2^31 us mark, frames keep going, commands run
- [ ] #7 Bench limit stated: the boiler-side rxIdleLevel change of upstream 720cee9 is NOT testable without a boiler; it is listed as untested in the alpha note
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC3 analysis (session 95, 2026-11-15, code + config read, no bench yet). Our libs sdkconfig (framework-arduinoespressif32-libs/esp32s3/qio_qspi/include/sdkconfig.h, memory_type qio_qspi per platformio.ini:26): CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM=1, CONFIG_GPTIMER_OBJ_CACHE_SAFE=1, CONFIG_GPTIMER_ISR_CACHE_SAFE not set, no SPI_FLASH_AUTO_SUSPEND. So the tx alarm interrupt is not IRAM-flagged and is deferred while the flash cache is off (a LittleFS sector erase, typically tens of ms). In that window process() cannot run either (loop task stalled). Normal operation: one tick per 500 us half bit, a frame is 68 ticks = 34 ms; the 10 ms limit is 20 missed ticks, far above ISR latency. A flash write in the middle of a frame stretches its bits, so the boiler gets a broken frame either way; with 10 ms the library ends it right after the write and reports TIMEOUT, with our 500 ms guard the stretched tail is sent and the 1 s WAIT_RESPONSE timeout follows. Both lose one frame; 10 ms recovers faster. Race to keep in mind: sendRequestAsync() sets requestTimestamp only after sendFrame() starts the timer; a process() on ANOTHER task in that gap would see TX_DATA with an old requestTimestamp and end the frame at once. Not reachable for us: sendRequestAsync() and process() both run on the loop task (ADR-242). To verify on the bench: otd frames during settings saves (flash writes) with the new library.

Robert chose A (DM 1571560593712746617, relayed by the coordinator): 'A and validate it on the bench best you can right now.' The fork branch is taken as is, including the 4 upstream commits. The boiler-side change of 720cee9 (idle-level detection also without Smart Power) cannot be tested on our bench (.61 has no boiler): that stays a field item.
<!-- SECTION:NOTES:END -->
