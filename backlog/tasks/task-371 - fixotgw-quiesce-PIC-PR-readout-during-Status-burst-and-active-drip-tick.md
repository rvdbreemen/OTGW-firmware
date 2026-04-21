---
id: TASK-371
title: 'fix(otgw): quiesce PIC PR-readout during Status-burst and active drip tick'
status: To Do
assignee: []
created_date: '2026-04-21 18:41'
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
- [ ] #1 queryNextPIC returns early when isStatusBurstActive() is true
- [ ] #2 queryNextPIC returns early when drip is due within the next 500ms
- [ ] #3 Expose dripDueWithinMs(uint32_t) as a narrow public API in MQTTstuff.h (mirrors isStatusBurstActive pattern)
- [ ] #4 Field log capture confirms no PR-readout response publish lands within 20ms of a status_master or status_slave publish during the first 60s after broker connect
- [ ] #5 Build passes python build.py --firmware without new warnings
<!-- AC:END -->
