# SAT low-load execution backlog

Date: 2026-10-28. Execution epic: **TASK-1412**. Planning source: **TASK-1411**.

This index records the execution breakdown of the [remediation plan](sat-low-load-remediation-plan-2026-10-28.md) and [verification plan](sat-low-load-verification-plan-2026-10-28.md). It does not record completed firmware work or physical validation. All 26 new child tasks start To Do. The 19 reused tasks keep their existing status, priority and ordinal; only their execution prerequisites and epic references change.

## Scheduling contract

The previous largest task ordinal was 549000. The epic is 550000 and its children occupy 551000 through 576000. Existing tasks are not renumbered. These values append the new programme to the existing queue; dependencies determine which task may execute. This does not create a dependency on every unrelated open task.

Start with TASK-1412.1, then TASK-1412.2. After the reviewed contracts are available, regression preparation (TASK-1412.3) and equipment-profile preparation (TASK-1412.17) can proceed independently. Bench-rig preparation (TASK-1412.20) can proceed after the profile while software fixes continue. Among ready tasks, retain existing queue ordering. A task with unresolved control approval remains blocked even if its technical dependencies are Done: read its question/decision references before implementation. The contract task records these scoped blockers; it does not invent their answers or wait for every unrelated question in TASK-1340.

Use the graph as a partial order: independent sensor, capability, mode, PID and telemetry work can proceed when their prerequisites permit it. Avoid a single global serial chain. Existing phase fixes are reused: TASK-1382 precedes TASK-1380; TASK-1368 precedes TASK-1270 and TASK-1381; TASK-1405 and classifier verification precede TASK-1331. Open designer approvals remain in force. The record task preserves the current one-question-at-a-time asking policy; this execution order is not permission to reorder or send Discord questions.

## New child tasks and direct prerequisites

| Task | Deliverable | Depends on |
|---|---|---|
| TASK-1412.1 | Reconcile SAT low-load question, decision and task evidence records | None |
| TASK-1412.2 | Establish reviewed OT low-load requirements and scoped decision contracts | TASK-1412.1 |
| TASK-1412.3 | Add executable low-load contract regressions before implementation fixes | TASK-1412.2 |
| TASK-1412.4 | Honor slave low-load permission across SAT entry and active PWM | TASK-1412.3 |
| TASK-1412.5 | Make SAT flow and return measurement validity and age explicit | TASK-1412.3 |
| TASK-1412.6 | Separate PWM phase actuation from overshoot protection enablement | TASK-1412.3 |
| TASK-1412.7 | Correct low-load entry qualification and post-DHW recovery | TASK-1412.4, TASK-1412.5, TASK-1412.3, TASK-1380, TASK-1382, TASK-1383 |
| TASK-1412.8 | Make SAT low-load exits correct across long and jittered scheduler intervals | TASK-1412.3 |
| TASK-1412.9 | Revalidate PWM start counting and phase duration policy after anchor fixes | TASK-1412.3, TASK-1380, TASK-1382, TASK-1383, TASK-1385, TASK-1375 |
| TASK-1412.10 | Verify SAT PID startup, held derivative and saturation semantics | TASK-1412.3, TASK-1270, TASK-1368, TASK-1381 |
| TASK-1412.11 | Make SAT command delivery and latest-state recovery verifiable | TASK-1412.5, TASK-1412.3, TASK-1370, TASK-1384 |
| TASK-1412.12 | Validate missing-return ignition and runtime startup on both transports | TASK-1412.5, TASK-1412.11, TASK-1371, TASK-1384 |
| TASK-1412.13 | Verify MaxTSet negotiation and every transmitted SAT ceiling | TASK-1412.3, TASK-1373 |
| TASK-1412.14 | Prove classifier input parity before cycle-end mode switching | TASK-1412.3, TASK-1405, TASK-1387 |
| TASK-1412.15 | Validate SAT telemetry against independent event and wire evidence | TASK-1412.3, TASK-1389, TASK-1409, TASK-1407 |
| TASK-1412.16 | Quantify repaired SAT duty and PID behaviour against controlled plant models | TASK-1412.10, TASK-1412.9, TASK-1412.7, TASK-1412.8 |
| TASK-1412.17 | Define equipment-specific gas, heat-pump and hybrid commissioning profiles | TASK-1412.2 |
| TASK-1412.18 | Pass the complete SAT low-load host and production integration gate | TASK-1412.4, TASK-1412.5, TASK-1412.6, TASK-1412.7, TASK-1412.8, TASK-1412.9, TASK-1412.10, TASK-1412.11, TASK-1412.12, TASK-1412.13, TASK-1412.14, TASK-1412.15, TASK-1331, TASK-1390, TASK-1365 |
| TASK-1412.19 | Pass reproducible closed-loop low-load simulation before hardware bench | TASK-1412.18, TASK-1412.16, TASK-1412.17 |
| TASK-1412.20 | Prepare and calibrate an isolated SAT real-board bench simulator | TASK-1412.2, TASK-1412.17 |
| TASK-1412.21 | Execute SAT low-load real-board bench simulation on PIC and OT-Direct | TASK-1412.19, TASK-1412.20, TASK-1412.15 |
| TASK-1412.22 | Approve bench evidence and authorize controlled equipment validation readiness | TASK-1412.21 |
| TASK-1412.23 | Commission accepted SAT candidate on controlled real heating equipment | TASK-1412.22, TASK-1412.17 |
| TASK-1412.24 | Prepare tester field protocol and readiness after bench and equipment acceptance | TASK-1412.23, TASK-1412.22 |
| TASK-1412.25 | Run authorized matched SAT tester field validation and publish measurements | TASK-1412.24, TASK-1412.22 |
| TASK-1412.26 | Independently verify SAT low-load implementation and release evidence | TASK-1412.25, TASK-1412.1 |

