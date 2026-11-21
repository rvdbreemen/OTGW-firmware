---
id: TASK-2993
title: 'SAT: PWM duty always with the live flow, no held BT (entry 437, Sergeant D)'
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T03:28:00Z'
updated_date: '2026-11-21T07:55:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1120000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, entry 437 (#dev-sat-mqtt 2026-11-21 03:25 UTC, msg 1573539719147552939): duty = (requested setpoint - 27.2) / (flow - 27.2), always with the live flow (MsgID 25). Today BT is an EMA for 180 s after ignition and then held (SATcontrol.ino:1648, 1669-1686; as thermo-nova pwm.py:144-149, 229-239). Field case indigo_light 19 Nov 21:27-21:33: live-flow duty 101 % then ~47 %, logged duty 97-100 % with BT held at 32.12 C -> range 5, MM=T, saturation exit 21:32:30, fail-safe 21:33:30. Reverses Robert's entry-64 choice (keep 7beaaf5): Robert's OK asked in-channel (msg 1573540314436730942). Open: start-up filter kept for 30 s or dropped (asked, msg 1573540311874142270). Likely makes TASK-2992 (entry 436) moot; keep both until Sergeant D says so.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's OK (reversing entry 64) and Sergeant D's answer on the start-up filter are recorded in docs/sat/sergeant-d-corrections.md before any code change
- [ ] #2 A host test on the real PWM duty code shows the duty staying at 1.00 with a held BT while the live flow rises (current code, red) and following the live flow with the fix (green); base+1 floor kept
- [ ] #3 The start-up filter follows Sergeant D's answer (none, or N s), with a test for the window edge
- [ ] #4 Replay of indigo_light 19 Nov 21:27-21:33 gives live-flow duties of about 101 % then about 47 %, no range 5 after 21:28:30, no MM=T, no saturation exit; replay reports the remaining switches with TASK-2987 + 2989
- [ ] #5 Bench via the NAS (replay or simulation) before the push
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: Sergeant D answered B (entry 438): keep the start-up EMA for the first 30 s after ignition, then the live flow until the next ignition. Waiting only for Robert's OK on reversing entry 64.

2026-11-21 03:34 UTC: Sergeant D entry 439: this duty fix is the only SAT change for 2.0.0 from this thread; afterwards the next field runs are checked for remaining switching. Still waiting for Robert's OK on reversing entry 64.

2026-11-21 (92): Robert's OK for 2.0.0 recorded as entry 441 (DM 1573595110879395942, 'A', reverses entry 64). Q-438a answered A (entry 443): the 30 s start-up filter stays. Q-438b (live flow in the OFF phase) asked 07:07 UTC, msg 1573595715278602335; open.

2026-11-21 (92): Q-438b answered B (entry 444, msg 1573596529984544881): the live flow only in the PWM ON phase; in OFF the duty keeps the last ON value. Interpretation for you to confirm with a test: hold the last ON flow and recompute with the current request. All open Sergeant D questions on this thread are answered (queue 0).

2026-11-21 (92): Q-444a asked 07:41 UTC (msg 1573604247805890651): hold the last ON flow (A, as built) or hold the duty (B) during OFF; without an answer before 17:30 the 20:00 beta goes with A. Known limit (Q-444b, parked for 2.1.0): a hot-water draw near the end of an ON phase makes the held flow the DHW-heated primary flow (about 55-60 C) for the whole OFF phase, so a long pause; rare, not handled in 2.0.0.

2026-11-21 (92): Q-444a answered (entry 446, msg 1573605458000879747): 'during the PWM OFF we keep the last PWM ON boiler flow temperature ... we freeze the flow temperature'. Read as A = as built (frozen flow, duty recomputed with the current request). No code change needed.

2026-11-21 (92): entry 447: Sergeant D confirmed explicitly (msg 1573607198859460671): during PWM OFF the request stays active and may change the OFF duty; only the flow is frozen. That matches the build.
<!-- SECTION:NOTES:END -->
