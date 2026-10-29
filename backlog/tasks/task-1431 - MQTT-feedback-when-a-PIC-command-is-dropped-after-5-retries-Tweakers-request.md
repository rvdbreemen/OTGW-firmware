---
id: TASK-1431
title: MQTT feedback when a PIC command is dropped after 5 retries (Tweakers request)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T20:28:00Z'
updated_date: '2026-10-29T05:14:00Z'
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
- [x] #2 A dropped command produces an MQTT publish naming the command, verified on bench by forcing a retry exhaustion
- [x] #3 TP= commands are never replaced in the command queue: each one is appended behind the others (exception to the two-letter dedup), verified on the bench by queueing two TP= commands back to back
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

2026-10-29 bench .68 on released beta.8 (+65762e7), temporary anonymous mosquitto on 192.168.88.32 as broker, all traffic recorded.
- Correction to the planned trigger: ZZ=1 does NOT exhaust retries. The PIC answers 'ZZ: NG', checkOTGWcmdqueue matches it on the two letters and removes it as confirmed (telnet_zz.log). Error replies (NG/BV/SE...) therefore count as a confirmation; see follow-up task.
- AC #2: PR=Z via MQTT command topic. PIC answers 'PR: BV' without the register letter, so the PR register match never hits: sent 5 times 06:09:15-06:09:35, then MQTT OTGW/value/<id>/otgw-firmware/command_dropped = PR=Z and command_retry_exceeded = ON (06:09:35), device/info cmd_dropped 1. The next confirmed command (TP=11:1, 06:09:54) set command_retry_exceeded OFF. HA discovery for sensor otgw-firmware_command_dropped ('Last Dropped Command') and binary_sensor otgw-firmware_command_retry_exceeded ('Command Dropped') was published on connect.
- AC #3 via REST POST /api/v2/otgw/commands (the MQTT path always uses forceQueue=true, so it never deduplicates and cannot show the difference): TT=20.5 then TT=21.5 -> 'Found cmd exists in slot [0]', only TT=21.5 sent (dedup kept). TP=11:1 then TP=12:1 -> slots 0 and 1, both sent 06:11:25 (TP: 11:1, TP: BV).
Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559\telnet_drop_tp.log, telnet_rest_tp.log, telnet_zz.log, mqtt_all.log.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A PIC command dropped after 5 unanswered tries is now reported instead of only logged (Tweakers request):
- MQTT otgw-firmware/command_dropped (event, not retained) carries the dropped command, e.g. PR=Z.
- otgw-firmware/command_retry_exceeded turns ON on a drop and OFF on the next confirmed non-PR command (internal PR= readouts do not clear it).
- HA entities on pseudo-ID 241: sensor 'Last Dropped Command' and binary_sensor 'Command Dropped'; binary-sensor discovery now sanitizes its label like the sensor path.
- cmd_dropped counter in /api/v2/device/info.
- TP= commands are never replaced in the queue; each is appended (exception to the two-letter dedup on the REST/internal path).

Shipped in v1.7.8-beta.8. Verification: build + evaluate.py green, host tests 7/7; bench .68 on the released binary proved the drop report (PR=Z, 5 tries, MQTT + counter + flag clear) and TP append vs TT dedup over REST. Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559.

Known limit, follow-up task: a PIC error reply such as 'ZZ: NG' counts as a confirmation, so a rejected command is neither reported nor retried.
<!-- SECTION:FINAL_SUMMARY:END -->
