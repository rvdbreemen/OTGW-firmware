---
id: TASK-878
title: >-
  fix(mqtt): low-severity review cleanups (snprintf_P, otgwcmd guard, stale
  comments)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-15 14:32'
updated_date: '2026-06-20 11:31'
labels: []
dependencies: []
ordinal: 94000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTT review LOW batch. (1) 4x bare snprintf (non-_P) with RAM format-string literals at MQTTstuff.ino:2412,2482,2572,3086 -> snprintf_P + PSTR (PROGMEM rule). (2) PIC-command truncation guard incomplete: the 128-byte msgPayload guard (MQTTstuff.ino:822) does not protect the downstream otgwcmd[51] sink (snprintf_P at :976/:1008 silently truncates 51-127-char payloads) -> add a length check against sizeof(otgwcmd) before addCommandToQueue. (3) Stale ESP8266 cooperative re-entrancy comments on publishToSourceTopic (MQTTstuff.ino:1857-1858) + SATmqttPublish.cpp:11-14/.h:32 -> correct (ESP32: feedWatchDog does not yield, doAutoConfigure async, ADR-128 dropped ESP8266); guards stay precautionary. (4) Reconnect-wait timer comment says '10 minutes' but DECLARE_TIMER_SEC is 42s (MQTTstuff.ino:1054/1166/1183/1211/1224) -> fix comments.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bare snprintf sites converted to snprintf_P+PSTR
- [x] #2 otgwcmd truncation guarded against sizeof(otgwcmd)
- [x] #3 Stale ESP8266 re-entrancy + 10-minute-timer comments corrected
- [x] #4 Build green 3 targets; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
LOWs: 4 bare snprintf->snprintf_P+PSTR; otgwcmd[51] truncation guarded at both sinks; stale ESP8266 re-entrancy + 10-min timer comments corrected.
Implemented on branch claude/mqtt-reliability-phase3 (off feature-2.0.0-esp32s3-async). evaluate.py --quick green (0 failures). ESP32 build NOT verifiable in this container (network policy blocks PlatformIO framework-arduinoespressif32 download); build + field ACs left for maintainer verification.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Low-severity review cleanups complete. AC#1/#2 (snprintf_P, otgwcmd guard) landed in 1d91bbce (alpha.198). AC#3 (stale comments): the two reconnect-wait comments at MQTTstuff.ino:1360/1362 said '10 min'/'10-minute fallback' but the timer is timerMQTTwaitforconnect=42s (verified: DECLARE_TIMER_SEC line 1054; the 5-attempts-then-42s fallback at lines 1164-1167) — corrected to 42s at alpha.225. AC#4 build/eval: esp32-classic build green + evaluate.py --quick green (0 fail/1 warn/98.6%) at alpha.225; the change is platform-common (comment-only) so esp32/esp32-combo compile identically (full 3-target was green at alpha.224 with the same files). All ACs met.
<!-- SECTION:FINAL_SUMMARY:END -->
