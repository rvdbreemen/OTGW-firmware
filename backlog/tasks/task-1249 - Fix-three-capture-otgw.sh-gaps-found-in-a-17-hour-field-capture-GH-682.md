---
id: TASK-1249
title: 'Fix three capture-otgw.sh gaps found in a 17-hour field capture (GH #682)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-18 12:49'
updated_date: '2026-10-18 21:03'
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
- [x] #1 A capture whose telnet stream goes silent while the device is unreachable reconnects telnet once the device answers again, and the summary records the idle reconnect
- [x] #2 A non-interactive run without -BrokerHost on a host without mosquitto_sub starts, captures telnet/probe/crashlog, and states that MQTT capture is skipped
- [x] #3 After a default run the debug toggles the script switched on are switched back off, and the summary says so
- [x] #4 The same three behaviours hold in capture-mqtt-debug.bat, or the gap there is recorded
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-18 implemented in dc4e0341c and tested on the 1.x bench unit .88.68 (1.7.8-beta.3+9354921). Fault: MikroTik bridge filter on the unit's MAC (DHCP and ARP to the router allowed), unit stays associated with its IP. Logs: %LOCALAPPDATA%/OTGW-capture/bench-20261018/{old-capture,new-capture-sh,new-capture-bat}, rig-1250/events.log.
OLD (HEAD scripts before dc4e0341c): during a 25 min outage plus 4 min after the device answered HTTP again (poll.log from 22:08:13) old .sh telnet.log was last written at 21:43:01, one connect in the whole run; it switched REST API, MQTT, MQTTGate, Sensors on and the banner after the run still showed them [1]; without -BrokerHost the old .sh exits 2 'BrokerHost is required.' and the old .bat throws the same.
NEW .sh (non-interactive, no -BrokerHost, empty config dir, 4.5 min outage below the firmware's 8-probe threshold): 'MQTT capture skipped: no BrokerHost given'; 'Telnet silent for 120s and the device does not answer HTTP: waiting' at 22:47:15; 'Telnet idle reconnect #1: no telnet bytes for 272s while the device answers HTTP (200)', reconnected 22:49:55 (15 s after the fault was lifted); after that reconnect the banner showed every toggle already on; teardown 'Debug toggles restored to their state at capture start: REST API off, MQTTGate off, Sensors off'; banner after the run REST 0 / MQTTGate 0 / Sensors 0 (start state).
NEW .bat, same scenario: same lines (idle reconnect after 281 s, reconnected 22:58:20), same restore, banner confirms.
Extra gap found and fixed: TOGGLED_KEYS / ToggledKeys was overwritten on every connect, so after a reconnect without a reboot nothing was restored (also for -QuietDebugToggles); restore now compares states with the first connect.
Static: shellcheck 0.11.0 -s bash exit 0 (old and new), bash -n ok, .bat PowerShell payload 0 parse errors. NOT verified: macOS bash 3.2 (only Git Bash bash 5 on Windows).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
capture-otgw.sh and capture-mqtt-debug.bat (dc4e0341c): telnet idle watchdog (-TelnetIdleSeconds, default 120 s, waits while the device does not answer HTTP, reconnects once it does, counted in the summary); -BrokerHost optional and no broker prompt in a non-interactive run; debug toggles restored to their state at the first connect for both policies, also after a reconnect without a reboot. Evidence per AC on the 1.x bench unit with a MikroTik MAC filter outage: AC1 new .sh reconnected 15 s after the device answered again (old .sh stayed silent for the rest of the run); AC2 new scripts start without a broker and say MQTT capture is skipped (old ones exit 'BrokerHost is required.'); AC3 REST API, MQTTGate and Sensors back to off, banner-confirmed (old run left them on); AC4 the .bat shows the same three behaviours. shellcheck/bash -n clean. Not run on macOS bash 3.2.
<!-- SECTION:FINAL_SUMMARY:END -->
