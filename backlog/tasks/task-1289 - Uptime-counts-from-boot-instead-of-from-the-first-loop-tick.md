---
id: TASK-1289
title: Uptime counts from boot instead of from the first loop tick
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T04:09:00Z'
updated_date: '2026-10-22T05:54:00Z'
labels:
  - bug
  - wifi
  - diagnostics
dependencies: []
priority: medium
ordinal: 252000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
state.uptime.iSeconds is a tick counter in doTaskEvery1s (OTGW-firmware.ino:236) with SKIP_MISSED_TICKS. It starts only when loop() runs and never recovers a skipped second. The RTC diagnosis in TASK-1262 measured it at 53 against ~86 s real on a normal boot and 26 against ~228 s real on a boot that waited for WiFi in setup(). The value goes to Home Assistant (MQTT uptime), the REST API (runtime.uptime_sec), the web UI and the telnet banner, and anyone who derives a restart time from it is misled (it misled the TASK-1262 analysis itself). Derive it from micros64() instead. The boot-command gate (TASK-1121) uses the counter as a 3-second grace of loop time for the PIC banner; it keeps a loop-relative counter.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Uptime reported by /api/v2/device/info runtime.uptime_sec matches wall time since the reset within 2 s after a normal boot, measured from the laptop RTS reset; old build shows the deficit on the same test
- [x] #2 Uptime matches wall time since the restart within 5 s after a WiFi supervisor restart whose boot waits for WiFi in setup(); old build shows the deficit on the same test
- [x] #3 Boot commands are still sent after the PIC banner is read or after 3 s of loop time, not at the first loop tick (TASK-1121 grace kept)
- [x] #4 Uptime does not wrap at 49.7 days (micros64-based, no 32-bit millis wrap)
- [x] #5 python build.py via build.bat succeeds and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-22 (bench .88.68, scratchpad t1278/t1289_rts.sh, t1289_deny.sh):
- AC#1 RTS reset, laptop time bracket before/after the pulse: old 1251039 deficit 6.8-7.5 s (constant: the counter missed setup time, no lost ticks); fix b266cb6 1.2-2.0 s after the pulse (whole-second reading refreshed once per second).
- AC#2 deny 540 s on fix b266cb6: boot computed from uptime = deny+304.5 s; expected supervisor restart deny+~304 s (TASK-1262 RTC diag: decision loss+300.0, boot +3 s). Old build on the same path showed uptimeSec 26 vs ~228 s real (TASK-1262 WDiag).
- AC#3 fix: telnet shows "Current firmware type: gateway" at 07:28:22.08, then "OTGW boot message = [PR=A]" at 07:28:22.47 and the PIC answered "PR: A=OpenTherm Gateway 6.8". Gate uses loopSeconds, incremented where the old counter was, so the grace is the same 3 s of loop time.
- AC#4 by source only (not run for 49.7 days): core 2.7.4 micros64() is a 64-bit count with an overflow tick every 60 s (core_esp8266_wiring.cpp:184); seconds in uint32 wrap after 136 years.
- AC#5 build.bat "Build completed successfully" (bins 06:14:55), evaluate.py --quick 37/39 pass, 0 fail.
- Commit b7c77fd56. 2.0.0 tree has the same tick counter (OTGW-firmware.ino:759); not ported (needs the cross-worktree master-plan step).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The uptime now counts from boot instead of from the first loop tick.

state.uptime.iSeconds was a tick counter in doTaskEvery1s(), so it missed the time setup() spends (about 7 s normally, up to 6 min when the boot waits for WiFi). Every consumer (MQTT uptime, REST runtime.uptime_sec, web UI, telnet banner, nightly-restart and discovery-verify gates) and anyone deriving a restart time from it were off by that much; it misled the TASK-1262 analysis. It now reads micros64()/1e6. The TASK-1121 boot-command gate keeps a loop-relative counter so its 3 s grace for the PIC banner is unchanged.

Tests: bench RTS reset old 6.8-7.5 s behind, fix 1.2-2.0 s; supervisor restart dated at deny+304.5 s by the uptime (expected ~304); boot command sent after the banner; build + evaluator green.

Not done: the 2.0.0 line has the same counter (not ported).
<!-- SECTION:FINAL_SUMMARY:END -->
