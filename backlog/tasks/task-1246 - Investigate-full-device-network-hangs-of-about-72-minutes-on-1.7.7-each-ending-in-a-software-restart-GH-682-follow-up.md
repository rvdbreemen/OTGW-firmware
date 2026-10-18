---
id: TASK-1246
title: >-
  Investigate full-device network hangs of about 72 minutes on 1.7.7, each
  ending in a software restart (GH-682 follow-up)
status: Done
assignee: []
created_date: '2026-10-18 08:09'
updated_date: '2026-10-18 12:51'
labels:
  - bug
  - needs-info
  - mqtt
  - wifi
dependencies: []
priority: high
ordinal: 242000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by mrfox7688 on GH #682 (comment 2026-10-16, 24h capture on a Synology NAS, firmware 1.7.7+4a15d9e, PIC 6.8, ESP8266 NodoShop OTGW).
Observed 16 Oct (CEST): last telnet line 13:13:24 and last MQTT publish 13:13:23 with a healthy heap (18-20 KB free, HEALTHY, all drop counters 0). HTTP stops at 13:13:44; Mosquitto 'exceeded timeout' at 13:14:54 (90 s = 1.5 x keepalive). Device returns about 14:25-14:26 (HTTP + MQTT together) and reboot_log.txt gains a 'Software/System restart (4)' entry. Second identical cycle 15:17 -> 16:30 (73 min), again ending in a restart (4) entry. Between: 13 self-closed MQTT sessions (14:34-15:14, 'disconnected: Success'), later 'session taken over' x3 and 'exceeded timeout' x3 (16:40-16:57), offline 16:57-17:37.
Triage facts from the transcript (otgw-transcript-20261016-public.txt.gz, 66 MB): during the hangs every curl fails fast with 'Failed to connect ... after ~1460 ms' (consistent with no ARP reply, i.e. the station is off the network) rather than a TCP timeout; the telnet log ends mid-stream with no error, no exception and no WDT line; reboot_log dates are 2106 (clock not set when written). The 1.x WiFi state machine (networkStuff.ino:240-301) restarts after 10 x 30 s = 5 min of failed reconnects, which does not explain 72 min; cause not yet known (inferred, not verified).
Reporter questions to answer: is there a ~72-min timer/watchdog; do the all-on debug toggles influence this; what does 'session taken over' followed by 'exceeded timeout' point to. Reporter offers: a run with -QuietDebugToggles, 30 s ping log, router log, signal strength.
Capture-script findings (separate tasks if confirmed): telnet stays half-open for hours with no idle reconnect; -BrokerHost required non-interactively even without mosquitto_sub; unclear whether debug toggles are restored at the end.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause of the ~72 min silent period and the restart that ends it is identified from code plus the reporter's capture, or the missing data is requested on GH #682 with a concrete capture recipe
- [x] #2 If firmware-caused: a reproduction on the 1.x bench (ESP8266 .88.68) shows the hang on the old build and not on the fix
- [x] #3 Reporter replies on GH #682 answer the three questions in the 16 Oct comment
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-18 first analysis (from transcript + 1.x code, not yet proven):
- Loop most likely kept running: the 0x26 external watchdog is fed from loop on a 100 ms millis timer (OTGW-Core.ino:1011-1016); a blocked loop would have logged 'External System (6)', but both recoveries log 'Software/System restart (4)' = ESP.restart().
- Network dead, not the app: telnet ends mid-stream with no error, HTTP probes fail fast (curl (7) after ~1460 ms, not a timeout), MQTT 'exceeded timeout' 90 s after the last packet.
- The WiFi state machine (networkStuff.ino:240-301) restarts 10 x 30 s after WiFi.status() != WL_CONNECTED, so for most of each hang the SDK must have reported the station as connected (inferred).
- Duration matches 2^32 us = 4294.97 s = 71.58 min: last packet 13:13:24 + 71.58 min = 14:24:59 (MQTT back 14:25:54); 15:17:02 + 71.58 min = 16:28:37 (MQTT back 16:30:36). Two events only. No micros()-based wait exists in 1.x app code or vendored libs (only the unused OpenTherm lib); core micros() = system_get_time(). If causal, it lives in the closed SDK/lwIP (speculation).
- Which ESP.restart() fired is unknown: reboot_log stores only the rst reason, not the doRestart() reason text (helperStuff.ino:398). Candidate without operator action: doRestart('WiFi: too many reconnect failures').
Next: persist the doRestart reason in reboot_log; ask the reporter for an ICMP ping log, the router's station/association log and RSSI around a hang.

