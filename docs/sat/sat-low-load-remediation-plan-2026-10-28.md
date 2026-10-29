# SAT low-load remediation plan

Date: 2026-10-28 (Europe/Amsterdam). Planning deliverable: **TASK-1411**.

Execution breakdown: **TASK-1412**, with 26 child tasks and reused existing fixes. See the [ordered backlog and mandatory bench-to-field gates](sat-low-load-execution-backlog-2026-10-28.md). The task IDs there replace the provisional `NEW-*` work-order labels below; the issue register remains the scope reference.

Status: proposed work programme, with existing decisions identified separately from new recommendations. This document does not approve a boiler-control change, move the thermo-nova pin, amend an Accepted ADR, or claim that the controller has passed physical validation.

## 1. Outcome and scope

Make the SAT controller's low-load behaviour explainable, interoperable and verifiable from room demand through actual OpenTherm frames to measured heat delivery. Repair the demonstrated port defects before comparing alternative controller models. Correct the documentation and evidence records without rewriting historical statements as if they were current or creating quotations/decisions for Sergeant D.

The scope is every finding raised in the 2026-10-27/28 review conversation: firmware behaviour, task/decision-record quality, the validity of low-load claims, and the comparative-research gaps. It is not all 69 findings of the older port audit, nor authorization to replace SAT with a different controller. Adjacent existing tasks are included where they are necessary to make the reviewed control path work coherently.

Deliverables accompanying this plan:

- [Verification protocol and executable commands](sat-low-load-verification-plan-2026-10-28.md).
- [Pinned SAT equations, timelines and control-theory research](sat-control-algorithm-research-2026-10-28.md).
- [Comparison with boiler and low-temperature heat-pump controls](sat-low-temperature-control-comparison-2026-10-28.md).
- [Baseline and acceptance runner](../../scripts/verify_sat_low_load.py). A successful baseline process is not a controller release verdict.
- [Pinned-reference trace adapter](../../scripts/inspect_sat_reference.py) for the actual upstream PWM duration, ultra-low and relight traces.
- Durable [port baseline report](evidence/2026-10-28-low-load-baseline.json) and [reference trace report](evidence/2026-10-28-sat-reference.json).

## 2. Frozen reference and evidence rules

Review baseline: `dev`, firmware commit `4c7498a28945b510166999cb7c27e0d1b889d4bc`. Unrelated working-tree state when this work began: modified `.wolf/anatomy.md`, untracked `compile_commands.json`. Neither belongs to TASK-1411.

