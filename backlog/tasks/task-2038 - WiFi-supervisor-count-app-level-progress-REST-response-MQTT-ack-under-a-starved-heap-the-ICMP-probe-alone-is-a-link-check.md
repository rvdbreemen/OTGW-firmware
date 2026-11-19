---
id: TASK-2038
title: >-
  WiFi supervisor: count app-level progress (REST response / MQTT ack) under a
  starved heap; the ICMP probe alone is a link check
status: To Do
assignee: []
created_date: '2026-11-19T00:54:00Z'
labels:
  - wifi
  - watchdog
  - heap
milestone: 2.1.0
dependencies: []
priority: medium
ordinal: 1045000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2026 (option B, coordinator 2026-11-19). The dead-link supervisor (networkStuff.ino) probes the gateway with an ICMP echo through an lwIP raw pcb (platform_esp32.h:676-705): it proves WiFi association, the driver, lwIP IP/ICMP and the AP, not a TCP round trip through async_tcp, the web server or MQTT. Run 3 of TASK-2026 (.63, 381+b06eeff, 2026-11-19, heap held starved 30 min): REST and telnet unreachable the whole time, yet probes were answered in between (the streak was cut by answered probes and by AP re-associations, wifi_outages 7). ADR-258 + ADR-261 (option C) recover the 'lwIP starved' case; a unit whose network stack answers while the app is starved ('stack alive, app starved') is not covered. Idea: while the heap is starved, count a decision point as dead when no app-level progress happened within the window (no completed REST response, no MQTT publish acknowledgement), ignoring an ICMP answer. Risk: a false restart of a quiet but healthy unit (no REST clients, MQTT off), so the starved-heap condition must be a hard gate and a unit with MQTT off and no REST traffic needs a defined answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A reproduction on the host state machine where ICMP answers but no app progress happens under a starved heap: no action on the current code (red)
- [ ] #2 A decision recorded (ADR amending ADR-258/ADR-261) on what counts as app-level progress and how a quiet healthy unit is protected against a false restart
- [ ] #3 Fix with the same reproduction green, and a scenario with a quiet healthy unit (MQTT off, no REST) that never restarts
<!-- AC:END -->
