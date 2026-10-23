---
id: TASK-1328
title: >-
  Investigate the 732-byte heap watermark on a tester's OT-Direct unit
  (alpha.474)
status: To Do
assignee: []
created_date: '2026-10-23T19:01:00Z'
labels:
  - heap
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 451000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D's capture 2026-10-23 21:30 (#alpha-testing msg 1563262919527895071, transcript-20261023-213057-2.0.0-alpha.4749ec1b87-...-OTGW-otgw-1020BA16C6BC.txt): telnet banner 'Heap : free 65996 frag 52% minFree 732 maxBlk 31732', uptime 12 min, 'low/warn/crit 1/0/0', 'maxLoopGap 1967 ms'. minFree is the native ESP32 allocator watermark: at some point in the first 12 minutes free heap dropped to 732 bytes, close to an allocation failure. reboot_log shows software resets 20:17/20:18 and a power-on 20:32. Find what allocates that much at boot or under load on this setup (BLE? MQTT discovery? web UI page load with graph history?) and whether the earlier resets were OOM-related. Ask for the stats topics (otgw-firmware/stats/heap) over a longer run if needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The cause of the low watermark is named with evidence (capture, telnet heap trace or bench reproduction), or recorded as not reproducible with what was tried
- [ ] #2 If a firmware fix follows: watermark before/after on the same scenario
<!-- AC:END -->