The epic depends on TASK-1412.26. No child depends on its parent; parentage groups the work and does not substitute for execution dependencies.

## Existing implementation tasks reused

These dependencies are recorded in Backlog metadata, rather than only in this document. They were empty before this sequencing update; existing approval notes remain intact. Integration verifies their outcomes again on the combined candidate.

| Existing task | Required prerequisites |
|---|---|
| TASK-1270 | TASK-1412.3, TASK-1368 |
| TASK-1331 | TASK-1405, TASK-1387, TASK-1412.14, TASK-1412.4, TASK-1412.7 |
| TASK-1365 | TASK-1412.3 |
| TASK-1368 | TASK-1412.3 |
| TASK-1370 | TASK-1412.3, TASK-1412.5 |
| TASK-1371 | TASK-1412.5, TASK-1412.11, TASK-1382 |
| TASK-1373 | TASK-1412.1, TASK-1412.3 |
| TASK-1375 | TASK-1412.3 |
| TASK-1380 | TASK-1412.3, TASK-1382 |
| TASK-1381 | TASK-1412.3, TASK-1270 |
| TASK-1382 | TASK-1412.3 |
| TASK-1383 | TASK-1412.3 |
| TASK-1384 | TASK-1412.3, TASK-1412.5 |
| TASK-1385 | TASK-1412.1, TASK-1412.3 |
| TASK-1389 | TASK-1412.3 |
| TASK-1390 | TASK-1412.5 |
| TASK-1405 | TASK-1412.3 |
| TASK-1407 | TASK-1412.3 |
| TASK-1409 | TASK-1412.3 |

TASK-1387 is an already completed classifier rule prerequisite, retained explicitly. TASK-1365 remains In Progress with its recorded recovery question; this planning update does not close it. TASK-1385 must first be revalidated against landed work before a duplicate fix is attempted. TASK-1373's inaccurate work-order wording is repaired under TASK-1412.1 before implementation.

## Mandatory validation sequence

1. **TASK-1412.18:** combined production host/integration gate, including the relevant project checklist and software LL checks.
2. **TASK-1412.19:** reproducible closed-loop simulation against plant/load/fault models.
3. **TASK-1412.21:** real-board bench simulation, using the separately prepared TASK-1412.20 rig, with an emulated plant on both verified PIC and OT-Direct transports.
4. **TASK-1412.22:** independent bench acceptance for the exact candidate and settings. Missing or failed scenarios block acceptance.
5. **TASK-1412.23:** controlled real-equipment commissioning under manufacturer limits; verify actual burner/compressor state and hand-back.
6. **TASK-1412.24:** tester protocol and readiness, including accepted artifacts, measurement requirements and rollback.
7. **TASK-1412.25:** authorized matched field validation. It depends directly on bench acceptance as well as tester readiness.
8. **TASK-1412.26:** independent final proof, supported-equipment scope and release evidence review; only then may the epic close.

