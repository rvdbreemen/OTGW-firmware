---
id: TASK-1653
title: >-
  SAT on PIC: request MsgID 25 itself when the thermostat never reads it
  (maintainer decision)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T07:00:00Z'
updated_date: '2026-11-05T11:07:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 777000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05: on a PIC installation whose thermostat never reads MsgID 25, SAT has no flow value (the PIC only listens). SAT asks for MsgID 25 itself through a PIC command, only when 25 was not seen for N s. The safe PIC command must be confirmed in other-projects/otgw-6.6 first (no guessing).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The PIC command and its behaviour are cited from otgw-6.6 (file:line) and checked against otmonitor usage before any code
- [ ] #2 SAT requests MsgID 25 only after N s without a sample, never in parallel with the thermostat's own reads; N and rate documented
- [ ] #3 Host test red on old / green on fix; bench on the Classic (.63) with a thermostat that does not read 25, or a replay that omits it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 research (opus agent, read-only, all cited lines opened): command = PM=25 (gateway.asm SetPrioMessage 4984-4995: RAM only, no EEPROM; dispatched 3920-3926). It reaches the bus only in an alternative slot: CreateRequest 2386-2413 takes SendAltRequest only for a thermostat request of an ID the PIC marks unsupported (UnknownMask 3491-3510); Alternative 3390-3441 serves PIC init msgs, then the priority msg, then the AA list. It REPLACES that thermostat request (thermostat still gets Unknown-DataID, SendAltResponse 2466-2476); never adds a frame. Cleared only by a boiler reply with that ID (HandleResponse 2430-2434, also the thermostat's own read of 25) or a newer PM=/TP=; no PIC timeout. Frames: T 80xx0000 -> R 80190000 -> B C019hhll -> A F0xx0000; firmware feed (SATboilerFeed.h:67-75) takes the B as Valid (traced, not run). Rejected: AA= (EEPROM list, 32 max, auto-deleted on Unknown-DataID), UI=/KI= (EEPROM, changes what the thermostat sees), PR= (prints settings), SR=/CR= (fake responses only). otmonitor never sends PM=/AA=/UI= itself (AC1 'checked against otmonitor' = otmonitor does not use it). thermo-nova sends PM=25 after every CS= (coordinator/mqtt/opentherm.py:195-197). Risks: no slot -> PM=25 never answered; overwriting the PIC's own fault-code/ID99 priority reads (2758-2759, 3265-3267) and cutting a TSP readout (TSPCount reset); PS=1 hides frames (gate off); monitor mode no transmit; standalone polls 25 itself. Robert 2026-11-05 (session): no-slot install -> accept + publish a 'PM=25 unanswered / no slot' status (no UI= opt-in); N = 20 s; trigger = no thermostat T25 for 300 s AND flow age >= 20 s AND a slot seen in the last 120 s AND PIC gateway mode, not PS=1, not pmChainBusy(), 25 not unsupported; at most one outstanding, re-arm on B25 or the existing 60 s slot timeout; hold off a few s after a fault-bit edge. Side finding: the existing entry-45 / TASK-1371 PM=25 flood (SATcontrol.ino:1818-1819) has the same slot dependency.

2026-11-05 implementation (opus agent, wt-1653; integrated on wt-1645b): new SATpm25Poll.h (no state; constants 300 s quiet / 20 s age / 120 s slot window / 10 s fault hold-off / 500 ms pairing), hook in processOT (OTGW-Core.ino ~5868, skips localAnswer/replayed), picPrioSlotBusy() reuses the 60 s priority-slot gate, satPm25PollStep() in SATcontrol.ino after satMiStep(); at most one PM=25 outstanding, at least 20 s apart; REST /api/v2/sat/status pm25_no_slot/sent/answered/unanswered, MQTT sat/pm25_no_slot (docs/api/MQTT.md). Robert 2026-11-05 (session): slot rule B, widened: besides 'T X then R Y', also 'T X then B X Unknown-DataID within 500 ms' counts as a slot (PIC prints R/A only when it substitutes; with an empty AA list the bus shows only T X / B X Unknown-DataID, gateway.asm 3447-3462, 2401, 2446; it blacklists after 3 rejections, 2429-2446). Tests: tests/test_sat_pm25_poll.py 16/16 (old: 11 FAIL, 2 skipped); mutants (spacing, hold-off) red; pic_pm_chain 40, ot_loss 24, restart_quiet 20 OK; evaluate 0 failed. Not verified: PIC 6.8 behaviour, bench on a PIC with a thermostat that skips 25 (needs such a thermostat or the TASK-1654 simulator).
<!-- SECTION:NOTES:END -->
