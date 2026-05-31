---
id: TASK-789
title: >-
  Fix watchdog reboot loop and dead LED: getParam variadic over-read in
  DECLARE_TIMER jitter macros
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 19:08'
updated_date: '2026-05-31 19:12'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Commit 7199e158 (TASK-786) added jitter to the DECLARE_TIMER_* macros via getParam(3, __VA_ARGS__, 0). getParam reads i+1 varargs; for the common 1-2 arg timer callers, getParam(3,...) reads one past the supplied varargs = undefined behaviour, returning stack/register garbage that becomes jitter_max in random(0, garbage). This corrupts every no-jitter timer's initial _due, including feedWatchDog()'s timerWD (100ms I2C hardware-watchdog feed) and timerWDBlink (1000ms LED1 blink). Result on hardware: watchdog never fed -> hardware reset -> reboot loop; LED never toggles -> appears dead. Runtime UB, invisible to build.py/evaluate.py. Fix: pad the sentinel so getParam(2/3) never read past the varargs even for a 1-vararg caller.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DECLARE_TIMER_MIN/SEC/MS macros pad the getParam(2) and getParam(3) jitter calls with enough trailing-zero sentinels that index 3 is always backed by a real argument for every existing caller (1, 2, or 4 varargs)
- [x] #2 Intended jitter caller (timer5min, 4 varargs) still receives its 30000/60000 jitter bounds unchanged
- [x] #3 A comment documents why the trailing-zero padding is required so a future edit cannot silently reintroduce the over-read
- [x] #4 python build.py exits 0 (firmware + filesystem)
- [x] #5 python evaluate.py --quick shows no new failures
- [ ] #6 On-device (authoritative): flashed firmware blinks LED1 ~1/s, uptime climbs without reboot loop, no 'Reset by External WD' on subsequent boots
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the watchdog reboot loop + dead LED introduced by commit 7199e158 (TASK-786 jitter).

Root cause: DECLARE_TIMER_MIN/SEC/MS appended jitter via getParam(2/3, __VA_ARGS__, 0). getParam walks i+1 varargs, so getParam(3,...) reads the 4th vararg, but most timers pass only 1-2 varargs + one sentinel zero. getParam(3) therefore read one past the varargs (UB) and returned stack garbage as jitter_max; random(0, garbage) shoved each timer's initial _due far into the future. feedWatchDog()'s timerWD (100ms I2C watchdog feed) and timerWDBlink (1000ms LED) were both corrupted -> watchdog unfed (hardware reset -> reboot loop) and LED never toggled. Runtime UB: build.py and evaluate.py both passed; only manifests on hardware.

Fix (src/OTGW-firmware/safeTimers.h): padded the getParam(2)/getParam(3) jitter calls in all three DECLARE_TIMER_* macros with three trailing zeros so index 3 always resolves to a real 0 even for a 1-vararg caller. Verified by varargs counting for 1-, 2-, and 4-vararg callers: all reads now in-bounds; timer5min still gets 30000/60000. Added an anti-regression comment on __JitterOffset__.

Validation:
- python build.py (no flags): exit 0, firmware (0.71 MB) + LittleFS (1.98 MB) produced (1.6.1-beta+f36c298).
- python evaluate.py --quick: 0 failures, 34/34, 100%.
- Committed 7e73d79f, pushed to origin/dev.

BLOCKING AC (left In Progress): AC#6 is the only authoritative test for this class of bug (runtime UB invisible to build/lint). Requires flashing to a USB-connected ESP and confirming LED1 blinks ~1/s, uptime climbs without reboot loop, and no 'Reset by External WD' on subsequent boots. Must ship in the same beta build as the TASK-788 MQTT revert (every recent dev build bricks until this lands). No cross-worktree port: 2.0.0 does not carry the jitter commit.
<!-- SECTION:FINAL_SUMMARY:END -->
