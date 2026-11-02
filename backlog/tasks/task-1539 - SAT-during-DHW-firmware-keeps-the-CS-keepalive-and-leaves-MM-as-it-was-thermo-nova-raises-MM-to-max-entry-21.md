---
id: TASK-1539
title: >-
  SAT during DHW: firmware keeps the CS keepalive and leaves MM as it was;
  thermo-nova raises MM to max (entry 21)
status: To Do
assignee: []
created_date: '2026-11-01T07:53:00Z'
updated_date: '2026-11-02T16:08:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 695000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1412.17 second read (docs/sat/sat-commissioning-profiles-2026-11-01.second-read.md), verified 2026-11-01. (1) satPicCSKeepalive() (SATcontrol.ino:597-606, called every pass at :6373, before the DHW early return at :6421) has no DHW gate: on PIC boards the last CS is re-sent every 30 s during a hot-water draw; SH=, MI=500 and SW= also run outside the control tick (:6346-6349, :6391). (2) During a draw the firmware sends no MM, so a PWM MM=0 stays for the whole draw; thermo-nova keeps sending CS/MM/CH and raises MM to the maximum while DHW is active (heating_control.py:136-137, :196-240, :517-520), which entry 21 ('DHW active: MM=max_modulation') also records. Already listed as a difference in sat-comparison.md:102, :478, :950. SAT control behaviour: put to Sergeant D before any change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answer recorded as a numbered entry (keep firmware behaviour, or follow thermo-nova/entry 21 for MM during DHW; keepalive during DHW yes/no)
- [ ] #2 If a change is decided: host test on the real code red on HEAD and green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no. Waits on Sergeant D Q-1539a.

2026-11-02 Q-1539a answered B (entry 220): MM to the maximum during a draw. The keepalive sub-question is unanswered; the keepalive stays until he answers. Q72 A (entry 221): no general resend during a draw.

2026-11-02 entry 232: Sergeant D answered the keepalive sub-question A (msg 1566838288340226199): the 30 s CS keepalive keeps running during a draw. Only MM to max during the draw (entry 220) remains to implement.
<!-- SECTION:NOTES:END -->
