---
id: TASK-1874
title: >-
  SAT: after an ESP reset the gateway sends CS (curve value) + CH=1 before SAT
  has a room temperature; the burner lit above target (gruijter beta.214 14:07,
  17:59)
status: To Do
assignee: []
created_date: '2026-11-12T20:56:00Z'
updated_date: '2026-11-12T21:09:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
ordinal: 948000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source: gruijter (Discord 733401267665109054) capture 20261112-121301-upload.zip on 2.0.0-beta.214+5d03c17 (Classic, PIC 6.8), #alpha-testing https://discord.com/channels/812969634638725140/1514720723980259460/1570536002936905839; analysis posted https://discord.com/channels/812969634638725140/1514720723980259460/1570542193934860348. Evidence (transcript lines, checked first-hand by comms): before the Task-watchdog reset at 14:06:20 SAT's last commands were CS=10.0 MM=100 CH=0 (14:06:17, line 73204; SATTICK cs=10.0 ch=0 line 73207). After the reboot (banner line 73267) at 14:07:05.9 the thermostat sends T10010A00 (TSet 10.0) and status [-D---W--], while the gateway sends R90011D34 (TSet 29.20, line 73339) and R00000300 (CH enable, [CD---W--], line 73370). Flame on 14:07:12 with the room at 19.32 (target 19.0), off 14:08:30 (78 s). SAT's first logged command only at 14:07:30 (CS=29.2 MM=100 CH=1, SATTICK room=nan); 'Room temperature back' 14:08:23. Same at 17:59 (R 10011F67 CS 31.4 at 17:59:09, relight 17:59:20, reboot banner line 216013) and at capture start 12:13 (CS 30.2 + CH on). Telnet was down during boot, so the log does not show who queued that first CS. Code comment to verify against this: SATcontrol.ino:4033-4047 at 5d03c17 (satRestart): initSAT sends no SAT command; valid restored state -> nothing sent, the PIC keeps SAT's last CS for about a minute (entry 129); not valid -> entry-47 hand-back once (CS=0, CH=1, MM=T). Neither branch explains CS 29.2 / 31.4 (the curve value) with CH=1 here. Related: Q-P1 (open with Sergeant D, docs/sat/open-questions.md: what SAT sends after a restart, curve + saved P+I+D as thermo-nova vs curve only vs curve + I; entry 115), TASK-1866 (integral display, different issue), the morning 01:08 reboot on beta.182 (curve-only for 22 s, CS 30.1), TASK-1865 (the watchdog resets). Blocked for the behaviour change on Sergeant D's Q-P1 answer; d3 is checking read-only who sends the first CS.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 d3 read-only trace (relayed by the coordinator, not re-checked by comms): the first CS/CH after boot comes from SAT itself, not the PIC. With a valid restored state, satRestartStep() (SATcontrol.ino:4062-4130) sets the boot takeover and makes the control tick due at once (about line 4122, step 5 of the satRestart comment). That tick runs with room=nan, so it is curve-only (entries 38/46) and sends CS = curve (29.2 / 31.4) and CH=1. The comment line 'valid: nothing is sent' (about 4040) contradicts step 5 of the same block. Evidence: the telnet banner shows 'SAT active sp 29.2C' about 2 s after boot, and the R frames match the curve. Open: what cleared the command cache before the 30 s tick (the OT-online edge at SATcontrol.ino:7072-7075 or the PIC banner at OTGW-Core.ino:6213). The behaviour (wait for the room, or keep the last CS/CH) is Sergeant D's Q-P1.
<!-- SECTION:NOTES:END -->
