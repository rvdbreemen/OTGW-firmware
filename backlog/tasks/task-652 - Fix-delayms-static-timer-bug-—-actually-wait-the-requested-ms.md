---
id: TASK-652
title: Fix delayms() static-timer bug — actually wait the requested ms
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 20:32'
updated_date: '2026-05-21 20:32'
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
- [ ] #1 delayms(N) loops with doBackgroundTasks() for at least N ms across multiple calls
- [ ] #2 blinkLED() at setup() produces visible LED blinks (boot signature restored)
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures
- [ ] #5 Prerelease bumped and staged with the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace static DECLARE_TIMER_MS inside delayms() with a local millis()-based loop.
2. Bump prerelease beta.14 -> beta.15.
3. python build.py --firmware.
4. python evaluate.py --quick.
5. Commit on the audit branch and push; PR #617 already covers the broader audit, so this rides along.
<!-- SECTION:PLAN:END -->
