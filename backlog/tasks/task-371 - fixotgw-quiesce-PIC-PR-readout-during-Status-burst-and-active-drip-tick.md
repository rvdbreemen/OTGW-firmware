---
id: TASK-371
title: 'fix(otgw): quiesce PIC PR-readout during Status-burst and active drip tick'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 18:41'
updated_date: '2026-10-12 00:04'
labels:
  - code-review
  - mqtt
  - quality-of-life
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field logs (2026-04-21, 1.4.1-beta+deaddd8) show queryNextPIC emits 13 PR=X commands over ~40s at boot. Each response triggers 2 MQTT publishes (otgw-pic/settings/<name> + event_report). Several land inside Status-burst windows and on drip-publish ticks, amplifying heap pressure (canPublishMQTT dropped 4/5/7/11/17 msgs) and worsening TASK-370 oscillation. Reuse the existing isStatusBurstActive() signal (same pattern drip already uses since commit 837e8600) to defer queryNextPIC when a burst is active or drip is about to publish.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 queryNextPIC returns early when isStatusBurstActive() is true
- [x] #2 queryNextPIC returns early when drip is due within the next 500ms
- [x] #3 Expose dripDueWithinMs(uint32_t) as a narrow public API in MQTTstuff.h (mirrors isStatusBurstActive pattern)
- [ ] #4 Field log capture confirms no PR-readout response publish lands within 20ms of a status_master or status_slave publish during the first 60s after broker connect
- [x] #5 Build passes python build.py --firmware without new warnings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add static uint32_t sDripDueAtMs = 0 at file scope in MQTTstuff.ino
2. In loopMQTTDiscovery(), after DECLARE_TIMER_SEC: sDripDueAtMs = timerDiscoveryDrip_due
3. Implement dripDueWithinMs(uint32_t windowMs) using sDripDueAtMs in MQTTstuff.ino
4. Declare bool dripDueWithinMs(uint32_t) in OTGW-firmware.h alongside isStatusBurstActive()
5. Add two early returns to queryNextPICsetting() after existing flash/PIC guards:
   if (isStatusBurstActive()) return;
   if (dripDueWithinMs(500)) return;
6. Build + check ACs 1-3 and 5; AC4 requires field log
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added static uint32_t sDripDueAtMs at file scope in MQTTstuff.ino.
Added dripDueWithinMs(uint32_t windowMs): returns true if (sDripDueAtMs - millis()) <= windowMs (overdue included).
loopMQTTDiscovery() updates sDripDueAtMs = timerDiscoveryDrip_due on every call (after DECLARE_TIMER_SEC).
dripDueWithinMs() declared in OTGW-firmware.h alongside isStatusBurstActive().
queryNextPICsetting(): two early returns added after flash guards:
  if (isStatusBurstActive()) return;
  if (dripDueWithinMs(500)) return;
Deferred queries retry on next 3s tick (picSettingsQueryIdx not incremented on early return).
Build: python build.py --firmware passed (exit 0, no new warnings).
AC4 (field log capture) left unchecked: requires hardware observation.

2026-10-12 Classic-S3 bench validation (alpha.418 combo on OTGW Classic + LOLIN S3 Mini, PIC16F1847 gw 6.8): Bench evidence review 2026-10-12 (C:/Users/rvdbr/AppData/Local/OTGW-capture/classic-validation-20261011/EVIDENCE.md, Classic carrier + LOLIN S3 Mini, PIC16F1847 gateway 6.8, esp32-combo 2.0.0-alpha.418): AC#4 is NOT covered. The run checked PIC-path MQTT (6 TASK-410 retained ghosts gone after a reconnect; discovery stat_t paths; netem max loop gap 38 ms) and manual PR=A/M/V answers (REST queued; port 25238 for 180 s with no NG/SE). It made no timestamped capture of queryNextPIC PR-readout publishes against status_master/status_slave publishes in the first 60 s after broker connect, so it gives no 20 ms proximity result. The AC also concerns the 1.x ESP8266 line (1.4.1-beta field logs), and this bench ran 2.0.0. Closing AC#4 needs a 1.x capture-mqtt-debug transcript from boot with per-publish timestamps. Note that the task's status is already Done while AC#4 is still unchecked.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Quiesce PIC PR= readout during Status-burst fanouts and imminent drip ticks.

Why:
Field logs (2026-04-21, 1.4.1-beta+deaddd8) showed 13 PR=X commands over ~40s at boot. Each PR: response triggers 2 MQTT publishes (otgw-pic/settings/<name> + event_report). Several landed inside Status-burst windows and on drip ticks, amplifying heap pressure and worsening TASK-370 oscillation.

Changes:

MQTTstuff.ino:
- Added static uint32_t sDripDueAtMs = 0 at file scope (updated by loopMQTTDiscovery on every call).
- Added bool dripDueWithinMs(uint32_t windowMs): returns true when the drip fires within windowMs ms or is overdue. Uses signed comparison (long)(sDripDueAtMs - millis()) <= (long)windowMs for millis() rollover safety.
- loopMQTTDiscovery(): sDripDueAtMs = timerDiscoveryDrip_due after DECLARE_TIMER_SEC.

OTGW-firmware.h:
- Declared bool dripDueWithinMs(uint32_t windowMs) alongside isStatusBurstActive().

OTGW-Core.ino, queryNextPICsetting():
- Added two early returns after existing flash/PIC guards:
  if (isStatusBurstActive()) return;
  if (dripDueWithinMs(500)) return;
- Deferred queries are not skipped: picSettingsQueryIdx is not incremented on early return, so the same setting retries on the next 3s tick.

Build: python build.py --firmware passed, no new warnings.

AC4 (field log: no PR-readout publish within 20ms of status_master/status_slave during first 60s): unchecked, requires hardware observation.
<!-- SECTION:FINAL_SUMMARY:END -->
