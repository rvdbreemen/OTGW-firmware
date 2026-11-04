---
id: TASK-1471
title: >-
  Outbound connections fail quietly on a blocked-internet LAN (weather, NTP, PIC
  update check)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T10:49:00Z'
updated_date: '2026-11-04T16:24:00Z'
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
- [x] #1 Inventory of every outbound connection on dev with file:line, timeout and the task/context it runs in
- [ ] #2 Bench test with outbound internet blocked (router firewall drop rule, MikroTik): no loop gap above the normal maximum, no reboot, SAT keeps running on its fallbacks; evidence = capture transcript
- [x] #3 Each failure path logs at most once per retry interval and backs off; any path that fails this gets a fix or its own task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related: TASK-1478 (found in gruijter's capture, TASK-1474): even with working internet every Open-Meteo fetch blocks the loop ~5 s because the stream parser waits for the 5 s byte timeout on a keep-alive socket. Same class as this task; fix them together or in order.

Robert 2026-10-29 (interactive): released, priority medium. TASK-1478 (alpha.534) already fixed the 5 s tail of a successful weather fetch; this task covers blocked (dropped/rejected) outbound traffic.

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: partly: weather backoff SATweather.ino:438-455; outbound inventory (AC1) doable, rest needs a blocked-WAN bench.

2026-11-04 Robert: MikroTik fault injection allowed: one rule per test, scoped to the bench boards' MAC/IP (.61/.63), comment-tagged, scheduler auto-removal within 30 min, removed right after; config export before and after recorded in the notes.

2026-11-04 (sonnet agent, lead verified R2 in code): inventory in docs/audits/2026-11-04-outbound-connections.md (weather, NTP, PIC HEAD/GET, MQTT DNS/connect, webhook LAN-only, mDNS, DHCP/DNS; no GitHub/OTA/Discord calls). AC3: every path backs off and logs at most once per interval except two that block the loop task: R1 weather GET (5 s+ per attempt), R2 refreshpic GET after a failed HEAD (lead: a feedWatchDog sits between HEAD and GET, so each block stays under the 30 s TWDT, but the GET is useless); both go to TASK-1604. Low: R4 MQTT hostname lookup, R5 two ungated [HEAP] lines. AC2 bench plan in the audit.

2026-11-04 AC2 red run beta.74 (.63, T1 forward drop, 16:56-17:21): five Exception/panic reboots, one every 5 min, backtrace into NetworkManager::hostByName -> dns_clear_cache -> sntp_dns_found (lwIP core-lock assert); filed TASK-1605 (high). Router: rule auto-removed, 'left: 0', export before/after saved on the router (t1471-red-before/after).
<!-- SECTION:NOTES:END -->
