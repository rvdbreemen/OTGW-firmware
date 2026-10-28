---
id: TASK-1291
title: >-
  Investigate: daytime packet loss on a fixed 9-minute rhythm while the gateway
  stays associated (GH #682)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-22T04:25:00Z'
updated_date: '2026-10-28T20:32:00Z'
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
- [x] #2 Every firmware, SDK and lwIP periodic timer near 9 min (including DHCP renewal) is listed with its period, and each is ruled in or out against the reporter's timestamps
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 AC#2: periodic timers on the 1.x line (1.7.8-beta.6 source + core 2.7.4 lwip2 headers), none has a ~9 min (540-546 s) period:
- Firmware (DECLARE_TIMER in src/OTGW-firmware): 1 s, 3 s, 60 s task ticks; dead-link probe 60 s (3 pings over ~6 s, the "(1/8)" events are quantised to it, which is why they land on :13 s); MQTT keepalive 60 s (MQTTstuff.ino:936), MQTT retry wait 42 s; webhook retry 30 s; PIC temperature read 180 s (only when enabled); 5 min status burst (timer5min, MQTT publish of all settings topics); 15 min persist of /ot-thermo.json + /ot-boiler.json to LittleFS (timer15min); nightly-restart check hourly; discovery drip and sensor/S0 intervals from settings.
- lwIP: ARP_MAXAGE 300 s (the ESP re-ARPs its router every 5 min), DHCP coarse timer 60 s with renewal at T1 = lease/2 (router-dependent: an 18 min lease would give exactly 9 min), SNTP 3600 s, DNS 1 s, IGMP reports only in answer to router queries (router interval, often 125 s).
- SDK/AP side, not in firmware: beacon/DTIM, WPA group-key rotation (AP setting), AP band-steering/roaming assistant.
- Not a firmware period. Ruled in/out per item needs the reporter's DHCP lease time and group-key interval; the strict daytime window (09:25-17:38, none at night) argues for something external that runs by day. AC#1 waits for the beta.5 Link/Drop data.

2026-10-28 triage: GH #682 comment 2026-10-22 20:54Z (mrfox7688, first full day on beta.5): banner shows only 2 real WiFi outages in ~16.5 h (reason 7 NOT_ASSOCED roam attic->office node, down 0 s; reason 200 BEACON_TIMEOUT, down 6 s) while ping loss stays ~20%/h on both APs -> associated-but-silent. Dead-link check never past 2/8. 45 of 52 '(1/8)' gaps exactly 9.0 min. 16 Mosquitto disconnects 11:13-17:17. Reporter asks: (1) outage count + last drop record via REST/MQTT (device/info), (2) any 9-min timer in firmware/WiFi stack. New lead: solar inverter + Pocket LAN dongle 40 cm from gateway, active hours roughly match loss window (correlation only). Unanswered on GH.
<!-- SECTION:NOTES:END -->
