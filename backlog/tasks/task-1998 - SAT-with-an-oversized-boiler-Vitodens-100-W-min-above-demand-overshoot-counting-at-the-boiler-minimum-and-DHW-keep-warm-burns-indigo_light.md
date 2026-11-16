---
id: TASK-1998
title: >-
  SAT with an oversized boiler (Vitodens 100-W, min above demand): overshoot
  counting at the boiler minimum and DHW keep-warm burns (indigo_light)
status: To Do
assignee: []
created_date: '2026-11-16T10:42:00Z'
updated_date: '2026-11-16T10:48:00Z'
labels:
  - needs-maintainer-ok
  - sat
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1022000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by indigo_light (Discord 301077214374658048), #beta-testing https://discord.com/channels/812969634638725140/914498730001072149/1571835129871990805 and the follow-up 1571835155864358953 (2026-11-16 10:31 UTC). Details in the notes. Waiting for capture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Long-run capture on beta.339 or newer and the graph CSVs from the reporter received and analysed
- [ ] #2 Q2 and Q3 put to Sergeant D one per message with the release scope line, answers recorded as entries
- [ ] #3 Follow-up per his answers: a change with a host test red on the old code and green on the fix, or a recorded works-as-designed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Setup (indigo_light): OTGW Classic + LOLIN S3 Mini, PIC 6.8, DS18B20 outside sensor on the PIC; Viessmann Vitodens 100-W WB1C 26 kW combi (min about 6.5 kW, modulation 1:4); radiators, 1961 house; heat demand 2.5-4 kW at 2-10 C outside, below the boiler minimum; at 0 % modulation the flow settles about 42 C. Firmware SAT beta.250 (15 Nov 12:40-16:40): continuous, PWM vetoed (pwm_veto_reason 2 = the boiler reports MsgID 3 HB bit 4 = 1, which blocks PWM per entry 213, SATcycles.ino:1302-1336), about 2 cycles/h, room 20.5-20.8 at target 20.5, 4h_overshoot_fraction 33 %. HA SAT (12 Nov) in PWM: about 3 cycles/h, room 20.4-20.7. Since 16 Nov on beta.339.

Open design questions for Sergeant D (one per message, each with the release scope line, after the current queue): Q2: should flow overshoot count as overshoot when the boiler cannot modulate below about 42 C (a third of his cycles look like a problem while the room is fine)? Q3: the Vitodens fires a short DHW burn (dhw_on, flow 60-78 C) after almost every CH burn without a tap (probably keep-warm): does SAT account for it, or does it disturb the cycle statistics and the self-tune? Questions message: https://discord.com/channels/812969634638725140/914498730001072149/1571835155864358953. Milestone 2.1.0 per entry 386 (anything new goes to the next release).
<!-- SECTION:NOTES:END -->
