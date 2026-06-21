---
id: TASK-901
title: Fix 1.6.x heap-fragmentation instability (delay/yield mainloop regression)
status: To Do
assignee: []
created_date: '2026-06-21 23:31'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ROOT CAUSE (bisected from OTGW-logs/bisect-testset transcripts). Failure signature per README: largest CONTIGUOUS heap block (maxBlock) floor collapses while free heap looks fine -> alloc failure -> Exception/SW-WDT reboot. Quantified p05 maxBlock floor per build: 1.3.5=10456, 1.5.0=5616, beta.13=4040 (last GOOD) -> beta.16=1296 + 12 exceptions (first BAD). 1.7.0=728. The ONLY functional commit in beta.13(a777a73)..beta.16(c30afb9) is 05e777bf 'perf(mainloop): audit+fix delay/delayms (TASK-651/652)'. It (1) replaced delay(1) with yield() at end of doBackgroundTasks() [primary: uncaps the ~1000 Hz loop], and (2) fixed delayms() from an accidental near-no-op (buggy DECLARE_TIMER_MS static) into a real busy-wait [secondary, only caller is blinkLED]. The old delayms bug was load-bearing/protective. TASK-651 AC#5 (field-validation under load) was shipped UNCHECKED. GOAL: restore >=1.3.5 stability (maxBlock p05 floor), minimize heap pressure, keep webserver responsive. Plan: reproduce via overload test (WS+HTTP+MQTT, ported 2.0.0 harness), confirm mechanism, fix root cause, validate floor recovers, agentic 15-min loop.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Overload test harness ported/adapted to otgw-1.x.x reproduces the maxBlock floor collapse on a BAD build
- [ ] #2 Root-cause mechanism confirmed (why uncapping the loop fragments the heap) with evidence, not assertion
- [ ] #3 Fix restores sustained maxBlock p05 floor to >=4000 (>=1.3.5-class) under the overload test
- [ ] #4 python build.py firmware exit 0 + evaluate.py --quick no new failures
- [ ] #5 No regression to webserver responsiveness that TASK-651 was trying to fix (or documented trade-off)
<!-- AC:END -->
