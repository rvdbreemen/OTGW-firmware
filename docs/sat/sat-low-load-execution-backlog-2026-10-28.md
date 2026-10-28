# SAT low-load execution backlog

Updated 2026-10-28, after decision entry 111. Execution epic: **TASK-1412**.
This current index supersedes the earlier wave notes. Backlog metadata remains the live dependency authority; status values below are a snapshot, not a release verdict.

Read the [builder handoff](sat-low-load-agent-handoff-2026-10-28.md), [remediation scope](sat-low-load-remediation-plan-2026-10-28.md) and [verification protocol](sat-low-load-verification-plan-2026-10-28.md). Machine-readable [manifest](sat-low-load-execution-backlog-2026-10-28.json) and [audit](evidence/2026-10-28-low-load-backlog-order.json) were refreshed from actual tasks.

## Work that can proceed

Continue the existing TASK-1368 and TASK-1409 ownership. Decision entry 111 unblocks TASK-1368. Pending SATpid.ino writers execute serially:

**1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415**

TASK-1381 is a separate branch after TASK-1270 and its Q53 approval. It does not block independent PID changes. TASK-1415 remains gated by Robert's persistence write-cadence choice. Revalidate each task against the preceding commit: an already implemented fix needs evidence and task reconciliation, not a duplicate patch.

TASK-1409 reporting and TASK-1425 telemetry can proceed separately from PID writers. TASK-1413 auto-tune removal feeds model-baseline verification. Capability, measurement and mode work retains its scoped contract prerequisites. This order preserves the reconciliation committed in 6dda1e15a; TASK-1412.3 is not reinstated as a global blocker for fixes with their own approved contract and executable regressions.

No task status, assignee or queue ordinal is changed by this update. The initial epic and 26 children were appended at ordinals 550000..576000 after the previous 549000 maximum; later follow-ups keep their existing positions. Dependencies determine readiness. Completed tasks can retain historical prerequisites without being reopened; candidate integration still has to pass the combined gates.

## Required verification ownership

- TASK-1412.7 now includes TASK-1422 and TASK-1424, so combined post-DHW regressions are required.
- TASK-1412.8 includes TASK-1420 plus TASK-1428/1429; TASK-1428 keeps the TASK-1412.5 measurement-validity contract prerequisite.
- TASK-1412.10 includes PID tasks 1414..1418 and TASK-1423.
- TASK-1412.15 includes TASK-1409 and TASK-1425, so misleading telemetry cannot become the scoring oracle.
- TASK-1412.16 includes TASK-1413; the repaired model baseline cannot bypass auto-tune removal.
- TASK-1412.18 includes TASK-1426; an unanswered heat-pump modulation contract cannot disappear from final acceptance.
- TASK-1412.3 integrates the focused suites, including tests/test_sat_pwm_anchors.py, into the LL runner. Its status does not imply those integrated checks already exist.

## Human decision gates

Read the correction log and each task's latest plan. Entry 111 answers Q55; entry 110 replaces the older underfloor default decision. Q53, Q54/TASK-1426, the remaining MaxTSet default/write details and Robert's persistence write cadence remain separate recorded gates. The entry-100/ID6 interoperability conflict is not resolved by this graph. Interpretations in entries 104/107/108 are not silently upgraded to designer confirmation. The question queue remains one question at a time; this index authorizes no Discord or tester messages.

## Bench and release sequence

Each control fix needs archived old-versus-new board bench proof for its exact candidate before a tester alpha. That preserves Robert's recorded alpha policy. Host tests/builds alone do not satisfy it. Formal validation remains:

1. TASK-1412.18: complete production host/integration gate.
2. TASK-1412.19: reproducible closed-loop simulation.
3. TASK-1412.21: real-board bench simulation on both named transports, using the TASK-1412.20 rig.
4. TASK-1412.22: independent bench acceptance.
5. TASK-1412.23: controlled real-equipment commissioning.
6. TASK-1412.24: tester readiness/protocol.
7. TASK-1412.25: authorized formal field validation.
8. TASK-1412.26: independent final implementation/evidence review.
9. TASK-1412: close only after the final gate.

