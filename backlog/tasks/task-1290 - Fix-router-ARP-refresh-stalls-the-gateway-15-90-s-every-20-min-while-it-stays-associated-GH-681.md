---
id: TASK-1290
title: >-
  Fix: router ARP refresh stalls the gateway 15-90 s every 20 min while it stays
  associated (GH #681)
status: To Do
assignee: []
created_date: '2026-10-22T04:23:00Z'
updated_date: '2026-10-22T04:24:00Z'
labels:
  - bug
  - wifi
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/681
priority: high
ordinal: 253000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
theaprilhare (GH #681, 2026-10-21, 1.7.8-beta.4, 25 h capture): besides the AP-side WiFi episodes, 19 stalls of 15-90 s where the AP logs nothing and probe, telnet and MQTT all stop. They fall on a 20.3-20.8 min grid that restarts after every rejoin. The router is OPNsense with ARP max_age 1200 s. Measured by the reporter: the OTGW ARP entry expired at 00:07:04; a 1 Hz laptop ping lost 00:06:59-00:07:21 and 14 replies then arrived at once with RTT counting down from 14.4 s, i.e. the router queued packets waiting for an ARP reply from the ESP. A static ARP entry on the router removed all 20-min stalls (07:02-13:55, 1 Hz ping). In 5 stalls the beta.2 dead-link check logged 'gateway answers again', so the ESP could not reach its router either. Reporter asks why the ESP8266 answers ARP late while associated with sleep mode 0, and suggests a periodic gratuitous ARP (precedent: Tasmota SetOption41). Possibly the same mechanism as DenSinH's 'still associated and authorized' hangs on #681. Also seen: 7 'Serial Overrun' events in 10 s right after the 01:58 reconnect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause of the late ARP reply is shown or bounded from a capture on the bench (router ARP expiry forced on the MikroTik, ESP side logged), old build vs fix
- [ ] #2 With the fix, a router ARP entry expiry no longer stalls traffic to the gateway for more than 2 s on the bench
- [ ] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
