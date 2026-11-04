---
id: TASK-1471
title: >-
  Outbound connections fail quietly on a blocked-internet LAN (weather, NTP, PIC
  update check)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T10:49:00Z'
updated_date: '2026-11-04T15:45:00Z'
labels:
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related: TASK-1478 (found in gruijter's capture, TASK-1474): even with working internet every Open-Meteo fetch blocks the loop ~5 s because the stream parser waits for the 5 s byte timeout on a keep-alive socket. Same class as this task; fix them together or in order.

Robert 2026-10-29 (interactive): released, priority medium. TASK-1478 (alpha.534) already fixed the 5 s tail of a successful weather fetch; this task covers blocked (dropped/rejected) outbound traffic.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: weather backoff SATweather.ino:438-455; outbound inventory (AC1) doable, rest needs a blocked-WAN bench.

2026-11-04 Robert: MikroTik fault injection allowed: one rule per test, scoped to the bench boards' MAC/IP (.61/.63), comment-tagged, scheduler auto-removal within 30 min, removed right after; config export before and after recorded in the notes.
<!-- SECTION:NOTES:END -->
