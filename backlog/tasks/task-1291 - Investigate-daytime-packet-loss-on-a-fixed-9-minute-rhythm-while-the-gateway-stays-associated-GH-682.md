---
id: TASK-1291
title: >-
  Investigate: daytime packet loss on a fixed 9-minute rhythm while the gateway
  stays associated (GH #682)
status: To Do
assignee: []
created_date: '2026-10-22T04:25:00Z'
labels:
  - bug
  - wifi
  - needs-info
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/682
priority: medium
ordinal: 254000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 (GH #682, 2026-10-21 20:24Z, 1.7.8-beta.2, ASUS RT-AX88U Broadcom, ch 1): after moving the gateway off the boiler top (23:23 on 20 Oct) losses fell (Mosquitto disconnects 55 -> 11, failed daytime pings 325 -> 204, none at night), but the rhythm stays: 33 of 52 gaps between dead-link '(1/8)' events are exactly 9.0 min, NAS ping episodes ~9 min 6 s apart, only 09:25-17:38. Reporter asks whether anything in the firmware or WiFi stack runs on a ~9-min timer. Quick grep 2026-10-22: no DECLARE_TIMER with 9 min/540 s in src/OTGW-firmware (not yet checked: SDK/lwIP timers, DHCP lease T1 on the reporter's router). Reporter flashed beta.5 at 2026-10-21 22:06 and will report the banner Link/Drop lines for 22 Oct. Attachments on the issue: otgw-transcript-20261021-public.txt.gz, otgw-ping-20261021.log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 beta.5 Link/Drop banner data from the reporter classifies the 9-minute losses as associated-but-silent or as real disassociations
- [ ] #2 Every firmware, SDK and lwIP periodic timer near 9 min (including DHCP renewal) is listed with its period, and each is ruled in or out against the reporter's timestamps
<!-- AC:END -->
