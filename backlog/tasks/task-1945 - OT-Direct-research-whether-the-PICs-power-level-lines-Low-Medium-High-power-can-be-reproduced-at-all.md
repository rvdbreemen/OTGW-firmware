---
id: TASK-1945
title: >-
  OT-Direct: research whether the PIC's power-level lines (Low/Medium/High
  power) can be reproduced at all
status: To Do
assignee: []
created_date: '2026-11-14T18:26:00Z'
labels:
  - otdirect
  - parity
  - research
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1850 (coordinator, 2026-11-14). The PIC 6.8 prints 'Low power', 'Medium power' and 'High power' when the thermostat-side OpenTherm line level changes (Smart Power; gateway.asm 6.8 G:1532-1551; the firmware event table knows the tokens, OTGW-Core.ino kPICStatusTokens). OT-Direct prints none of them. Research first, no code: can OT-Direct detect the thermostat-side line levels at all? Cite the hardware (the OTGW32 OpenTherm interface: which signals the ESP32 sees, whether a voltage/current level is measurable) and OTDirect.ino (what the receive path exposes). Keep it consistent with TASK-1848 (95), where OT-Direct answers PR=P with a fixed 'Low power' (no Smart Power): if levels cannot be detected, the outcome is 'no lines, PR=P stays Low power' and this task closes with that evidence.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Research note with citations (hardware interface and OTDirect.ino receive path): whether the thermostat-side line level is observable on OT-Direct
- [ ] #2 Decision recorded: either a follow-up task for the lines, or closed as not reproducible, consistent with TASK-1848's fixed 'Low power' answer to PR=P
<!-- AC:END -->