2026-10-18: v1.7.8-beta.1 published with the restart reason in reboot_log.txt (TASK-1247). Reply on GH #682 (issuecomment-6247770652) shares the analysis (72 min = 2^32 us, loop alive, network dead), answers the three questions and asks for: reboot_log.txt after the next hang on beta.1, a 30 s ping log, router association log for the gateway MAC, RSSI and channel. Waiting on reporter.

2026-10-18 micros() review of the WiFi reconnect path (1.x, core 2.7.4):
- No micros() in the path. loopWifi (networkStuff.ino:239-302) uses safeTimers (millis, rollover-safe signed compare, safeTimers.h:186-235). lwIP sys_now = millis (tools/sdk/lwip2/include/arch/cc.h:87). WiFiClient timeouts use millis; PolledTimeout fast variants use CPU cycles with range compensation. core millis() is wrap-safe as long as micros_overflow_tick (os_timer, 60 s) runs at least once per 71.58 min (core_esp8266_wiring.cpp:61-67,173-196,216). Only closed SDK code (system_get_time, 32-bit us) remains as a place where a 2^32 us wrap could matter: unverifiable.
- Ways loopWifi stays idle without micros: (1) recovery keys only on WiFi.status()!=WL_CONNECTED; WL_CONNECTED means STATION_GOT_IP, so an associated station with dead traffic is never acted on, and failed MQTT reconnects do not escalate; (2) isFlashing() stuck true skips loopWifi (OTGW-firmware.ino:460); (3) the safeTimers spiral guard (>10 intervals late) re-arms instead of firing, which only delays.
- Timing conflict: the WiFi supervisor restarts >= 5 min after the status flips (10 x 30 s). The restart lands about 71.6 min after the last packet, so the status would have to flip at about 66.6 min in both hangs. If beta.1's reboot_log shows no reason text, the restart did not come through doRestart().

AC2 deviation (docs/guides/backlog-cli.md protocol): the cause was not shown to be firmware. Leading explanation from the capture plus code: the station stayed associated with an IP (WL_CONNECTED = STATION_GOT_IP) while no traffic passed, and loopWifi only acts on a drop, so the firmware never stepped in. The 2^32 us duration match (2 events) stays an unproven hypothesis about the SDK. No old-vs-fix bench reproduction was possible (no ESP8266 on the bench); that proof now lives in TASK-1248 AC5, which carries the mitigation. AC2 is checked on that written transfer, not on a reproduction.
Capture-script remarks from the reporter: all three confirmed in scripts/capture-otgw.sh and filed as TASK-1249.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Investigation of the ~72 min full-device network hangs on 1.7.7 (GH #682, mrfox7688, 17 h NAS capture of 16 Oct 2026).

Findings (from the transcript plus 1.x code, cited in the notes):
- The whole device dropped off the network while the firmware kept running: telnet ended mid-stream with a healthy heap, HTTP probes failed fast, Mosquitto timed out 90 s after the last packet, and both recoveries logged Software/System restart (4), not the external watchdog (6) a hung loop would cause.
- loopWifi only acts on WiFi.status() != WL_CONNECTED, which means 'associated with an IP'; a station that stays associated while no traffic passes is never acted on. This is the most likely reason nothing recovered for an hour.
- Both silences last 2^32 us (71.6 min) to within about a minute of the restart; no micros()-based wait exists in the 1.x app, vendored libraries, core or lwIP glue (sys_now = millis), so if causal it lies in the closed SDK. Two events only: hypothesis, not proven.
- The restart that ended each hang could not be identified, because reboot_log.txt stored only the reset reason.

Outcome per AC:
- AC1: root cause not proven; missing data requested on GH #682 (issuecomment-6247770652): reboot_log.txt after the next hang, a 30 s ping log, the router's association log for the gateway MAC, RSSI and channel, and a day with -QuietDebugToggles.
- AC2: deviation, see notes: cause not shown to be firmware; the old-vs-fix proof moved to TASK-1248 AC5 (hardware).
- AC3: the same comment answers the reporter's three questions (no 72 min timer; debug toggles unlikely, run with -QuietDebugToggles to rule out; 'session taken over' then 'exceeded timeout' = a reconnect while the broker still held a dead session, then that link died too).

Follow-ups shipped or filed: TASK-1247 (restart reason in reboot_log.txt, in v1.7.8-beta.1; portal-chain fix in 19a9d444c), TASK-1248 (connected-but-dead detection and recovery, a8c79fbf6 and 19a9d444c, not yet on hardware), TASK-1249 (three capture-otgw.sh gaps the reporter found).
<!-- SECTION:FINAL_SUMMARY:END -->
