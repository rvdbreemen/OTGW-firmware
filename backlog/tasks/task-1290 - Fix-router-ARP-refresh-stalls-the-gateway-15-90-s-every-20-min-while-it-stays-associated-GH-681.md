---
id: TASK-1290
title: >-
  Fix: router ARP refresh stalls the gateway 15-90 s every 20 min while it stays
  associated (GH #681)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T04:23:00Z'
updated_date: '2026-10-22T15:03:00Z'
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
- [x] #1 Root cause of the late ARP reply is shown or bounded from a capture on the bench (router ARP expiry forced on the MikroTik, ESP side logged), old build vs fix
- [x] #2 esp8266 build and evaluate.py --quick green
- [x] #3 With the fix the gateway sends a gratuitous ARP every 60 s while it has an IP, also after a WiFi reconnect (bench sniffer), and the router-side effect is shown from primary source: a received ARP refreshes the expiry of an existing entry on the reporter's router OS
- [ ] #4 The reporter confirms on a build with the fix that the 20-minute stalls are gone (field, OPNsense)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 bench (.68, MikroTik /tool sniffer quick mac-protocol=arp on wlan2; router ARP entry removed before each run):
- Old build a1d756077, 150 s: no broadcast ARP from the ESP; ESP unicast ARP to the router at 27.4 s; router unicast ARP request at 92.359 s answered at 92.360 s (1 ms). Entry back to 'delay' afterwards (re-learned from the ESP's own traffic).
- Fix bae31e7 (task1290.patch), 150 s: broadcast gratuitous ARP at 35.537 s and 95.543 s (60.0 s apart); router unicast requests at 42.670/102.680 s answered in 2 ms and 7 ms.
- After a 25 s access-list block and rejoin (no reboot, uptime unbroken): GARP still sent (89.272 s in the next 130 s window).
- build.bat green, globals 53376 B (unchanged; the core timer lives in the core), evaluate.py --quick 0 failed.
AC#1 (bounded): the late ARP reply does not occur on the bench; the ESP answers the router's ARP request within 1-7 ms on both builds at RSSI ~-76. So the field delay is not the ESP being slow to process ARP while it hears the request. Unproven field candidates: a broadcast ARP request (what FreeBSD sends after the entry expired; the MikroTik probes unicast) is sent without 802.11 ACK/retry and can be lost on air, and FreeBSD retries once per second (arpt_rexmit) up to arp_maxtries before marking the host down for arpt_down. The gratuitous ARP avoids the expiry, so it sidesteps whichever of these it is.
AC#2 reworded (was: 'router ARP entry expiry no longer stalls traffic > 2 s on the bench'): the bench never stalls on expiry with either build, so the old wording could not separate old from fix. New AC#2 checks the mechanism; new AC#4 needs the reporter.
Primary source for AC#2: FreeBSD sys/netinet/if_ether.c in_arpinput(): for a received ARP whose sender has an existing llentry, la = lla_lookup(...); arp_check_update_lle(...) always calls arp_mark_lle_reachable(), which sets la_expire = time_uptime + V_arpt_keep and reschedules arptimer (non-static entries). The packet is then dropped at the reply stage (not our target), after the cache update. OPNsense is FreeBSD-based. Linux-based routers (MikroTik bench) only mark the entry STALE on an unsolicited ARP, so there the GARP does not prevent their own unicast probes (seen on the bench: probes continued).
<!-- SECTION:NOTES:END -->
