---
id: TASK-1431
title: MQTT feedback when a PIC command is dropped after 5 retries (Tweakers request)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T20:28:00Z'
updated_date: '2026-10-28T22:36:00Z'
labels:
  - enhancement
  - needs-maintainer-ok
dependencies: []
references:
  - https://gathering.tweakers.net/forum/list_message/86420402#86420402
  - https://gathering.tweakers.net/forum/list_message/86427540#86427540
priority: low
ordinal: 258000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
sjorsjuhmaniac (Tweakers, 2026-10-24/25, 1.7.7, came from ESPEasy + otgw2mqtt): after 5 failed attempts handleOTGWqueue() (OTGW-Core.ino ~3125) drops a command and only logs it (debug + WebSocket '!' event). He wants HA to see the failure, e.g. a TT=22.0 that never lands. Proposed one-line diff: sendMQTTData(F("command_retry_exceeded"), ON). Open design points he raised himself: which command failed, latching or not, how long, cleared by user ack or next successful command, non-persistent. Feature request, not a bug; needs maintainer decision on topic shape before work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Maintainer decided topic shape, payload (which command) and clear semantics
- [ ] #2 A dropped command produces an MQTT publish naming the command, verified on bench by forcing a retry exhaustion
- [ ] #3 TP= commands are never replaced in the command queue: each one is appended behind the others (exception to the two-letter dedup), verified on the bench by queueing two TP= commands back to back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 maintainer decision (Robert, in session): all three of the offered options, plus a queue rule.
- Event: non-retained publish on otgw-firmware/command_dropped, payload = the dropped command text (e.g. TT=22.0); counter cmd_dropped in /api/v2/device/info.
- HA: discovery sensor "Last dropped command" on that topic (diagnostic) and a binary sensor on the flag below.
- Latching flag: otgw-firmware/command_retry_exceeded ON (retained) on a drop, OFF on the next successful command. PR= readouts run every 3 s internally and are excluded from clearing it, or the flag would clear before anyone saw it.
- TP= commands are never replaced in the queue but appended behind each other (exception to the two-letter dedup in addOTWGcmdtoqueue).
Known limits: internal PR= polls that drop are reported too (they are real failures); a drop while MQTT is down is lost (TASK-1212).

2026-10-28 implemented: reportCommandDropped() in the retry-exhaustion branch of handleOTGWqueue() (counter state.otgw.iCmdDropped, non-retained otgw-firmware/command_dropped with the command text, latch otgw-firmware/command_retry_exceeded ON retained); clearCommandDropped() in checkOTGWcmdqueue() on a confirmed non-PR command; publishCommandDroppedState() on every MQTT (re)connect so a retained ON from before a reboot is replaced. TP= bypasses the two-letter dedup in addOTWGcmdtoqueue (appended, like forceQueue). HA discovery: pseudo-ID 241, sensor row appended at the end (MQTT_HA_SENSOR_COUNT 338, index 337) and binary row appended (MQTT_HA_BINSENSOR_COUNT 56, index 55), queued in publishNonOTDiscoveryConfigs(); always announced, no data gate needed. Binary-sensor discovery topic and uniq_id now sanitize the label like the sensor path does (a '/' would add a topic level); every existing binary label is already [A-Za-z0-9_-], checked by script, so no existing entity changes. cmd_dropped in /api/v2/device/info. MQTT.md, openapi.yaml, CHANGELOG updated.
Gates: build.bat 1.7.8-beta.8+628cb88 green, evaluate.py --quick 38/0, host tests 7/7.
NOT verified on hardware: no ESP8266 bench (maintainer, 2026-10-28); AC #2 and #3 stay open. Planned bench trigger once a bench exists: publish an unknown command (e.g. ZZ=1, PIC answers NG) on OTGW/set/<id>/command and watch command_dropped after 5 x 5 s. Field verification by sjorsjuhmaniac via beta.8.
<!-- SECTION:NOTES:END -->
