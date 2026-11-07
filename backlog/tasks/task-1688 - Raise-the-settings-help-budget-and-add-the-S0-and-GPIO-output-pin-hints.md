---
id: TASK-1688
title: Raise the settings help budget and add the S0 and GPIO output pin hints
status: To Do
assignee: []
created_date: '2026-11-07T11:05:00Z'
labels:
  - webui
  - docs
dependencies: []
priority: low
ordinal: 808000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: raise the settings-help budget. MAX_BYTES 18 KB in tests/webui/settings-help.test.mjs dates from when settings-help.json lived on LittleFS; since TASK-1294 (ADR-194) it is PROGMEM in the app slot (2,293,760 B, beta.118 uses 2,123,984 B) and is streamed from flash without a RAM copy (webSendPStream). Raise to 24 KB, fix the stale LittleFS comment in settingsHelp.h, and add the GPIO-not-hole hint to s0counterpin and gpiooutputspin (TASK-1662 follow-up).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 settings-help test limit 24 KB with the reason in the test comment, test passes
- [ ] #2 s0counterpin and gpiooutputspin help name the GPIO-not-hole rule
<!-- AC:END -->
