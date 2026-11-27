---
id: TASK-2911
title: >-
  After a panic boot .61 stayed associated with DHCP but dead to ping, HTTP and
  telnet for 8 min; the dead-link supervisor never arms on a boot whose gateway
  never answered
status: To Do
assignee: []
created_date: '2026-11-19T09:43:00Z'
updated_date: '2026-11-27T20:04:00Z'
labels:
  - wifi
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1053000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-19 .61 (beta.388+7a99f02): after the 10:24:05 panic the next boot (no heap hold, fresh heap) was associated and had a DHCP lease, but ping 100 % loss, HTTP 000, telnet timeouts, COM4 0 bytes, for ~8 min, no reboot; a USB reset at 10:32:47 recovered it (09, timeline in OTGW-field-data/bench-otgw32/20261119-task2026-388/). Cause not established (no log from that period). Code fact: the WiFi dead-link check only arms after a first answering gateway ping since boot (networkStuff.ino:751 sets wifiArmedGateway; :773 and :823 keep the check off while it is 0), so a boot whose network never works is never recovered by the supervisor. Coordinator decision: high, not a 2.0.0 blocker until reproduced.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A boot log capture (serial or telnet from boot) of a reproduction, or a documented failed attempt
- [ ] #2 Root cause stated with evidence, or the hang ruled out as a one-off
- [ ] #3 Decision recorded on an unarmed-supervisor guard (N min associated, no answering gateway and no traffic -> reconnect/restart)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Clue (coordinator, 2026-11-19), with caveats: the 10:24:28 telnet banner, already after the panic reboot ('Up 0(d)-00:00', so < 1 min after boot, without the heap hold), showed free 10304 / maxBlk 3828. The normal reference after the USB reset is freeheap 47172 / maxfreeblock 31732 (09 device/info). So the heap was strikingly low and fragmented right after that boot, which fits the 8-min hang. Caveats: one banner; < 1 min after boot it may be mid-init (WiFi/MQTT setup), a transient state, not a steady one. Next step: a boot log capture on .61 with heap values per setup step.

CORRECTION 2026-11-19 (09, checked by me in OTGW-field-data/bench-otgw32/20261119-task2026-388/telnet.raw): the clue above is WRONG. The banners with free 10680 / 10304, minFree 1392, maxBlk 3828 (telnet.raw lines 84 and 109) say 'Boot : Software reset': they are the 388 boot DURING the heap hold (10:04, 10:09), before the panic. The post-panic boot banner (line 128, 'Boot : Exception/panic') breaks off after 'Net : WiFi' (line 131): there is NO heap value from the boot that hung. My earlier reading tied those values to the 10:24:28 banner; that was a mix-up of lines. Known about the hang boot: associated with a DHCP lease, ping/HTTP/telnet dead ~8 min, no reboot_log line, recovered only by the USB reset at 10:32:47. Reference: 09 ran 5 normal USB-reset boots of .61 on 389 (OTGW-field-data/bench-otgw32/20261119-task2911/): all clean, first ping 5.6-6.0 s, HTTP 200 13.1-13.6 s, telnet 16.1-16.7 s, MQTT 16.3-18.1 s, banner free ~75.2 K / maxBlk 31.7-32.8 K / minFree 60-65 K. So the hang after a panic (n=1) is not reproduced on a normal boot (n=5); there is no heap evidence for or against a heap cause.

2026-11-27 06:17 (bench-tester, relayed): the OTGW32 unit (.61) was NOT dead. It had a new network address since 2026-11-25 12:07 (router lease), and the 'offline since 26 Nov 07:50' came from a GET on the old address. The unit was alive: ping 0 % loss, health UP, MQTT connected, last reset Power on (switched on 26 Nov about 20:59), crashlog available false, last reboot log line is the 22 Nov OTA, 0 WiFi outages. The TASK-2911 symptom (dead after a panic boot, DHCP works, ping does not) was NOT observed here; it may be a separate real event of 22 Nov. Rule for notes: resolve a unit by MAC or hostname, never by a fixed address. Evidence: OTGW-field-data/bench-61/2026-11-27_rel/before/ (device/info, health, settings, settings.ini sha256 33a1f6bf...d32). Task stays open (coordinator).

otgw-architect verification 2026-11-27 21:xx (D-class triage): kept, proposed for closing as not a firmware fault. AC1/AC2: the 2026-11-27 06:17 note (bench-tester, relayed) shows the unit alive on a new DHCP lease; the 'dead' reading came from the old address, so the hang is ruled out. OPEN: AC3 needs a recorded decision on the unarmed-supervisor guard (proposal: no guard, nothing to guard against); coordinator or Robert decides.
<!-- SECTION:NOTES:END -->
