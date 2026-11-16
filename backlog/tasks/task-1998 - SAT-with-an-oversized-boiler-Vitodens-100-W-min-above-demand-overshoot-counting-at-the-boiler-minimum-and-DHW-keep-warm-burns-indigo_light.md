---
id: TASK-1998
title: >-
  SAT with an oversized boiler (Vitodens 100-W, min above demand): overshoot
  counting at the boiler minimum and DHW keep-warm burns (indigo_light)
status: To Do
assignee: []
created_date: '2026-11-16T10:42:00Z'
updated_date: '2026-11-16T16:39:00Z'
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

2026-11-16 (#beta-testing): Q3 answered by Sergeant D (entry 390: keep-warm DHW burns do not affect SAT). Q2 clarified by indigo_light (https://discord.com/channels/812969634638725140/914498730001072149/1571841155337814069): it is about the firmware SAT; 4h_overshoot_fraction 33 %, last cycle class overshoot while the room was fine; at minimum modulation the flow stays about 42 C, so every burn ends above the requested 29-40 C. Sergeant D reversed the MsgID 3 bit 4 PWM veto (entry 392): 'Don't look at bit 4 at all ... boiler should be able to enter PWM'; follow-up Q-392a. indigo_light posted the graph CSVs of both runs (https://discord.com/channels/812969634638725140/914498730001072149/1571841667839828008: otgw-graph-20261112-210857.csv HA SAT, otgw-graph-20261115-163915.csv firmware SAT beta.250; not downloaded yet) and will run a long-run capture on beta.339 as the 'before' of the veto change; he offers to test the fix.

2026-11-16 13:25-13:29 UTC #alpha-testing: indigo_light asked whether to long-run before AND after upgrading to beta.350 (1571878962584363120); Sergeant D: 'No, just run beta.350' (1571879271532724316); indigo: 'Will do' (1571879975194333316). The capture this task waits for is therefore a beta.350 long-run.

2026-11-16 indigo_light capture on beta.350 (real Vitodens, SAT on, simulation off), analysed by comms 92; archived at RvdB/OTGW-field-data/indigo_light/20261116-144953/ (zip sha256 1573a14c...bd84; analysis/ holds the summary, the SAT timeline and charts). Findings, facts from the telnet/OT log: (1) PWM now runs (see TASK-2000). (2) Mode ping-pong 16:17-16:39: enter PWM on flow >= request + 1.5 for 30 s (TASK-1888 rule), back to continuous about 2 min later on 'flame on and request above flow for 60 s' (ADR-236 exit), four times; the flame itself burned 1955 s without a break, peak flow 43.6. (3) The first PWM entry at 16:00:58 was triggered by flow 67.3 while no CH flowed: boiler water hot from DHW burns, during a band pause; the PWM ON phase that followed had no flame and counted as a start (starts/hr 2), the entry 398 case. (4) Seven DHW burns of 1-16 s (15:15-15:53, flow 61-73) were logged as SAT cycles #2-#5 and in the 4 h window (n=5, avgOn 87 s); entry 390 says such burns must not affect SAT: whether they change a control decision is not shown by this capture and stays open here. (5) The band worked: CH off at 14:53:58 (room 20.76, target 20.5) with PWM active, resumed 16:14:58. Not known from the log: whether the DHW burns were taps or keep-warm (question for indigo). Report sent to Sergeant D by DM (1571924364050432032) with the overview chart; to Robert via the coordinator.

2026-11-16 Q-1998b queued for Sergeant D (after Q-2005a): do the short DHW burns belong in SAT's cycle statistics and 4 h window (7 burns of 1-16 s logged as SAT cycles #2-#5, window n=5), given entry 390? If he answers they must not count, the fix goes into 2.0.0 (coordinator).
<!-- SECTION:NOTES:END -->
