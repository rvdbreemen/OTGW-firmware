---
id: TASK-652
title: Fix delayms() static-timer bug — actually wait the requested ms
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 20:32'
updated_date: '2026-05-22 06:39'
labels:
  - bug
  - led
  - boot
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
delayms() uses DECLARE_TIMER_MS which expands to static locals. Static vars are initialised once per program, so on the first call due > now and the while(DUE) loop exits immediately; on subsequent calls the interval is frozen at the first value ever passed and DUE fires once. Effect: blinkLED() at boot and on telnet 'b' don't actually blink — LED just rapid-toggles. The ADR-036 guard in doBackgroundTasks() already documents the intent that delayms blocks cooperatively, so the function is the regression, not its callers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 delayms(N) loops with doBackgroundTasks() for at least N ms across multiple calls
- [x] #2 blinkLED() at setup() produces visible LED blinks (boot signature restored)
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
- [x] #5 Prerelease bumped and staged with the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace static DECLARE_TIMER_MS inside delayms() with a local millis()-based loop.
2. Bump prerelease beta.14 -> beta.15.
3. python build.py --firmware.
4. python evaluate.py --quick.
5. Commit on the audit branch and push; PR #617 already covers the broader audit, so this rides along.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced the static-timer-based delayms() with a local-timestamp loop:

  void delayms(unsigned long delay_ms) {
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < delay_ms) {
      doBackgroundTasks();
    }
  }

Why it was broken: DECLARE_TIMER_MS expands to static uint32_t locals. They are init-once per program, so on the first call due was already millis()+interval (future) — DUE() returned 0 and the while loop never entered. On every subsequent call the static interval was frozen at the first value ever passed.

Effect of the fix:
- blinkLED(LED1, 3, 100) at setup() now produces visible 600 ms LED1 blink as designed.
- blinkLED(LED2, 3, 100) at end of setup() now produces visible 600 ms LED2 blink as designed.
- blinkLED(LED1, 5, 500) on telnet 'b' command now blinks for ~5 s as designed.
- Boot time increases by ~1.2 s — matches the original ADR-036 intent ("blinkLED/delayms in setup() would otherwise invoke handleMQTT() before startMQTT() sets the buffer", which implies the calls were expected to take real time).

Verification:
- python build.py --firmware: exit 0 (beta.15+f1b6ae0).
- python evaluate.py --quick: 34/34 passed, health 100%.

Shipped on the same branch as TASK-651, riding along on PR #617.
<!-- SECTION:FINAL_SUMMARY:END -->
