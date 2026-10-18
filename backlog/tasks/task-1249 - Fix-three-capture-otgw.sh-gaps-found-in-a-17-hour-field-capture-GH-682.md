---
id: TASK-1249
title: 'Fix three capture-otgw.sh gaps found in a 17-hour field capture (GH #682)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 12:49'
updated_date: '2026-10-18 19:24'
labels:
  - tooling
  - capture
dependencies: []
priority: medium
ordinal: 245000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by mrfox7688 on GH #682 (16 Oct 2026 comment) after a 17 h capture with capture-otgw.sh 2.0.0 on a Synology NAS; each confirmed by reading scripts/capture-otgw.sh on otgw-1.x.x:
1. Half-open telnet is never detected. The capture loop reconnects only when the telnet helper process exits or when the ESP.restart() marker appears in the log (capture-otgw.sh:1551-1600). During the GH #682 hang the device went silent without a marker and the socket stayed half-open, so the capture had no device-side log of either recovery for about 4 hours. Needs an idle watchdog: no telnet bytes for N seconds while the reachability probe fails or the device answers HTTP again -> close and reconnect.
2. -BrokerHost is mandatory even when MQTT capture is impossible. Line 359 dies with 'BrokerHost is required.' before the mosquitto_sub check, so a non-interactive run on a host without mosquitto_sub cannot start without a dummy broker address.
3. The default debug-toggle policy ('enable all that are off', enable_all_debug_if_needed at :603) does not record what it switched on, so the teardown restores nothing and the device keeps every debug toggle on after the capture. Only -QuietDebugToggles records and restores (:623-669).
Check capture-mqtt-debug.bat for the same three gaps (parity, TASK-1161).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture whose telnet stream goes silent while the device is unreachable reconnects telnet once the device answers again, and the summary records the idle reconnect
- [ ] #2 A non-interactive run without -BrokerHost on a host without mosquitto_sub starts, captures telnet/probe/crashlog, and states that MQTT capture is skipped
- [ ] #3 After a default run the debug toggles the script switched on are switched back off, and the summary says so
- [ ] #4 The same three behaviours hold in capture-mqtt-debug.bat, or the gap there is recorded
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Approved by the maintainer 2026-10-18 (AskUserQuestion):
1. Telnet idle watchdog in capture-otgw.sh: no telnet bytes for more than 120 s while the HTTP probe fails, or while the device answers HTTP again after being silent -> close the telnet helper and reconnect; count idle reconnects and report them in the summary.
2. -BrokerHost optional: without a broker address or without mosquitto_sub the capture starts, captures telnet/probe/crashlog and states 'MQTT capture skipped'.
3. Default debug-toggle policy records which toggles it switched on and switches exactly those back off at teardown; the summary says which.
4. Same three in capture-mqtt-debug.bat, or the gap recorded.
Test on the COM3 bench unit .88.68: AC1 with the same MikroTik fault rule as TASK-1248 (old script during the old-firmware fault shows the gap, new script recovers); AC2 run without -BrokerHost; AC3 default run, read the toggles back over telnet.
<!-- SECTION:PLAN:END -->