SAT behaviour reference: `other-projects/SAT-releases-thermo-nova/`, pinned at `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. Upstream references below are relative to `custom_components/sat` at that commit. Firmware references are relative to `src/OTGW-firmware` at the review baseline. Line numbers describe that snapshot; future implementations must reopen the location and record their own commit.

External application contract: OpenTherm Application Functional Specification v1.1f, printed pp6-10, 17, 24-25. The supplied original is `D:/Users/Robert/Documents/GitHub/RvdB/OTGW-other-projects/OT-Spec/OT Application Functional Specification v1-1f.pdf`; the adjacent Markdown is a navigation aid because its tables/figures are damaged. The repository [protocol v4.2 text](../opentherm%20specification/OpenTherm-Protocol-Specification-v4.2.md) defines the data IDs, especially lines 1603-1611, 2015-2040 and 2381-2384.

Use four distinct evidence columns in implementation records:

| Column | Establishes | Does not establish |
|---|---|---|
| Protocol/application requirement | External behaviour or interoperability contract | A particular PID, gain or duty formula |
| Recorded human decision | Desired SAT behaviour or an explicitly chosen deviation | What a commit executes or what a boiler did |
| Source/host/replay result | Executed software behaviour under stated inputs | Real ignition, minimum firing, COP or comfort |
| Boiler/heat-pump/field measurement | Observed physical outcome in the named installation | Universal suitability or seasonal savings |

The detailed low-load recipe in application section 2.1.1.2 is presented as a suggested solution. Section 2.1.1.2.10 explicitly mandates checking and honouring ID3 HB4. Relative modulation 0% means minimum firing output, not OFF (section 3.2). Designer preference and Python parity do not remove an external interoperability obligation. Accepted ADR-196 already preserves the OpenTherm specification's authority in its Scope (lines160-163); this plan applies that existing boundary rather than requiring a new authority policy.

### Governing architecture and human decisions

Read and preserve Accepted ADR-051 (settings/state), ADR-157 (manual heating source; detection is a non-control hint), ADR-179 (SAT owns MsgID1), ADR-195 (cold cutoff/CH semantics), and ADR-196 (SAT source and decision process). ADR-197/198/199 are **Proposed**, not governing replacements for their Accepted predecessors. The current `<=` cutoff remains the existing implementation/ADR-195 baseline while contradictory Q20 wording is reconciled; this plan does not choose a different boundary.

Relevant decision entries and treatment:

| Entries | Treatment in this programme |
|---|---|
| 1, 64-66 | Keep the pinned learned ON-temperature model. Correct version evidence; research is not approval to adopt another branch. |
| 5, 7-12, 17 | Preserve requested-vs-sent CS distinction, cold cutoff and PWM OFF CS10/CH0; verify actual master bits. |
| 15, 18, 36, 37, 55 | Keep classifier prerequisites before cycle-end mode switching. Rule-table parity alone is insufficient. |
| 20-23 | Preserve approved startup/suppression defaults, source/MM exceptions and faster suppression path until a reviewed replacement is approved. |
| 28, 42 | Preserve custom deadband PID policy; implement the already decided 60-second cadence separately from unresolved Q23-Q28 policy. |
| 30, 31, 40, 43, 50, 52 | Distinguish period, minimum commanded ON, first phase and actual flame/compressor runtime. Q35/Q38 remain explicit control-change approval dependencies. |
| 39, 47, Q33/Q34 | Preserve the recorded OT-loss/hand-back decision and identify unresolved recovery policy. CS0 releases ownership; it is not proof of burner OFF. |
| 44 | Command-refresh direction is decided; transport deadline, the 5-second scheduler and traffic feasibility still require a concrete design. |
| 45 | Missing-return ignition is an approved direction with unresolved functional proof; do not implement its literal command sequence as a proven universal ignition procedure. |
| 48, 51, 54 | Preserve the user ceiling intent; correct ID49 bounds / ID57 value / ID6 support in the work order. |
| 58-60 | Preserve phase-only counting intent; correct DHW/window terminology and distinguish master CH-enable from slave CH-mode. |

No recorded entry covers enforcement of the slave low-load veto or a complete per-ID freshness contract. Those require an explicit proposal and the project's control-change review, rather than an invented historical approval.

## 3. Complete issue-to-remediation register

Priorities here are recommendations, not silent changes to existing task metadata. `NEW-*` names are proposed work-order labels, not created Backlog IDs. Existing implementation tasks retain their recorded status and approval requirements. `LL-*` identifiers refer to the companion verification plan; automated availability is explicitly stated there.

| Issue | Finding to resolve | Work package / backlog | Verification / closure |
|---|---|---|---|
| R01 | ID3 HB4 is read/published but ignored by SAT control | WP2 / NEW-CAP | LL-010; veto blocks every entry path and active PWM response is defined |
| R02 | Ultra-low zero-ON schedule can request ON | WP3 / TASK-1383, Q38 | LL-001; zero ON cannot produce CH1 through expiry or entry |
| R03 | Old flame anchor aborts an initial ignition attempt | WP3 / TASK-1382, Q35 | LL-002; delayed ignition does not flap the phase |
| R04 | Fixed 60-second integral dose can run on faster ticks | WP4 / TASK-1368 | LL-003; main and area cadence independent of actuator tick |
| R05 | Supported ticks above 60 seconds disable runtime exit timers | WP3 / Q44, TASK-1367 follow-up | LL-004; all supported ticks and delayed scheduler calls |
| R06 | Overshoot trigger is a proxy, not proof of minimum-power saturation | WP2/WP6 / NEW-ENTRY | LL-012; startup, setpoint fall, pump changes, max-temperature and DHW cases |
| R07 | Turning off overshoot protection also bypasses PWM OFF actuation | WP2 / NEW-MODE | LL-017; either explicit approved mode semantics or reject ambiguous combination |
| R08 | Return absent/invalid/stale is treated as a numerical value | WP2/WP5 / TASK-1371, Q40, NEW-DATA | LL-011/LL-018; distinguish measurement states and ignition fallback |
| R09 | Three-second refresh can reuse stale flow indefinitely while the link is alive | WP2/WP5 / NEW-DATA | LL-011; per-ID age and actual response-to-write deadline |
| R10 | Queueing order is not confirmed boiler receipt; full queue can lose writes | WP5 / NEW-TX, TASK-1370 | LL-013; queue acceptance, on-wire confirmation and retries scoped separately |
| R11 | 180 seconds described as unconditional physical flame minimum | WP1/WP3 / Q35/Q38 and entry31 clarification | LL-002/LL-019; record phase and each actual ignition separately |
| R12 | Duty temperature ratio is described too readily as heat delivery | WP6 / NEW-MODEL | LL-016; power/energy and room-error measurements, no watts inferred from ratio |
| R13 | Integral reset/deadband discontinuity, held derivative and lack of actuator-tracking anti-windup | WP4/WP6 / Q23-Q28, NEW-MODEL | LL-020; characterize boundaries/saturation before proposing a policy change |
| R14 | Heat-pump automatic PWM exclusion and generic timing are unresolved | WP7 / Q45, TASK-1366, Q37 | LL-015; device-specific permission and compressor protection |
| R15 | MaxTSet IDs and all-phase ceiling contract are inconsistent | WP5 / TASK-1373, Q41 | LL-014; correct bounds/value/support, rejects and ignition clamps |
| R16 | Classifier rule parity retains different inputs/percentiles/warm-up | WP6 / TASK-1405 before TASK-1331; TASK-1387 scope | LL-021; actual upstream and port on identical timestamped input |
| R17 | Device control profile must not follow an unreliable source-detection hint | WP2/WP7 / ADR-157, NEW-CAP | LL-010/LL-015; manual gas/HP/hybrid selection remains authoritative |
| R18 | First PID call can output zero until a later tick | WP4 / TASK-1270, Q39 | LL-003/LL-020; explicit approved startup output and restore cases |
| R19 | OFF timing can be moved by unrelated flame/DHW events | WP3 / TASK-1380, Q35/Q51 | LL-012/LL-019; phase-owned OFF anchor across unrelated ignitions |
| R20 | Range-5 reseeding task may overlap landed start-counting changes | WP1/WP3 / TASK-1385 vs TASK-1401 | LL-019; reopen current code, close/re-scope only with evidence |
| R21 | Runtime enable does not reproduce required startup PM/MI setup | WP5 / TASK-1384, TASK-1371 | LL-013/LL-018; PIC and OT-Direct equivalent behaviour, no assumed MI effect |
| R22 | Gas cycles range/default differs from recorded choice | WP3 / TASK-1375 | LL-019; defaults, migration and allowed3/4 values (policy still scoped by source) |
| R23 | MM units/value and cycle off-gap/duty statistics can mislead verification | WP5 / TASK-1389, TASK-1409 | LL-022; telemetry agrees with timestamped events and wire fields |
| R24 | Continuous previous-CS clamp differs from the pin | WP4 / TASK-1381 | LL-020; rising/falling demand, flame OFF and missing flow |
| R25 | Post-DHW guard is not the same as the spec's recovery sequence | WP2/WP6 / NEW-ENTRY | LL-012; compare actual recovery with application2.2.3 rather than just a5min inhibit |
| R26 | Task/decision records mix desired, current and verified behaviour | WP1 / TASK-1340 + NEW-RECORD | LL-023; current resolution index with distinct status/evidence fields |
| R27 | Q35/Q38/Q44/Q47 mix defects with policy and use broad 'keep today' alternatives | WP1/WP3 / existing questions and tasks | LL-023; one unresolved policy per question, concrete defect correction retained |
| R28 | Entry45 lacks an explicit supersession/experimental-evidence interpretation | WP1/WP5 / TASK-1371 | LL-018/LL-023; authored clarification, not a fabricated SD decision |
| R29 | Entry48 and task1373 AC retain ID57 bounds after notes correct ID49 | WP1/WP5 / TASK-1373 | LL-014/LL-023; work order and tests use actual protocol definitions |
| R30 | Entry31 conflates physical runtime and leaves P-vs-2P as a 'wording' issue | WP1/WP3 / Q38 and explicit follow-up | LL-019/LL-023; pin behaviour and intended policy both recorded |
| R31 | Q36 and 'rolling hour/DHW pause/never blocks' explanations are stale or too broad | WP1/WP3 / TASK-1401, Q36, Q48-Q51 | LL-012/LL-019/LL-023; phase events vs ignitions, sliding vs anchored window |
| R32 | PID narrative omits retained weighted D and dynamic gain effects | WP1/WP4 / entry28 clarification | LL-020/LL-023; formula includes Kd(t)*heldRawD |
| R33 | Branch ancestry and confirmation chronology conflict in entries64-66 | WP1 / NEW-RECORD | LL-023; exact-SHA comparison both directions and separate quote/code attribution |
| R34 | Stale 'Open', 'now', 'to be filed', Q33 order and Q2-Q4 order; dangling C references/drafting text | WP1 / TASK-1340 + NEW-RECORD | LL-023; resolution index, live queue and resolvable references |
| R35 | Q40 options overlap protocol support, validity, source and age | WP1/WP2 / Q40, NEW-DATA | LL-011/LL-023; dimensions separated before asking age/fallback policy |
| R36 | Q46's 'about half' probability is unsupported; Q47 defers action one tick | WP1/WP3 / Q46/Q47 | LL-004/LL-023; measured jitter, same-update order and scheduled-time policy |
| R37 | Provisional Q48-Q51 lack explicit expiry/re-evaluation triggers | WP1/WP3 / TASK-1369/1401 | LL-019/LL-023; revisit Q49 after Q35 correction, preserve maintainer provenance |
| R38 | Checked ACs contradict evidence/defaults; old task14/16/43 records and ambiguous task IDs | WP1 / NEW-RECORD | LL-023; annotate original scope/successors; exact task identity before graph edits |
| R39 | Task1277~3s claim exceeds~6s replay evidence and does not cover startup | WP1/WP5 / TASK-1277 scope annotation | LL-011/LL-018/LL-023; corrected evidence class and actual sample gaps |
| R40 | 'Exactly Python' rule-table title can be mistaken for full semantic equivalence | WP1/WP6 / TASK-1387/1405/1331 | LL-021/LL-023; input fidelity and kept deviations explicit |
| R41 | No complete OT-requirement-to-test mapping or coherent release proof | WP8 / TASK-1411 plan + NEW-VALIDATION | All LL gates; no missing physical checks hidden by host green |
| R42 | Efficiency/comfort/compressor claims lack matched measured comparisons | WP7/WP8 / NEW-FIELD | LL-015/LL-016; pre-registered matched protocol and uncertainty |

> **2026-10-29 reconciliation notes (TASK-1412.1; the original rows and text above and below stay as written).** The current resolution index, with separate decision, implementation, host-test and physical-verification state per item, is [sat-low-load-resolution-index-2026-10-29.md](sat-low-load-resolution-index-2026-10-29.md).
>
> - R26: the owner is TASK-1412.1 (execution backlog R26 -> TASK-1412.1). TASK-1340 is Done but covers only the question queue, not the index. The text of WP1 step 4 "via Backlog MCP" reads "via the backlog CLI" (project rule: CLI only, never the MCP server).
> - R29: Q41 was answered after this plan was written (entry 100): MaxTSet bounds come from MsgID 49 with a 20/80 fallback, the value is always sent as MsgID 57 (SH=), there is no MsgID 6 gate, and SAT caps the CS itself. "ID6 support/writability" at lines 60 and 170 therefore means observation and rejected-write read-back only, not a precondition. TASK-1373 AC#1 and TASK-1412.13 AC#1 were aligned on this date. Underfloor default 50 per entry 110; the heat-pump default is still open.
> - R37: the trigger is met. Q48-Q51 are decided (entries 89-91, 95, 97). Q49 B is implemented in TASK-1421 (932cda228) after the Q35 fix TASK-1382 (de23e3ea1). Maintainer provenance is kept in entry 89 and TASK-1369. Remaining: TASK-1412.9 AC#2 host verification.

## 4. Work packages and implementation sequence

### WP1 - Repair the operational record without erasing history

Owner role: SAT documentation maintainer, with an independent source reviewer. Existing umbrella: TASK-1340. Proposed follow-up NEW-RECORD covers technical clarifications/current-state index, not another firmware redesign.

1. Add a current-decision index linked to original numbered entries, implementing tasks and commits, and separate host/replay/device/field evidence. Keep an explicit `reviewed_at_commit` and last reconciliation date.
2. Add an attributed technical review addendum for mistaken interpretations (PID held raw D, ID49/57/6, fixed versus sliding hour, pre-ignition reasoning, commit ancestry). Do not append invented Sergeant D quotations or label a reviewer inference as his answer. Any actual new answer is recorded under the existing append-only rule.
3. Reconcile open-question status/order. Keep the one-question-at-a-time process. Explain established defects and seek approval on a concrete correction; separate timeout, cap exceptions, source-specific behaviour and fallback choices. Add an explicit date/trigger for provisional Q49 reassessment after TASK-1382.
4. Correct mutable task ACs via Backlog MCP after reading each task; append historical completion-scope corrections rather than rewriting measured results. Resolve duplicate/ambiguous task identities with the maintainer/tooling before changing dependencies. Do not blindly reopen all historical Done tasks.
5. Reconcile Proposed ADR-197/198/199 text with current decisions and task states. Accepted ADRs remain immutable; if source-precedence wording or control architecture must change, author a Proposed successor through ADR Kit and wait for human lifecycle authority.

Acceptance: every R26-R40 record item has a current resolution link; no technical mapping is offered as a vote; all claims of present implementation name a commit; no prior evidence is promoted into real-boiler proof. A second reader tries to refute each changed technical statement.

### WP2 - Define capability, data validity and mode contracts

Owner role: OpenTherm/SAT integration implementer. New work orders NEW-CAP, NEW-DATA, NEW-MODE and NEW-ENTRY should be created only after scope/approval review, reusing existing tasks where appropriate.

1. Model ID3 results as allowed, disallowed, explicit UnknownDataID, not-yet-known/invalid/stale. Apply the specified UnknownDataID fallback. Do not equate silence or temporary invalidity with an explicit unsupported response. Proposed interim behaviour before valid capability is known must be reviewed, not guessed.
2. Enforce the veto centrally, so runtime overshoot, cycle-end re-enable, restored state and any command path cannot bypass it. Define what happens if disallow appears during an ON phase: proposal is to relinquish master cycling to ordinary temperature demand in one coherent update; this choice needs the controller review/ADR appropriate to its scope.
3. Keep source selection manual per ADR-157. HB1(on/off type), HB2(cooling), HB4(permission), ID14 support and manufacturer quirks are separate dimensions.
4. Define data records for last valid ID25/28/17/3 value, valid/invalid/unsupported state, source and monotonic timestamp. Determine age limits and fallback by physical role. A positive old float is not a valid current measurement. Avoid new hot-path String allocation; state is transient per ADR-051.
5. Explicitly resolve what an enabled PWM scheduler with `overshoot_protection=false` means. Candidate A: disable master cycling and report temperature control; candidate B: retain time-proportional CH OFF while separately disabling only ON suppression. Both change current semantics and must be put to Sergeant D; this plan chooses neither silently.
6. Document overshoot entry as an anticipatory heuristic. Test minimum-power information when reliable, absence of ID17, max-temperature cutout, startup high fire, rapid setpoint reduction, pump step and DHW recovery. Keep existing 2C/60s policy unless a recorded decision revises it.

Acceptance: LL-010/011/012/017; all unknown cases have an explicit owner/fallback; no automatic heat-source inference controls equipment policy; the OT functional crosswalk names any difference from the suggested entry recipe and the evidence supporting it.

### WP3 - Correct temporal control as one coherent state machine

Order: Q35/Q38 approval -> TASK-1382 and1383 together with TASK-1380 timing interactions -> Q44/Q46/Q47 correction -> revalidate TASK-1385 and Q48-Q51 -> TASK-1375 source-specific range.

1. Keep explicit phase start, first-ignition synchronization and actual flame-edge timestamps separate. Start OFF timing from the approved phase event; unrelated DHW or relight edges must not reset it. Keep zero as a sentinel only with a rollover-safe explicit initialized state.
2. Require admissible mapped ON duration on every OFF/IDLE->ON path, including ordinary expiry. Cover all five duty ranges, entry, the hour limit and no-flow IDLE. The already-running-burn entry exception remains the recorded provisional policy until resolved.
3. Process the initial ignition wait before ordinary ON expiry. Decide the no-flame timeout policy explicitly (upstream continues timing; current port aborts). Preserve the approved cold cutoff, user stop and safety precedence. A normal phase minimum is not a promise that external safety or the boiler cannot stop a burn.
4. Use sustained-condition anchors consistently for entry and exits. The first qualifying observation starts the window. After gaps, either revalidate continuity or apply an approved missing-sample rule; do not count unobserved intervals as proven sustained conditions. Test all configured10..300s ticks, scheduler jitter, delayed calls and millis wrap.
5. Establish same-update ordering: evaluate mode/state transitions, compute CS/MM/CH from one resulting state, then enqueue the tuple. Never publish the new mode with an actuator tuple from the previous mode. Queue atomicity is handled in WP5, not assumed here.
6. Revalidate range5 reseeding against landed TASK-1401 before implementing TASK-1385. Preserve the sliding PWM-start limiter chosen for the port; separately document the upstream anchored-hour difference and ineffective normal cap. Revisit failed-start counting when Q35 is corrected.

Acceptance: LL-001/002/004/019, no known expected-failure marker for repaired behaviour, current versus pre-fix sources demonstrate red/green. Include 6% radiator demand above the cold cutoff, long OFF, delayed ignition and a still-burning OFF edge.

### WP4 - Make PID demand deterministic before retuning

1. TASK-1368: independent60s main/area PID updates with one approved startup evaluation. Actuator/suppression updates use the held demand without accumulating another integral step. After a scheduler outage, define coalescing explicitly; do not replay multiple60s doses against one stale sensor sample.
2. TASK-1270/Q39: agree initial output using current curve/P/restored state, then test enable/reboot/restore with and without valid sensors. Combine scheduler changes so the startup bug is not merely moved to another timer.
3. TASK-1381: bring rising/falling continuous clamp, previous CS, missing flow and flame-OFF cases under the chosen pin contract.
4. Characterize existing reset, deadband, clamp, persistence, Ki0, rounding and sensor-time behaviour under Q23-Q28. Include the held raw derivative multiplied by current gain. Do not replace the custom PID or add tracking anti-windup as an incidental fix.
5. If measurements motivate a different integral/saturation strategy, produce a separate Proposed ADR and compare it against repaired SAT with identical plant scenarios.

Acceptance: LL-003/020, ten normal integration opportunities in a600s test independent of10/30/60s actuator calls, defined startup exception, no catch-up dose burst, main and area outputs covered. Temperature/PID units and time bases are documented and checked.

### WP5 - Verify actuator delivery, feedback and limits

1. TASK-1370: define the maximum interval between logical demand updates and between actual boiler-facing writes. 'Every update' with a300s configurable tick does not implement a5s actuator service. Preserve the faster suppression path until a measured equivalent is approved. Benchmark queue load and coexistence with ordinary OT polling.
2. Track intended value, enqueue acceptance, actual transmission and available protocol/PIC acknowledgement separately. A PIC command echo is not proof of boiler acceptance or firing. Do not mark a rejected/full-queue write as delivered; coalesce superseded state deliberately and test latest-state recovery.
3. For a genuinely non-working ID14 path, measure age of the ID25 sample used, scheduling delay, transport delay and the resulting CS. The application2-3s guidance applies to following latest flow; a3s timer reading a6s-old sample is different evidence. Set feasible engineering latency budgets before bench acceptance; if the budget cannot be met, reject that strategy for the tested transport/device rather than hide the mismatch.
4. TASK-1371: reopen the missing-return design with the OT startup sequence beside it. Prove ignition with the actual equipment; include a hot boiler in anti-cycle lockout, supported/unsupported/invalid/stale return and runtime enable. Never assume MI500 changes gateway-mode polling; verify PIC and OT-Direct independently.
5. TASK-1373: correct the AC before implementation: ID49 bounds, ID57 value, ID6 support/writability; fallbacks and unsupported/rejected writes explicit. Clamp all transmitted CS paths, including ignition, refresh, continuous and recovery, to the effective user/equipment ceiling. Persist the setting, not transient measurements.
6. Reconcile TASK-1384 startup polling, TASK-1389 MM units/source and TASK-1409 off-gap/duty statistics. Misleading graphs must not become the evidence oracle.

Acceptance: LL-011/013/014/018/022 on both PIC and OT-Direct. Include queue saturation, interrupted frame exchange, device reset, stale cache and read-only remote parameters. No uncontrolled repeated command flood on an unknown response.

### WP6 - Close classifier/model gaps before changing control authority

1. Keep TASK-1405 before TASK-1331. Rule-order parity from TASK-1387 is retained but not renamed full equivalence: per-sample requested/control setpoints, warm-up exclusion, percentile interpolation, missing-data stand-ins and mode-at-sample fidelity remain separately audited.
2. Replay the same timestamped samples through real upstream and port classifiers, stating approved margin/cold/label deviations. Include mid-burn PWM->continuous, boot with existing flame, DHW tail, very short cycles and changing setpoints. Assert mode transitions as well as labels.
3. Evaluate the temperature-ratio duty against heat delivery. At fixed minimum power, average power approximately equals duty*minimum power; water-side heat is mass-flow*cp*(flow-return). Temperature ratio alone is not either measurement. Keep the held ON estimate as the baseline; test adaptation to TRVs/pump changes and OFF cooling rather than assuming raw OFF temperature is better.
4. Characterize the ultra-low dead zone, high-duty continuous region, recomputed phase durations, integral resets and saturation transitions. Record room error, actual heat and start events together. Do not optimize only a cycle-class label.

Acceptance: LL-016/020/021; no cycle-end hook until its input semantics pass; an alternative estimator must improve pre-registered physical/comfort criteria and stay within equipment constraints.

### WP7 - Separate boiler and heat-pump suitability

1. Define a commissioning profile: source/emitter type, native controller mode, supported OT functions, verified minimum output, equipment minimum ON/OFF/start limits, hydraulic minimum flow/volume, maximum flow temperature and measurement availability. Unknown equipment limits block a claim of compressor protection.
2. Respect entry21's heat-pump/hybrid MM exception. Gas relativeMM0 semantics do not prove an arbitrary HP interprets MM0 as minimum compressor power. Test actual interface behaviour.
3. Resolve Q45 automatic-HP entry only after WP2/WP3. Keep native modulation/weather compensation as the comparison baseline. Avoid competing room thermostats or arbitrary demand interruptions that conflict with native defrost/DHW/protection logic.
4. Compare repaired SAT with native weather compensation + room influence, a bounded PI/TPI design where appropriate, and predictive control only where required measurements/model quality exist. Any new controller family or pin move needs a separate reviewed ADR/task, not a tuning switch added under this plan.

Acceptance: LL-015/016; gas results do not close HP gates; record actual compressor state where flame status is not meaningful. No universal180s, cph or efficiency claim is derived from a generic profile.

### WP8 - Execute staged verification and decide release scope

Follow the companion protocol in order: record baseline -> red contract probes -> fix one bounded work package -> green host contracts -> closed-loop simulation -> hardware transport replay -> actual equipment -> matched field comparison. Do not use a richer simulator as substitute for real physical evidence.

The field protocol must hold comfort and equipment protections constant when comparing energy/cost. Record weather, indoor/outdoor temperatures, emitter/valve configuration, heat/electricity or fuel, DHW/defrost/backup use and sensor uncertainty. A tariff-cost improvement can coexist with worse COP. Do not compare headline percentages from different buildings as a ranking.

Release evidence includes commit/build identifiers, settings snapshots, raw frames with timestamps, instrument calibration, scenario IDs, expected and actual results, exceptions, reviewer and rollback outcome. All mandatory physical gates must be present and passed for a named supported equipment/transport combination. Unmeasured combinations remain unvalidated.

## 5. Delivery/merge order and dependencies

| Batch | Independently reviewable change | Hard dependency |
|---|---|---|
| A | Record/index/AC corrections and capability/data contracts | Current task/source reconciliation; no firmware change |
| B | New red tests and temporal state fixes1382/1383/1380 | Q35/Q38 approval; agreed timeout and phase semantics |
| C | PID startup/cadence and continuous clamp | Entry42; Q39 and relevant reset decisions; batchB tests stay green |
| D | Veto, measurement validity, transmit/refresh and ceiling implementation | WP2 design approval; preserve ownership/source constraints |
| E | Remaining temporal exit/order fixes and counting cleanup | Q44/Q46/Q47 approval as applicable; failed-start counting revisited |
| F | Classifier inputs, then end-of-cycle switching | TASK-1405 -> TASK-1331; TASK-1387 rule evidence retained |
| G | Source-specific commissioning and physical validation | B-F complete for the selected profile; unresolved HP policy blocks HP enablement |
| H | Optional model/controller experiment | Repaired baseline plus matched data; separate ADR/approval |

Do not bundle all firmware work into one PR. Each worker reopens governing entries, reads the then-current task and records a plan before implementation. New gaps discovered beyond this register require an explicit scoped follow-up, not opportunistic expansion.

## 6. Completion, rollback and stopping rules

Planning completion (TASK-1411) means the artifacts, runner and research are delivered and reviewed. It does not mean implementation tasks are Done or that SAT is certified. The runner may correctly demonstrate blocked acceptance on the current firmware.

Implementation completion requires relevant checks in [the project checklist](../guides/pr-checklist.md), all scenario evidence and a clear statement of supported hardware. Stop a physical trial on configured equipment limits, sensor invalidity, unplanned rapid cycling, loss of native protection or contradictory ownership. Use the installation's approved SAT-disable/hand-back procedure and verify the actual master/boiler state; do not interpret CS0 as physical heating OFF. Restore saved settings and approved firmware if needed. Never flash PIC firmware over WiFi with OTmonitor, never add Serial debug on the PIC link, and never bypass equipment protections to make a test pass.

## 7. Verification of the plan itself

- Cross-check all42 issue rows against the session findings and their work-package sections.
- Check existing task IDs/states via MCP, including ambiguity instead of selecting a same-number archived task.
- Check every relative document link, referenced script and available command; label future tests as to-be-created.
- Run the baseline runner and acceptance-mode negative control; store their distinct verdicts.
- Independently challenge source claims, proposed approvals, measurement definitions and evidence-class boundaries.
- Record build/check results and reviewer limitations with TASK-1411. Do not silently accept an ADR or rewrite the decision log to close this planning task.

## 8. New evidence from this planning pass

The exact develop snapshot named in entries64-66 is **older**, not a newer successor to the pin. Public GitHub compare results checked2026-10-28 for fixed SHAs:

| Base -> head | Result | Ahead | Behind | Merge base |
|---|---|---:|---:|---|
| `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb` -> `7d43560342ca9d854e92b8b5a91bdfbf9cbc2152` | behind | 0 | 294 | develop snapshot |
| develop snapshot -> release pin | ahead | 294 | 0 | develop snapshot |

Reproduce using the [pin-to-develop API comparison](https://api.github.com/repos/Alexwijn/SAT/compare/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb...7d43560342ca9d854e92b8b5a91bdfbf9cbc2152) and [reverse comparison](https://api.github.com/repos/Alexwijn/SAT/compare/7d43560342ca9d854e92b8b5a91bdfbf9cbc2152...7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb). This corrects the review conversation's unverified 'newer develop' wording. It does not settle which algorithm is preferable, reinterpret a quote as new approval, or authorize moving the pin. Entry64's ancestry statement matches this graph; entry66's opposite interpretation needs an attributed technical clarification. (Written 2026-10-29 as sergeant-d-corrections.md entry 125.)

## 9. Primary source locations supporting the register

- Upstream `pwm.py:130-160,185-207,219-301`: estimator, phase gate, ignition, cycle-end hook, duty map. `heating_control.py:197-238,262-376,380-448,450-523`: update order, entry/exits, override and MM. `pid.py:60-132,159-232`: gains, output, custom I/D; `climate.py:499-502,706-715`: freeze and schedulers. `device/types.py:8-31`: current capability/state representation.
- Firmware `SATcontrol.ino:387-396,827-848,899-1147,1637-1658,5074,5291-5355,5390-5456`: MM, startup, schedule, cached measurements, PID/areas and emission. `SATpid.ino:170-202,275-343`: integral/cadence. `SATcycles.ino:285-325,999-1093`: sliding count and transitions. `SATchEnable.h:26-51`: cutoff and phase CH rule. `OTGW-Core.ino:3118-3136,3744-3766`: permission meaning and queue behaviour.
- [Question queue](open-questions.md), especially Q35/Q38/Q40-Q51; [decision log](sergeant-d-corrections.md), entries listed in section2; [port audit](sat-port-audit-2026-10-24.md). These historical documents are evidence of statements and findings, not proof of the current runtime.
- [OTGW background](https://otgw.tclcode.com/background.html#custom): practical discussion of minimum-capacity cycling. Its suggested controller choices are advice, not extra normative OT requirements.
