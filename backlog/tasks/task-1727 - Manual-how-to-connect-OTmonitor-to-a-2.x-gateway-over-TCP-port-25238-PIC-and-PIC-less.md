---
id: TASK-1727
title: >-
  Manual: how to connect OTmonitor to a 2.x gateway over TCP port 25238 (PIC and
  PIC-less)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T10:48:00Z'
updated_date: '2026-11-08T13:10:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 835000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1720/1721 (Robert 2026-11-08 chose option A). docs/manuals/en/ch06-network.md:158 lists port 25238 but there is no how-to. Add: enable 'Legacy TCP port 25238', OTmonitor connection type TCP, host = gateway IP, port 25238; on PIC-less hardware this is the only OTmonitor path, USB is for flashing and diagnostics (opening the USB port resets the gateway, TASK-1720 gate); never flash the PIC over WiFi with OTmonitor. EN and NL manual.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 EN and NL manual network chapter contain the OTmonitor-over-TCP steps and the USB note
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: new section 6.7 in docs/manuals/en/ch06-network.md ('Connecting OTmonitor') and docs/manuals/nl/h06-netwerk.md ('OTmonitor verbinden'), in the gap between 6.6 and 6.8: enable Settings > OpenTherm Gateway > Legacy TCP port 25238 (default off, label from v2.js:2140 and settingsHelp.h), OTmonitor 'TCP connection' with 'Remote host' = gateway IP and 'Remote port' = 25238 (labels from otmonitor-6.6 gui.tcl:1218-1222); USB note: on hardware without a PIC the USB port is for flashing and diagnostics only, opening it from OTmonitor resets the gateway (TASK-1720 bench gate: one open/close rebooted .61, 'USB reset (host)'); never flash the PIC over WiFi with OTmonitor, use Advanced > PIC firmware; SAT note: CV control commands refused while SAT is on (TASK-1695), Classic port passes only text lines and drops binary data (ADR-217), so no OTmonitor PIC upgrade with SAT on; port 25238 has no password. No board names. tests/test_manual_sat_commands.py OK.
<!-- SECTION:FINAL_SUMMARY:END -->
