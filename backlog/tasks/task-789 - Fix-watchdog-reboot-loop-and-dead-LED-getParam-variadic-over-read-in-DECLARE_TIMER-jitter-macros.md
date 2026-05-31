---
id: TASK-789
title: >-
  Fix watchdog reboot loop and dead LED: getParam variadic over-read in
  DECLARE_TIMER jitter macros
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 19:08'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Commit 7199e158 (TASK-786) added jitter to the DECLARE_TIMER_* macros via getParam(3, __VA_ARGS__, 0). getParam reads i+1 varargs; for the common 1-2 arg timer callers, getParam(3,...) reads one past the supplied varargs = undefined behaviour, returning stack/register garbage that becomes jitter_max in random(0, garbage). This corrupts every no-jitter timer's initial _due, including feedWatchDog()'s timerWD (100ms I2C hardware-watchdog feed) and timerWDBlink (1000ms LED1 blink). Result on hardware: watchdog never fed -> hardware reset -> reboot loop; LED never toggles -> appears dead. Runtime UB, invisible to build.py/evaluate.py. Fix: pad the sentinel so getParam(2/3) never read past the varargs even for a 1-vararg caller.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 DECLARE_TIMER_MIN/SEC/MS macros pad the getParam(2) and getParam(3) jitter calls with enough trailing-zero sentinels that index 3 is always backed by a real argument for every existing caller (1, 2, or 4 varargs)
- [ ] #2 Intended jitter caller (timer5min, 4 varargs) still receives its 30000/60000 jitter bounds unchanged
- [ ] #3 A comment documents why the trailing-zero padding is required so a future edit cannot silently reintroduce the over-read
- [ ] #4 python build.py exits 0 (firmware + filesystem)
- [ ] #5 python evaluate.py --quick shows no new failures
- [ ] #6 On-device (authoritative): flashed firmware blinks LED1 ~1/s, uptime climbs without reboot loop, no 'Reset by External WD' on subsequent boots
<!-- AC:END -->
