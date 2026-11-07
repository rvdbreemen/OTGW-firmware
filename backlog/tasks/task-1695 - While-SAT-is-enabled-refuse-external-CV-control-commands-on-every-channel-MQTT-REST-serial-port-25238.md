---
id: TASK-1695
title: >-
  While SAT is enabled, refuse external CV control commands on every channel
  (MQTT, REST, serial, port 25238)
status: To Do
assignee: []
created_date: '2026-11-07T15:08:00Z'
labels:
  - sat
  - otdirect
  - mqtt
  - rest
  - bridge
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 810000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 (session): while SAT is enabled no other channel may send CS or other control commands; with SAT off, MQTT, REST and serial (HA integrations, OTmonitor) may control the boiler, also in OT-Direct master mode. Decisions (AskUserQuestion): block the CV commands CS=, CH=, MM=, C2=, H2=, TT=, TC= (DHW HW=/SW= and SH= stay free); port 25238 on the Classic: read the inbound text commands per line and drop the blocked ones while SAT is enabled, all other bytes unchanged (a binary PIC upgrade only with SAT off). Today only CS= is refused (OTGW-Core.ino:3887 queue path, OTDirect.ino:2576 handler; ADR-179); port 25238 bytes bypass the queue (ADR-177). The 25238 inbound filter needs a new ADR that complements ADR-177 (binding, adr-judge gated) before it is implemented. SAT's own commands pass via satOwn=true (TASK-1575).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With SAT enabled, CS=/CH=/MM=/C2=/H2=/TT=/TC= from MQTT, REST and the OT-Direct command path are refused and logged; SAT's own commands still pass (host test red on old, green on fix)
- [ ] #2 With SAT disabled all of them are accepted again, also in OT-Direct master mode
- [ ] #3 HW=, SW=, SH= are never blocked by this rule
- [ ] #4 Port 25238 on the Classic: blocked text commands dropped while SAT is enabled, everything else byte-identical (host test on the real bridge code); ADR accepted by Robert first
- [ ] #5 UI/MQTT feedback: a refused command is visible (log line, and a status field or MQTT reply)
<!-- AC:END -->
