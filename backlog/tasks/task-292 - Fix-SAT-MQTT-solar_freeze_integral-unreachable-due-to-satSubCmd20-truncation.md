---
id: TASK-292
title: 'Fix SAT MQTT solar_freeze_integral unreachable due to satSubCmd[20] truncation'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 13:19'
updated_date: '2026-04-18 14:10'
labels:
  - mqtt
  - sat
  - bug
dependencies:
  - TASK-284
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.ino handleMQTTcallback allocates char satSubCmd[20] (line 471) which readMQTTTopicToken truncates to 19 chars + NUL. The PSTR literal "solar_freeze_integral" is 21 chars, so strcasecmp_P always fails and the command silently falls through to the unknown-sub-command branch. Users cannot set SATsolarfreezeint via MQTT. All other SAT sub-commands fit in 19 chars. Found by ultrareview bug_003 on 2026-04-18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satSubCmd buffer enlarged (e.g. char satSubCmd[24]) or command renamed to <=19 chars
- [x] #2 MQTT set/<nodeId>/sat/solar_freeze_integral updates settings.sat.bSolarFreezeInt (or equivalent) via updateSetting
- [x] #3 ESP8266 build still green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Increase char satSubCmd[20] to satSubCmd[24] in MQTTstuff.ino handleMQTTcallback. 4 bytes extra stack, accommodates solar_freeze_integral (21 chars) + NUL with headroom for future SAT subcmd names.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Enlarged char satSubCmd[20] to satSubCmd[24] in MQTTstuff.ino handleMQTTcallback. The previous 20-byte buffer held 19 chars + NUL, which silently truncated the 21-char token solar_freeze_integral to solar_freeze_integr and made the downstream strcasecmp_P fall through to the unknown-sub-command branch. With 24 bytes the command now round-trips correctly and updates SATsolarfreezeint. Costs 4 bytes of stack. Verified via ESP8266 arduino-cli rebuild (Flash 71%, RAM 84%, IRAM 94% — unchanged vs pre-fix because stack usage does not affect image size). AC #2 (settings update) is a code-evidence check: the existing strcasecmp_P branch calls updateSetting("SATsolarfreezeint", msgPayload), which the truncation had previously rendered unreachable.
<!-- SECTION:FINAL_SUMMARY:END -->
