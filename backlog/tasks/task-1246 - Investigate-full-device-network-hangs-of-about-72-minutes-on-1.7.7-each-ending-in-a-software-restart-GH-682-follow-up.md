---
id: TASK-1246
title: >-
  Investigate full-device network hangs of about 72 minutes on 1.7.7, each
  ending in a software restart (GH-682 follow-up)
status: To Do
assignee: []
created_date: '2026-10-18 08:09'
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
- [ ] #1 Root cause of the ~72 min silent period and the restart that ends it is identified from code plus the reporter's capture, or the missing data is requested on GH #682 with a concrete capture recipe
- [ ] #2 If firmware-caused: a reproduction on the 1.x bench (ESP8266 .88.68) shows the hang on the old build and not on the fix
- [ ] #3 Reporter replies on GH #682 answer the three questions in the 16 Oct comment
<!-- AC:END -->