No tester field run can start before TASK-1412.22 passes. Reaching a task is not authorization to contact a tester: obtain Robert's authorization when contact is needed. A changed candidate must rerun affected integrated and bench gates before redeployment. The review must check evidence revision/configuration identity as well as task status; a historic Done state for another binary is insufficient.

The bench initially uses an emulated slave/plant, with no live boiler required. It establishes actual board/protocol behaviour. Real ignition, heat delivery, compressor protection and efficiency require the later equipment/field stages. PH-PIC, PH-OTD, PH-PROFILE and PH-FIELD remain distinct from simulated LL checks. Missing hardware or measurement means blocked or explicitly excluded support, not a waived pass.

## Review issue ownership

The following are execution owners before the final independent review. Existing fix ownership and exact LL/PH closure criteria remain in the linked remediation register and task acceptance criteria. TASK-1412.26 audits every row; it is not used as a substitute implementation owner.

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

## Verification and evidence rules

Run the commands and scenario procedures in the [verification plan](sat-low-load-verification-plan-2026-10-28.md). TASK-1412.3 creates the missing integrated contracts; this index does not claim they already exist. Baseline runner success is not release acceptance. Mandatory skips, expected failures, missing evidence and unapproved behaviour cannot produce a release pass.

Archive candidate commit/build/filesystem IDs, source/test/configuration hashes, simulator fixtures/seeds, raw timestamped protocol and independent physical measurements, calibration, expected/actual outputs, failures, reviewer and rollback result. The task graph and issue ownership are also available in [the execution manifest](sat-low-load-execution-backlog-2026-10-28.json). Dependencies live in Backlog metadata; the manifest is an auditable snapshot and must be refreshed if scheduling changes.

The [sequencing audit](evidence/2026-10-28-low-load-backlog-order.json) checked actual task metadata: 47 graph nodes, no dependency cycles, all 42 issues with a non-final execution owner, queue-tail ordinals, matching dependencies and mandatory bench ancestry for equipment and field stages. Its topological waves show possible parallel work; human approval blockers still apply. This is scheduling evidence only.

The governing decisions and their agreement/conflict treatment are listed in remediation-plan section 2. Every implementation task must reopen its own cited source and relevant recorded entries before acting. No accepted ADR, designer quotation or upstream pin changes in this scheduling update.

## Re-sequencing 2026-10-28 (after Sergeant D answered Q15-Q51)

Sergeant D's answers (entries 67-94) removed most of the approval blockers, and gruijter's alpha.514 capture
(entry 92) shows the defect testers feel today: after hot water the PWM OFF wait restarts on every hot-water
flame-off (~23.5 min CH delay), and an ON phase that starts flame-off ends after one tick. The dependency graph
stays; the order inside it changes so that this defect is fixed first, with old-vs-fix proof.

- TASK-1412.2 is split. A thin PWM-anchor contract (entries 43, 80, 89-91, 92) is written first and unblocks a
  PWM slice of TASK-1412.3. The full OT low-load crosswalk (low-load permission, measurement validity) runs in
  parallel and does not gate the PWM slice.
- Wave 1 (critical path): TASK-1412.1 (records, mostly done 2026-10-28) -> PWM contract slice -> TASK-1412.3
  PWM slice (real-code harness as tests/test_sat_hp_pwm.py; replay of the 2026-10-27 hot-water draws and the
  slow-ignition case, red on HEAD) -> TASK-1382 + TASK-1380 (one candidate) -> TASK-1383 -> TASK-1407
  (pwm_on/pwm_off in the SAT log, so the fix is visible) -> bench old vs fix on the Classic.
- Wave 2: TASK-1421 (count at the ON start, never a hot-water flame; hard dependency on TASK-1382) ->
  TASK-1419 (heat pumps) -> TASK-1420 (sustained switches).
- Wave 3: TASK-1368 -> TASK-1270 -> TASK-1381, then the PID parity tasks one at a time (they share SATpid.ino):
  TASK-1416, TASK-1417, TASK-1418, TASK-1414, TASK-1415 (write cadence is Robert's call).
- Wave 4: TASK-1413 (auto-tune removal; API removal recorded in ADR-198, Proposed), TASK-1409, TASK-1412.4/.5/.6,
  TASK-1365 close-out.
- Blocked on Sergeant D: TASK-1373 (Q15b, Q41), TASK-1375 (Q16), TASK-1355/ADR-199 (Q20b), Q28 rounding.
- Validation sequence TASK-1412.18 -> .26 is unchanged.
