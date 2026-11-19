---
id: TASK-2911
title: >-
  After a panic boot .61 stayed associated with DHCP but dead to ping, HTTP and
  telnet for 8 min; the dead-link supervisor never arms on a boot whose gateway
  never answered
status: To Do
assignee: []
created_date: '2026-11-19T09:43:00Z'
updated_date: '2026-11-19T09:52:00Z'
labels:
  - wifi
milestone: 2.1.0
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
<!-- SECTION:NOTES:END -->
