---
id: TASK-901
title: Fix 1.6.x heap-fragmentation instability (delay/yield mainloop regression)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-21 23:31'
updated_date: '2026-06-22 00:23'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ROOT CAUSE pinned offline: commit 05e777bf (beta.15), delay(1)->yield() in doBackgroundTasks tail = primary (uncaps ~1kHz loop); delayms busy-wait = secondary (blinkLED cold, boot/manual only - grep-confirmed). Bisect refined via p05 maxBlock floor: beta.13(4040)=last GOOD -> beta.16(1296,12exc)=first BAD. LOAD-DEPENDENT: live zutphen units on 1.7.0-beta.4 survive 5d at low load; crash needs WS+HTTP load (matches README 'browser capture ON'). Hardware: bench ESP8266 MAC C8:C9:A3:5A:CB:08 on USB COM3 (CH340), NOT WiFi-joined -> needs flash+provision. Full plan/state/matrix in OTGW-logs/AB-campaign.md. 7-arm matrix locked (C0 yield, C1 delay1, C2 dus500, C3 dus500+yield, C4 dus250+yield, C5 dus250, CREV full-revert). Launched: harness scripts + 7-arm builds (background). 15-min autonomous loop armed.

HARDWARE BLOCKER (2026-06-22): device on COM3 = MAC 84:F3:EB:22:B8:E1 = the live 88.68 unit (1.6.1-beta, PIC fw 6.6 = real boiler), NOT the C8:C9:A3 bench unit. Flash blocked: esptool reads MAC but stub/--no-stub fail with 'serial noise or corruption' = active PIC streams OT into ESP RX, corrupts bootloader. NO write happened; recovered to run mode (88.68 health=200). Will NOT autonomously flash/overload a production gateway. Need user: confirm safe bench target + quiet PIC, or point to real bench unit, or accept offline-confirmed fix. esptool works via 'python -m esptool' (pip 4.8.1); flash_esp.py cp1252 bug + flash_otgw.bat Get-FileHash bug noted. Root cause already pinned offline; hardware A/B is validation, not discovery.
<!-- SECTION:NOTES:END -->
