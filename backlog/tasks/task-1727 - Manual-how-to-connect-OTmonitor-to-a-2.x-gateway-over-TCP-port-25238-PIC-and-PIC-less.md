---
id: TASK-1727
title: >-
  Manual: how to connect OTmonitor to a 2.x gateway over TCP port 25238 (PIC and
  PIC-less)
status: To Do
assignee: []
created_date: '2026-11-08T10:48:00Z'
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
- [ ] #1 EN and NL manual network chapter contain the OTmonitor-over-TCP steps and the USB note
<!-- AC:END -->