Bench-rig preparation can proceed independently when its prerequisites permit it. A changed binary/settings combination invalidates affected acceptance and requires reruns. OTGW replay blocks transmission: its result is board-executed controller replay, not PIC wire-delivery or physical boiler/compressor proof. PH-PIC, PH-OTD, PH-PROFILE and PH-FIELD remain distinct and mandatory for claimed combinations.

## Live dependency snapshot

The following includes the full epic prerequisite closure, including completed implementation tasks and all relevant SAT follow-ups. TASK-1427 is unrelated UI work and is excluded.

| Task | Status at snapshot | Direct prerequisites | Deliverable |
|---|---|---|---|
| TASK-1270 | To Do | TASK-1368 | SAT sends CS=10/CH=0 for the first ~60 s after enable because the first PID call returns the uninitialised 0 |
| TASK-1331 | In Progress | TASK-1405, TASK-1387, TASK-1412.14, TASK-1412.4, TASK-1412.7 | SAT: port thermo-nova's on_cycle_end PWM re-enable (continuous cycle classified OVERSHOOT) |
| TASK-1365 | In Progress | TASK-1412.3 | SAT: on lost OpenTherm hand the boiler back once (CS=0, CH=1, MM=T), then send nothing (entries 39, 47) |
| TASK-1368 | In Progress | None | SAT PID: fixed 60 s update cadence as thermo-nova (entry 42) |
| TASK-1370 | To Do | TASK-1412.3, TASK-1412.5 | SAT: send CS, MM and CH on every control update as thermo-nova (entry 44) |
| TASK-1371 | To Do | TASK-1412.5, TASK-1412.11, TASK-1382 | SAT PWM ignition without a return temperature: flow-based suppression from the start, CS/PM=25 flood for 20 s, MI=500 (entry 45) |
| TASK-1373 | To Do | TASK-1412.1, TASK-1412.3 | SAT: user MaxTSet slider bounded by MsgID 57, sent to the boiler (entry 48) |
| TASK-1375 | To Do | TASK-1412.3 | SAT: gas boiler PWM cycles per hour 3 or 4 only, default 3 (entries 30, 52) |
| TASK-1380 | Done | TASK-1412.3, TASK-1382 | SAT PWM: time the OFF phase from PWM's own ON->OFF switch, not from the last boiler flame-off (audit #59) |
| TASK-1381 | To Do | TASK-1270 | SAT continuous mode: add thermo-nova's previous-CS term to the flow clamp |
| TASK-1382 | Done | TASK-1412.3 | SAT PWM: time the ON phase from its start and keep waiting for ignition as thermo-nova (Q35) |
| TASK-1383 | Done | TASK-1412.3 | SAT PWM: start an ON phase only when the mapped ON time is at least 180 s (Q38) |
| TASK-1384 | To Do | TASK-1412.3, TASK-1412.5 | SAT: send the boot PM/MI commands also when SAT is enabled at runtime (audit #44) |
| TASK-1385 | To Do | TASK-1412.1, TASK-1412.3 | SAT PWM: reseed the effective boiler temperature in duty range 5 as thermo-nova |
| TASK-1387 | Done | None | SAT: make the end-of-cycle classifier match thermo-nova's classifier.py exactly (entry 55, Q4 A) |
| TASK-1389 | To Do | TASK-1412.3 | MQTT MaxRelModLevelSetting/override always 0.00 and declared as temperature (degrees C) in HA discovery |
| TASK-1390 | To Do | TASK-1412.5 | SAT: stop SAT when the outside temperature is missing or old (entry 56, Q7 B) |
| TASK-1405 | To Do | TASK-1412.3 | SAT classifier: store the control setpoint per tail sample, as thermo-nova (entry 36, Q2 A) |
| TASK-1407 | Done | TASK-1412.3 | SAT log: add PWM ON and PWM OFF times in seconds (Sergeant D request 2026-10-26) |
| TASK-1409 | In Progress | None | SAT cycles: stored off-gap is always 0, so the 4h average off-time reads 0 and the duty ratios (per cycle, 4h, 24h) read 1.0 |
| TASK-1412 | To Do | TASK-1412.26 | SAT low-load remediation epic: ordered implementation, bench acceptance and field proof |
| TASK-1412.1 | To Do | None | SAT low-load: Reconcile SAT low-load question, decision and task evidence records |
| TASK-1412.2 | To Do | TASK-1412.1 | SAT low-load: Establish reviewed OT low-load requirements and scoped decision contracts |
| TASK-1412.3 | To Do | TASK-1412.2 | SAT low-load: Add executable low-load contract regressions before implementation fixes |
| TASK-1412.4 | To Do | TASK-1412.3 | SAT low-load: Honor slave low-load permission across SAT entry and active PWM |
| TASK-1412.5 | To Do | TASK-1412.3 | SAT low-load: Make SAT flow and return measurement validity and age explicit |
| TASK-1412.6 | To Do | TASK-1412.3 | SAT low-load: Separate PWM phase actuation from overshoot protection enablement |
| TASK-1412.7 | To Do | TASK-1412.4, TASK-1412.5, TASK-1412.3, TASK-1380, TASK-1382, TASK-1383, TASK-1422, TASK-1424 | SAT low-load: Correct low-load entry qualification and post-DHW recovery |
| TASK-1412.8 | To Do | TASK-1412.3, TASK-1420, TASK-1428, TASK-1429 | SAT low-load: Make SAT low-load exits correct across long and jittered scheduler intervals |
| TASK-1412.9 | To Do | TASK-1412.3, TASK-1380, TASK-1382, TASK-1383, TASK-1385, TASK-1375, TASK-1419, TASK-1421 | SAT low-load: Revalidate PWM start counting and phase duration policy after anchor fixes |
| TASK-1412.10 | To Do | TASK-1412.3, TASK-1270, TASK-1368, TASK-1381, TASK-1414, TASK-1415, TASK-1416, TASK-1417, TASK-1418, TASK-1423 | SAT low-load: Verify SAT PID startup, held derivative and saturation semantics |
| TASK-1412.11 | To Do | TASK-1412.5, TASK-1412.3, TASK-1370, TASK-1384 | SAT low-load: Make SAT command delivery and latest-state recovery verifiable |
| TASK-1412.12 | To Do | TASK-1412.5, TASK-1412.11, TASK-1371, TASK-1384 | SAT low-load: Validate missing-return ignition and runtime startup on both transports |
| TASK-1412.13 | To Do | TASK-1412.3, TASK-1373 | SAT low-load: Verify MaxTSet negotiation and every transmitted SAT ceiling |
| TASK-1412.14 | To Do | TASK-1412.3, TASK-1405, TASK-1387 | SAT low-load: Prove classifier input parity before cycle-end mode switching |
| TASK-1412.15 | To Do | TASK-1412.3, TASK-1389, TASK-1409, TASK-1407, TASK-1425 | SAT low-load: Validate SAT telemetry against independent event and wire evidence |
| TASK-1412.16 | To Do | TASK-1412.10, TASK-1412.9, TASK-1412.7, TASK-1412.8, TASK-1413 | SAT low-load: Quantify repaired SAT duty and PID behaviour against controlled plant models |
| TASK-1412.17 | To Do | TASK-1412.2 | SAT low-load: Define equipment-specific gas, heat-pump and hybrid commissioning profiles |
| TASK-1412.18 | To Do | TASK-1412.4, TASK-1412.5, TASK-1412.6, TASK-1412.7, TASK-1412.8, TASK-1412.9, TASK-1412.10, TASK-1412.11, TASK-1412.12, TASK-1412.13, TASK-1412.14, TASK-1412.15, TASK-1331, TASK-1390, TASK-1365, TASK-1426 | SAT low-load: Pass the complete SAT low-load host and production integration gate |
| TASK-1412.19 | To Do | TASK-1412.18, TASK-1412.16, TASK-1412.17 | SAT low-load: Pass reproducible closed-loop low-load simulation before hardware bench |
| TASK-1412.20 | To Do | TASK-1412.2, TASK-1412.17 | SAT low-load: Prepare and calibrate an isolated SAT real-board bench simulator |
| TASK-1412.21 | To Do | TASK-1412.19, TASK-1412.20, TASK-1412.15 | SAT low-load: Execute SAT low-load real-board bench simulation on PIC and OT-Direct |
| TASK-1412.22 | To Do | TASK-1412.21 | SAT low-load: Approve bench evidence and authorize controlled equipment validation readiness |
| TASK-1412.23 | To Do | TASK-1412.22, TASK-1412.17 | SAT low-load: Commission accepted SAT candidate on controlled real heating equipment |
| TASK-1412.24 | To Do | TASK-1412.23, TASK-1412.22 | SAT low-load: Prepare tester field protocol and readiness after bench and equipment acceptance |
| TASK-1412.25 | To Do | TASK-1412.24, TASK-1412.22 | SAT low-load: Run authorized matched SAT tester field validation and publish measurements |
| TASK-1412.26 | To Do | TASK-1412.25, TASK-1412.1 | SAT low-load: Independently verify SAT low-load implementation and release evidence |
| TASK-1413 | To Do | None | SAT: remove the firmware-only hourly heating-curve coefficient auto-tune (entry 72, Q22) |
| TASK-1414 | To Do | TASK-1270, TASK-1418 | SAT PID: reset integral and raw derivative on every target, preset, window and HVAC change as thermo-nova (entry 73, Q23) |
| TASK-1415 | To Do | TASK-1368, TASK-1423 | SAT PID: persist and restore all PID state as thermo-nova, no age check (entry 74, Q24) |
| TASK-1416 | To Do | TASK-1368, TASK-1270 | SAT PID: remove the firmware-only integral cap of +-20 and the 10 s minimum between updates (entry 75, Q25) |
| TASK-1417 | To Do | TASK-1368, TASK-1416 | SAT PID: fixed deadband 0.1, remove the deadband setting and its widening (entry 76, Q26) |
| TASK-1418 | To Do | TASK-1368, TASK-1417 | SAT PID derivative: no-change test == 0 and last temperature updated every update, as pid.py (entry 77, Q27) |
| TASK-1419 | Done | TASK-1382 | SAT PWM: heat pumps skip the hourly start limit and enter PWM through the 60 s overshoot switch (entries 81, 87; Q36, Q45) |
| TASK-1420 | Done | None | SAT: sustained switches as thermo-nova: overshoot >=, underheat/saturation timers, scheduled tick time, switch before CS (entries 86, 88; Q43, Q44, Q46, Q47) |
| TASK-1421 | Done | TASK-1382 | SAT PWM: count a start at the ON start (not first flame) and never count a hot-water flame (entries 89, 90; Q48, Q49) |
| TASK-1422 | Done | TASK-1421 | SAT PWM: pause the ON timer while the boiler DHW-active bit is set, then restart the start-up sequence (entry 95, Q48b) |
| TASK-1423 | To Do | TASK-1368, TASK-1414 | SAT PID: round every PID value to 3 decimals and match thermo-nova's Ki-near-0 clamp (entry 95, Q28a/Q28b) |
| TASK-1424 | Done | None | SAT PWM: an ON start right after a hot-water draw with the DHW flame still lit does not count against cycles per hour (entry 105, Q52D/Q52E) |
| TASK-1425 | To Do | None | SAT: cycles_this_hour saturates at 6 (rolling-hour ring SAT_MAX_CYCLES_PER_HOUR); heat pumps now have no start bound |
| TASK-1426 | To Do | None | SAT: heat pump MM in PWM is the user maximum, outside PWM always 100 (entry 21 open: 100 or user maximum?) |
| TASK-1428 | To Do | TASK-1412.5 | SAT sustained switches: no flow-validity check (thermo-nova resets on flow None) |
| TASK-1429 | To Do | None | SAT DHW 300 s guard: arm on sub-tick draws and count from the real end of the draw |

## Issue coverage

Every R01..R42 has a non-final execution owner. Exact findings and LL/PH criteria remain in the remediation register and task acceptance criteria.

| Issue | Execution owner(s) |
|---|---|
| R01 | TASK-1412.2, TASK-1412.4 |
| R02 | TASK-1412.3 |
| R03 | TASK-1412.3 |
| R04 | TASK-1412.3, TASK-1412.10 |
| R05 | TASK-1412.3, TASK-1412.8 |
| R06 | TASK-1412.2, TASK-1412.7 |
| R07 | TASK-1412.2, TASK-1412.6 |
| R08 | TASK-1412.2, TASK-1412.5, TASK-1412.12 |
| R09 | TASK-1412.2, TASK-1412.5 |
| R10 | TASK-1412.11 |
| R11 | TASK-1412.1, TASK-1412.9, TASK-1412.23 |
| R12 | TASK-1412.16, TASK-1412.25 |
| R13 | TASK-1412.10, TASK-1412.16 |
| R14 | TASK-1412.2, TASK-1412.17, TASK-1412.23, TASK-1412.25 |
| R15 | TASK-1412.2, TASK-1412.13 |
| R16 | TASK-1412.14 |
| R17 | TASK-1412.2, TASK-1412.4, TASK-1412.17 |
| R18 | TASK-1412.3, TASK-1412.10 |
| R19 | TASK-1412.3, TASK-1412.7 |
| R20 | TASK-1412.1, TASK-1412.9 |
| R21 | TASK-1412.11, TASK-1412.12 |
| R22 | TASK-1412.9 |
| R23 | TASK-1412.15 |
| R24 | TASK-1412.3, TASK-1412.10 |
| R25 | TASK-1412.2, TASK-1412.7 |
| R26 | TASK-1412.1 |
| R27 | TASK-1412.1 |
| R28 | TASK-1412.1, TASK-1412.12 |
| R29 | TASK-1412.1, TASK-1412.13 |
| R30 | TASK-1412.1, TASK-1412.9 |
| R31 | TASK-1412.1, TASK-1412.7, TASK-1412.9 |
| R32 | TASK-1412.1, TASK-1412.10 |
| R33 | TASK-1412.1 |
| R34 | TASK-1412.1 |
| R35 | TASK-1412.1, TASK-1412.5 |
| R36 | TASK-1412.1, TASK-1412.8 |
| R37 | TASK-1412.1, TASK-1412.9 |
| R38 | TASK-1412.1 |
| R39 | TASK-1412.1, TASK-1412.12, TASK-1412.23 |
| R40 | TASK-1412.1, TASK-1412.14 |
| R41 | TASK-1412.2, TASK-1412.3, TASK-1412.18, TASK-1412.19, TASK-1412.20, TASK-1412.21, TASK-1412.22, TASK-1412.23, TASK-1412.24, TASK-1412.25 |
| R42 | TASK-1412.16, TASK-1412.17, TASK-1412.24, TASK-1412.25 |

## Audit and communication

The refreshed graph has **63 nodes**, zero cycles, all 42 issue owners, the required PID writer order, no Q53 dependency in front of independent PID work, and bench-acceptance ancestry for every formal equipment/field/final stage. The audit checks scheduling only. Its topological waves show possible work order; human approvals still apply.

The handoff is published on TASK-1412 and active-task comments for TASK-1368/TASK-1409, plus TASK-1270. AGENTS.md and CLAUDE.md point builders at it. No active builder was visible through the Codex agent messaging tools, so these shared task records and repository entry points are the delivery mechanism; no live acknowledgement is claimed.
