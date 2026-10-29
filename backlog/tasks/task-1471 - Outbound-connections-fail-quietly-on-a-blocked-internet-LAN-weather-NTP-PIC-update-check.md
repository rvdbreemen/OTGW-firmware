---
id: TASK-1471
title: >-
  Outbound connections fail quietly on a blocked-internet LAN (weather, NTP, PIC
  update check)
status: To Do
assignee: []
created_date: '2026-10-29T10:49:00Z'
labels:
  - needs-maintainer-ok
  - sat
  - network
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 631000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester martreides (#alpha-testing msg 1565312598595993792, 2026-10-29) runs the OTGW in a restricted IoT VLAN: DNS and NTP are redirected to his own servers, nearly all other outbound traffic is blocked. Requirement: any outbound connection must fail without larger effects (no stall of loop(), PIC timing or SAT, no reboot, no log flood). Known outbound paths on dev: SAT weather fetch to api.open-meteo.com (SATweather.ino:63), NTP via configTime (networkStuff.ino:681), PIC firmware update check/download (OTGW-Core.ino:6370-6424, PIC_HTTP_TIMEOUT_MS), webhook. Not verified yet: behaviour with DNS that resolves but TCP is dropped (blackholed SYN) versus rejected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Inventory of every outbound connection on dev with file:line, timeout and the task/context it runs in
- [ ] #2 Bench test with outbound internet blocked (router firewall drop rule, MikroTik): no loop gap above the normal maximum, no reboot, SAT keeps running on its fallbacks; evidence = capture transcript
- [ ] #3 Each failure path logs at most once per retry interval and backs off; any path that fails this gets a fix or its own task
<!-- AC:END -->
