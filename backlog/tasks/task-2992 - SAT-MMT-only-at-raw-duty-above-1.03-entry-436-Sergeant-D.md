---
id: TASK-2992
title: 'SAT: MM=T only at raw duty above 1.03 (entry 436, Sergeant D)'
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T03:12:00Z'
updated_date: '2026-11-21T07:08:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1119000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, entry 436 (#dev-sat-mqtt 2026-11-21, msg 1573536221601210410): release the MM only when the duty cycle is above 1.03. Today MM=T (entries 242/243) is sent whenever the PWM duty is in range 5, which starts at dutyMax 0.90 at 4 cycles/h (SATcontrol.ino:1690-1699, satPwmAtMaxDuty :1237, mmTarget :8513). Reading (to be confirmed by Sergeant D before implementation): raw duty (request - base)/(BT - base) (SATcontrol.ino:1648) before the [0,1] cap at :1650-1651 must be > 1.03. Field case: indigo_light 19 Nov 21:27:30, duty 0.97 -> MM=T -> boiler modulation up to 55 %, flow 31.9 -> 41.5 C, saturation exit 21:32:30, fail-safe 21:33:30. Beside TASK-2987 and TASK-2989. Implementation only after Sergeant D confirms the reading.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's confirmation of the reading (raw duty before the cap > 1.03) is recorded in docs/sat/sergeant-d-corrections.md before any code change
- [ ] #2 A host test on the real PWM schedule/MM path shows MM=T at raw duty 0.97 on the current code (red) and no MM=T with the fix; MM=T still comes at raw duty above 1.03
- [ ] #3 The raw duty is available where MM=T is decided (keep the [0,1] cap for the ON/OFF times); documented in the code comment with entry 436
- [ ] #4 Replay of indigo_light 19 Nov: no MM=T at 21:27:30, and the replay reports the remaining switches with TASK-2987 + TASK-2989 + this task
- [ ] #5 Bench via the NAS (replay or simulation) before the push
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-dev read-only check (relayed by 92): (1) the raw duty is not available today; SATcontrol.ino:1648 clamps to [0,1] right away (:1649-1650) and fPwmDutyCycle and the range check (:1698) see only the clamped value, so > 1.03 needs a raw variable before the clamp (small change); MQTT/UI duty stays capped at 1.00. (2) Replay with back-computed BT (indigo BT 32.12 after the 180 s window): raw duty 0.97 at 21:27:30 and 21:28:00, 1.09 at 21:28:30, then 1.18/1.29/1.35, so MM=T comes 60 s later (21:28:30), not never: with BT held at the ignition flow the raw duty passes 1.03 as soon as the request rises ~0.2 C (threshold pid > 32.27). gruijter 20/11: the three MM=T moments have raw <= 1.02, so no MM=T at all (rougher, inside the EMA window). (3) Entry 436 changes only MM: range 5 at the clamped 0.90 still gives off time 0, so the indigo 21:32:30 saturation exit still fires on the recorded trajectory. Points for confirming the reading with Sergeant D: a) duty = unclamped value? b) MM only, or also range 5 'off time 0' at 0.90? c) with BT stuck, 1.03 delays but does not stop the release.

2026-11-21 03:34 UTC: CANCELLED by Sergeant D (entry 439, msg 1573542104532000799): build only the duty fix (entries 437/438, TASK-2993); the 1.03 rule (entry 436) is dropped. Do not implement. Coordinator to close/archive.

2026-11-21 03:52 UTC: RESTORED. Sergeant D chose to keep the 1.03 rule next to the duty fix (entry 440, msg 1573546640378171483 'yes A'), after the replay showed that with only the live-flow duty MM=T would still flip on at 21:25:00 / off 21:26:00 / on 21:26:30 in indigo's warm-up (raw duty 0.93-1.015). Build together with TASK-2993 in the same lane (the raw duty variable is shared). The 'cancelled' note above is void.

2026-11-21 (92): entry 441 (Robert's OK for the duty fix in 2.0.0) and entry 442 (guards 2987/2989 to 2.1.0) recorded; this 1.03 rule (entry 440) stays in the 2.0.0 lane with TASK-2993.
<!-- SECTION:NOTES:END -->
