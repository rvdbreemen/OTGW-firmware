---
id: TASK-1457
title: >-
  feat-2.0.0: port TASK-1442 (1.x) — report a command the PIC refuses
  (NG/BV/...) on command_dropped
status: To Do
assignee: []
created_date: '2026-10-29T07:03:00Z'
labels:
  - port-from-1.x
  - mqtt
  - pic
milestone: 2.0.0
dependencies:
  - TASK-1439
priority: low
ordinal: 617000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The PIC refuses a command with an error code instead of an echo: "ZZ: NG" for an unknown command, "PR: BV" for a register an older PIC lacks. Codes: NG SE BV OR NS NF OE (otgw.tclcode.com/firmware.html).
Problem on dev: checkCommandResponse() in OTGW-Core.ino (after f3b1351e4, ~3890-3930) matches such a reply on its two letters, so the command is removed as confirmed, not reported, and clearCommandDropped() sets command_retry_exceeded OFF.
1.x fix (otgw-1.x.x 11c169021, OTGW-Core.ino): isPicErrorReply(code) with a PROGMEM list; reportCommandRejected(cmd, code) -> reportCommandDropped("<cmd> (<code>)"); in the match loop, before the PR register check, on an error reply report/remove the first queued command with the same two letters and break; no retry; flag not cleared.
Dev differences: there is also a PM= match (atoi on the id): catch the error reply before that check too, as before the PR check. OT-Direct has no queue, so a synthesised "XX: NG" finds nothing (fine, note it). SAT's CS= goes through the queue too, so a refused SAT CS= is reported as well (wanted).
Depends on TASK-1439 (Done). Draft from the 1.x session, 2026-10-29.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 test/host/test_cmd_queue_drop.py gains the 1.x R1-R3 cases (ZZ: NG -> 'ZZ=1 (NG)', flag ON, sent once; SB: OR does not clear the flag; PR: BV -> 'PR=Q (BV)' after one send) plus PM: NG -> 'PM=3 (NG)'; red on HEAD, green on fix; existing D/C cases stay green
- [ ] #2 A normal confirmation still sets the flag OFF; PR= and PM= do not clear it (existing behaviour)
- [ ] #3 Bench on a PIC board (Classic .63) when available: ZZ=1 via MQTT gives command_dropped 'ZZ=1 (NG)'; otherwise recorded as not run
- [ ] #4 esp32-combo build and evaluate.py --quick green; MQTT.md and CHANGELOG as on 1.x
<!-- AC:END -->
