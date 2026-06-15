---
id: TASK-878
title: >-
  fix(mqtt): low-severity review cleanups (snprintf_P, otgwcmd guard, stale
  comments)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-15 14:32'
updated_date: '2026-06-15 20:07'
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
- [ ] #4 Build green 3 targets; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
LOWs: 4 bare snprintf->snprintf_P+PSTR; otgwcmd[51] truncation guarded at both sinks; stale ESP8266 re-entrancy + 10-min timer comments corrected.
Implemented on branch claude/mqtt-reliability-phase3 (off feature-2.0.0-esp32s3-async). evaluate.py --quick green (0 failures). ESP32 build NOT verifiable in this container (network policy blocks PlatformIO framework-arduinoespressif32 download); build + field ACs left for maintainer verification.
<!-- SECTION:NOTES:END -->
