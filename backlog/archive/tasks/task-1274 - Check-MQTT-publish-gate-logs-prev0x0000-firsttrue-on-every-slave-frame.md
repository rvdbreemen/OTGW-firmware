---
id: TASK-1274
title: 'Check: MQTT publish gate logs prev=0x0000 first=true on every slave frame'
status: Done
assignee:
  - '@claude'
created_date: 2026-10-20 18:18
updated_date: '2026-10-22T03:31:00Z'
labels:
  - mqtt
  - investigate
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 404000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
First real SAT field run (gruijter, Intergas HReco 30 + Honeywell Round + PIC Classic, alpha.439/441, #alpha-testing 2026-10-20), captures in %TEMP%\discord-1020. In the telnet log the MQTT gate prints 'src=S slot=1 prev=0x0000 first=true' on every A/B frame, as if the previous value is never stored. Not verified; may cause redundant publishes or just a misleading log line.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cause explained with file:line; fixed if it is a real defect (old-vs-fix publish count)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 analysis (main session). Cause, OTGW-Core.ino: shouldPublishMQTTForID() (~2035) treats a slot as first-seen while its packed time is TRACKED_TIME_UNSEEN, opens the gate and installs mqttPendingSlot; processOT() (~5281-5286) commits the slot only when decodeAndPublishOTValue() raised mqttSendSuccessCount, else drops the pending (ADR-104 item 7). A frame that publishes nothing therefore never commits its slot and logs first=true ... => publish [tracked update] on every occurrence. Field capture (gruijter, %TEMP%\discord-1020, 1237 gate lines, 611 first=true): every repeated first=true belongs to a frame that publishes nothing: thermostat Read-Data requests (id 17/25/9/5/57/18/28 M), boiler Write-Ack without an echo value (id 1 S 111x, 16/24 S), Data-Invalid (id 5 S), Unknown-Data-Id (id 14 S), gateway R requests. So NOT a redundant-publish defect: nothing is sent for those frames. Only the log misleads. Change: one CoreMQTTDebugTf line in the else branch ('MQTT gate id=N src=X => nothing published, slot stays unseen'), debug-gated, no publish behaviour change. Bench red (alpha.450 firmware, /simulate replay, telnet g, 90 s): 120 gate lines, 52 first=true, 0 explanation lines; 179 MQTT value messages in 95 s.

2026-10-22 GREEN (alpha.453+69ff4fc build of this diff, safeboot OTA, settings diff empty; same replay + telnet g, 90 s, steady state): 120 gate decisions, 52 first=true (same as red), 56 'MQTT gate id=N src=X => nothing published, slot stays unseen' lines (52 first=true plus a few changed=true frames that publish nothing). Publish logic unchanged: the diff adds only a debug-gated log call in the existing else branch; value messages in 95 s were 179 (red) vs 200 (green), within replay-phase/heartbeat variance, not a code effect. evaluate --quick 72/0, build.bat SUCCESS. No review team: single debug log line, no behaviour change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not a defect in publishing. The MQTT gate marks a slot as seen only after a frame really published something (ADR-104 pending-slot commit), and frames that publish nothing (thermostat Read-Data requests, boiler Write-Acks without an echo value, Data-Invalid, Unknown-Data-Id) never commit, so they log first=true ... => publish on every occurrence. Evidence: all 611 first=true lines in gruijter's field capture belong to such frames; nothing is sent for them, so no redundant publishes. The only problem was the log; it now adds 'nothing published, slot stays unseen' for these frames (OTGW-Core.ino processOT, debug-gated). Bench: red 52 first=true with no explanation; green 52 first=true with 56 explanation lines. Not done: the PS=1 path (OTGW-Core.ino ~4516) has the same drop without a log line; left alone, PS=1 is not used on the bench and was not in the report.
<!-- SECTION:FINAL_SUMMARY:END -->
