---
id: TASK-1927
title: >-
  SAT self-tune signal: replay gruijter field captures through the real
  exclusion checks and the proposed options (4, 3, 1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T08:21:00Z'
updated_date: '2026-11-14T09:42:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 987000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, #dev-sat-mqtt 2026-11-14: 'I agree. Let's try and improve this.' (https://discord.com/channels/812969634638725140/1105556725714649128/1571075495091511307) and 'Are there other runs you can use?' (1571075689166278717), after our options post 1571074438248661063 / 1571074445026525275. Field runs: capture-gruijter-1111 (beta.171, 20 tuner hours, 0 counted), capture-gruijter-20261113-pm (beta.218, 17 hours, 0 counted), graph CSVs of 9-13 Nov. Options: (4) keep the current day's partial counters across a restart (today not kept, SATcontrol.ino:6505); (3) thermo-nova solar-gain event (solar_gain.py:80-102) instead of the 12 deg sun gate (:6297, entry 161); (1) min-mod ticks with the room below target may vote raise only (:6301, entry 174). Analysis only, no firmware change; any change to the checks is Sergeant D's call afterwards.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The real satTuneTickReason() is compiled on the host from src/OTGW-firmware/SATcontrol.ino (no re-implementation) and reproduces the logged SATTUNEH reasons of both transcripts hour by hour
- [ ] #2 Per run a table: hours counted today and per option (4, 3, 1, and combined), labelled FIELD; the graph CSV runs labelled as replays of the checks without tuner lines
- [x] #3 Inputs the logs do not contain (for example sun elevation, solar-gain state) are named with the substitute used, or the option is reported as not checkable on that run
- [x] #4 Results posted to Robert in #dev-sat-mqtt with the evidence path; nothing is changed in the firmware
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Replay done 2026-11-14 (agent tune-replay). Evidence: C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/2c61e56a-5a01-4190-8efd-eb547d986f4e/scratchpad/tune-replay/out/ (step1_*.txt, tables.md, diag_*.txt, opt4_loss.txt, opt1_errtr.txt). Step 1: real tuner blocks cut from each run's own commit and host-compiled reproduce reason and counted in 67/67 hours over 5 gruijter runs (r1 beta.171 20 h, r2 beta.218 17 h, beta.167 11 h, beta.214 7 h, beta.68 12 h); remaining diffs only ticks/mean in boot hours (capture gaps) and one phase count (held PID error, SUBSTITUTE).

Counted hours (logged/today/opt4/opt3/opt1/1+3/1+3+4): r1 0/1/1/1/1/1/1; r2 0/0/0/0/0/0/0; b167 0/4/4/4/4/4/4; b214 0/0/0/0/0/0/0; b68 0/3/3/3/3/3/3. The 8 extra hours of today's code are gas PWM hours (ADR-231), warm-up only, votes 4 raise 4 band. Options gain 0 hours: (1) min_mod is mostly SAT sending CH=0 with the room above target; room-below-target ticks fall in hours excluded for other reasons, 0 raise votes; (3) thermo-nova solar_gain event is active in nearly every sun hour (fires on a 0.01 C/min step; disabled by default const.py:132); (4) the 5 restarts held 0 clean hours, no run reaches 24 h. Observation: one excluded tick excludes the whole hour (ADR-205 d2); hours with 78-98 % clean ticks fall on 2-20 dhw/sun/flame_off ticks. SUBSTITUTE for opt 1/3: held PID error, 60 s room sampling, valves_open=True.

Not done: the graph CSV runs (9-13 Nov) and tjfs b161 (no SATTUNEH lines) were not replayed; AC2 stays open for those. Posted to Robert: #dev-sat-mqtt 1571097840329953301 (interim 1571092002416893993). Subagent could not write REPORT.md (harness blocks report files for subagents).
<!-- SECTION:NOTES:END -->
