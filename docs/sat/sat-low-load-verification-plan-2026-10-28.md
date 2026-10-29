# SAT low-load verification plan

Date: 2026-10-28. Backlog: TASK-1411. Status: proposed verification contracts; no firmware changes or new SAT decisions are made by this document.

The issue-to-task map is [the remediation plan](sat-low-load-remediation-plan-2026-10-28.md). Its LL identifiers and this plan's identifiers refer to the same contracts; PH identifiers below describe physical evidence that the host runner cannot collect.

The acceptance question is whether SAT delivers the required heat smoothly below the boiler's minimum continuous output, respects the boiler's low-load permission and limits, and recovers correctly. A successful build, a passing mapper test, a replayed flame bit and a real physical burner run answer different parts of that question. None substitutes for the others.

## 1. Sources and authority

- OpenTherm **Application Functional Specification v1-1f**, local PDF and Markdown: `D:/Users/Robert/Documents/GitHub/RvdB/OTGW-other-projects/OT-Spec/OT Application Functional Specification v1-1f.pdf` and the same basename `.md`. Use the PDF for the figures. Relevant clauses: 2.1.1.2.1 detection, .2 capacity control, .3 OFF, .4 startup, .5 ON without ID14, .6 ON with ID14, .7 return to modulation, .10 interoperability; 2.1.1.3 pump overrun; 2.1.1.5 boiler minimum OFF time. Section 2.1.1.2 is a **suggested solution**, not a mandate to copy every step. Section .10 explicitly makes ID3/HB4 startup checking and the slave veto mandatory; an explicit Unknown-DataID response selects master low-load control. A timeout or never-read value is not that response.
- [OTGW low-load background](https://otgw.tclcode.com/background.html#custom): explanatory boiler behaviour, not a replacement for the specification or measured evidence.
- [Sergeant D corrections](sergeant-d-corrections.md): entries **1** (180 s startup filter, then held learned temperature), **5/9/17** (PWM OFF CS=10/CH=0, ON CH stays enabled), **28** (custom PID and filtering), **42** (independent 60 s PID cadence), **44** (CS/MM/CH every control update), **45** (proposed no-return startup), **50/52** (heat-pump period/minimum ON and defaults), **59** (count PWM ON starts), **64–66** (release pin versus the different recorded develop snapshot). This plan agrees with their recorded control intent and does not silently replace it. The historical claim that develop is newer is not an ancestry proof; verify commit ancestry before any migration. Entries 45 and the provisional Q48–Q51 paths require their documented open points to be settled before a corresponding control fix.
- **Status 2026-10-29 (TASK-1412.1, R37):** Q48-Q51 are no longer provisional: entries 89, 90, 91, 95 and 97 decide them, and TASK-1421/1422/1380 implement them after TASK-1382. Entry 45 is unchanged by this note.
- Upstream pin: `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, local read-only `other-projects/SAT-releases-thermo-nova/`. Citations in this plan are relative to `custom_components/sat`: `pwm.py:143-167`, `:185-215`, `:229-260`; `pid.py:159-192`; `climate.py:706-716`; `heating_control.py:236-238`, `:331-378`, `:380-448`.
- [Open questions](open-questions.md): Q35/Q38/Q44 are proposed temporal corrections, not already approved answers. Independent tests can expose the existing behaviour while those questions remain open. Changing the controller still follows the repository's SAT decision process.

Do not use develop's raw-flow duty behaviour as the release-pin oracle. Do not call a failure of the suggested no-ID14 method an OpenTherm encoding violation; identify the tested interoperability/functional contract separately.

## 2. Run what exists now

From the repository root in PowerShell, with Python and an existing host C++ compiler (`g++`, `clang++`, or Visual Studio C++):

```powershell
python scripts/verify_sat_low_load.py --help
$report = Join-Path $env:TEMP 'sat-low-load-baseline.json'
python scripts/verify_sat_low_load.py --mode baseline --output $report
Get-Content -Raw $report | ConvertFrom-Json | Select-Object verdict,baselineExecution,suiteTotals
```

The runner uses only Python's standard library and existing real-code host adapters. It compiles firmware functions into temporary host executables. It does not install a compiler, connect to a boiler, change settings, flash firmware or start a replay. Source override environment variables used by old-versus-new tests are cleared in workers and their names are recorded. Commit, branch, dirty paths, UTC start time, Python version, and source/test SHA256 hashes bind the report to the files actually tested. The commit alone is insufficient when sources are dirty.

The initial selected-suite baseline is:

| Existing module under `tests/` | Tests | Evidence |
|---|---:|---|
| `test_sat_pwm_override.py` | 72 | Extracted real PWM/MM/refresh functions, integral-freeze helper, source checks |
| `test_sat_pwm_start_limit.py` | 17 | Real PWM/cycle-ring code, entry/counting/lag/wrap scenarios |
| `test_sat_hp_pwm.py` | 22 | Real source/system getters and PWM mapper; includes one known expected failure |
| `test_sat_overshoot_sustain.py` | 17 | Real continuous-to-PWM sustain function plus bypass source checks |
| `test_sat_pwm_replay_fixture.py` | 5 | Replay fixture parity/content; does not prove a physical burner |
| Total | **133** | Initial result: zero ordinary failures/errors/skips, **one expected failure** |

Counts can change when tests are added. The report records actual counts rather than assuming 133 means success. The expected failure is `test_r3_on_phase_waits_for_flame`, tracked by TASK-1382/Q35. Some existing tests intentionally preserve current choices; passing them does not settle pending control decisions.

### Recorded run for TASK-1411

Full baseline executed on 2026-10-28 against firmware commit `4c7498a28945b510166999cb7c27e0d1b889d4bc` with the working verification files. Result:133 tests,0 ordinary failures,0 errors,0 skips,1 expected failure. LL-001/002/003/004 all failed their candidate contracts as described below. Baseline execution completed; acceptance remained **blocked** (23 blockers:18 unavailable checks,4 failed contracts,1 expected failure). The [full baseline JSON](evidence/2026-10-28-low-load-baseline.json) records executing-runner and target-source hashes; regenerate it with the command above to bind subsequent results to their own hashes. Release-mode contracts-only verification returned exit1 and a blocked verdict. No physical boiler was exercised by these runs.

The [pinned Python trace report](evidence/2026-10-28-sat-reference.json) is reproduced with `python scripts/inspect_sat_reference.py --output "$env:TEMP/sat-reference.json"`. This separate read-only adapter executes the actual reference PWM methods. It does not turn the current port's failed contracts into passes. Oracle negative controls also rejected empty observations, wrong CH outputs, an absent reference and a changed pin marker.

Direct suite command, also available without the runner:

```powershell
Push-Location tests
python -m unittest test_sat_pwm_override test_sat_pwm_start_limit test_sat_hp_pwm test_sat_overshoot_sustain test_sat_pwm_replay_fixture -v
Pop-Location
```

After a relevant fix, add the affected existing suites as appropriate: `test_sat_cycle_classifier.py`, `test_sat_room_loss.py`, `test_sat_ot_loss.py`, `test_sat_cmd_cache.py`, `test_sat_mqtt_fallback.py`. Run `python evaluate.py --quick` and `python build.py` for the firmware change, and apply [the PR checklist](../guides/pr-checklist.md). A verification-document/script change alone does not establish or require a new boiler measurement.

### Baseline versus release verdict

```powershell
python scripts/verify_sat_low_load.py --only contracts --mode baseline --output "$env:TEMP\sat-low-load-contracts.json"
python scripts/verify_sat_low_load.py --mode release --output "$env:TEMP\sat-low-load-release.json"
```

- Baseline mode exits **0** when the baseline runs without an unexpected regression or infrastructure error, even when candidate contracts fail. Its acceptance `verdict` remains **blocked**.
- Exit **1** means an ordinary test regression, unexpected success requiring expected-failure review, or a blocked release-mode run. Exit **2** means a worker/compiler/timeout infrastructure error. Compiler absence is not a passing probe.
- Release mode rejects expected failures, skips, failed contracts, omitted checks and unavailable checks. It is **deliberately blocked today**: several required integrated/hardware checks do not exist yet. There is no `allow-known-failures` or self-attestation flag.
- `--only suites` and `--only contracts` are diagnostic subsets. Their omitted checks block acceptance. `--timeout` bounds each isolated worker; stdout is JSON unless `--output` is supplied, and the short execution summary goes to stderr.

The JSON schema version is 1: `metadata`, `suites`, `suiteTotals`, `contracts`, `missingChecks`, `blockers`, `baselineExecution`, `verdict`, `evidenceLevel`. A future validation adapter must supply actual evidence for missing IDs before they can be removed from the required list; simply deleting a blocker is not verification.

## 3. Executable candidate contracts

These assertions describe the correction being assessed, not a replica of the implementation. Actual `.ino` functions execute under controlled clock/sensor/queue inputs. Source extraction errors fail visibly. The probes are intentionally red on the reviewed firmware.

| ID | Exact stimulus and assertion | Authority / current gap |
|---|---|---|
| **LL-001** | Gas, radiators, 3 cycles/h; flow 50°C, return 30°C, requested 28.568°C (above 28.2°C cutoff), flame OFF, last flame-off 3000 s ago. Call at 0/30/60 s. Mapped ON=0 must never request ON/CH. Record duty, durations, phase, CS, CH at every tick. | TASK-1383/Q38 A candidate; `pwm.py:154-155`, `:247-260`. Current output alternates CS48/CH1, CS10/CH0, CS48/CH1 despite ON=0. |
| **LL-002** | Same prior flame history, requested 38.6°C (50% calculated duty), no new flame through 60 s. The ON request must remain asserted during this initial ignition wait. | TASK-1382/Q35 A candidate; `pwm.py:185-215`. The expected-failure suite reproduces the premature end too. It does not settle what happens after 180 s without ignition. |
| **LL-003** | Compile all of `SATpid.ino`. Manual Kp=Kd=0, Ki=.01, deadband .1, curve40, target21, 600 s duration. Constant error .05, then the same physical-time error trace `.05 + .01*((elapsed/30)%2)`; supply samples on 10/30/60/120 s control ticks. Compare I with the 60 s reference. | Entry42/TASK-1368. This diagnoses the **current shared-tick call contract**, not the Home Assistant timer or full firmware scheduler. At the reviewed code, changing-trace I is .66/.66/.30/.15; constant-trace .30/.30/.30/.15. When PID scheduling moves, review this adapter and implement LL-020 rather than pretending direct sparse calls execute a new independent timer. |
| **LL-004** | Execute real `satCycleCheckPwmExits` under sustained underheat (flow40/request50) or saturation (OFF time0), in PWM ON. Ticks30/60/90/120/300 s, 30 samples each. Measure from **first qualifying sample**. Exit must be no earlier than 180/300 s and no later than one tick after it. | Q44 A candidate; `heating_control.py:331-378`. At30 s current exits occur after150/270 qualifying seconds; above60 s neither exit accumulates. |

LL-003's 0.0001°C integral comparison is a proposed numerical test tolerance, not an OT requirement. LL-004's one-tick allowance describes sampling resolution, not permission for an unbounded actuator delay. The runner's early candidate tests are not complete coverage of every source/system/configuration.

**Correction 2026-10-29 (TASK-1412.1, R36):** the LL-004 row describes the firmware before TASK-1420. Since alpha.521 (TASK-1420, entries 86, 95, 109) the underheat and saturation exits count from the first qualifying tick through satSustainedFor (SATcycles.ino:152, :1085, :1097) and fire on the first control tick at least 180 s / 300 s after it, so up to one tick later. Replayed-boiler bench on .63 (30 s tick only): 180.02 s and 300.03 s. The control timer runs SKIP_MISSED_TICKS (entry 109), so there is no catch-up grid. Still uncovered: jitter, delayed calls and millis rollover (TASK-1412.8 AC#2), and the exit-versus-CS order (open, Q47b in open-questions.md). This is not a physical-performance result.

## 4. Required deterministic integration matrix

The following checks are **planned and unavailable in the runner**. Existing helper tests provide partial coverage; they must not be represented as these whole-loop tests. Build adapters that call the real loop with an injectable monotonic clock, protocol input and observable command sink. Reuse a captured source trace against old and fixed commits. Do not reimplement the controller in the oracle.

| ID | Stimulus matrix | Required observable result |
|---|---|---|
| **LL-010** | Valid ID3/HB4=0, HB4=1, explicit Unknown-DataID, timeout, never read; boot and runtime change. Cross ID14 accepted/unsupported/rejected/ignored. | Read/configure permission explicitly. Veto prevents master low-load cycling; Unknown-DataID follows the specified fallback. Define timeout policy separately. Select the ID14/no-ID14 strategy by observed capability, not a manufacturer guess or 0 default. |
| **LL-011** | ID25/28 fresh, never seen, late, frozen positive, Data-Invalid and Unknown; ID0 continues. Delay/reorder frames; restore valid input. | Age and validity are observable per ID. A repeated cached value is not a new measurement. Assert the approved stale-data response and recovery; whole-link liveness cannot stand in for per-ID freshness. Missing-return policy depends on Q40/TASK-1371. |
| **LL-012** | DHW begins during continuous, PWM ON, PWM OFF and ignition; lasts shorter/longer than a phase; relight during DHW; ends near a timer boundary. | No CH suppression refresh corrupts DHW. Track actual DHW and CH heat separately. Entry59 counting excludes unrelated DHW/relights; recovery respects the approved guard. Q35/OFF-anchor decisions must be explicit. |
| **LL-013** | Empty/nearly-full/full command queue; withheld/late/negative/mismatched acknowledgements; repeated same-prefix commands; disconnect/reconnect; PIC and OT-Direct. | Compare intended command, accepted enqueue, PIC ack and boiler Read/WriteAck separately. Failed enqueue cannot silently become confirmed actuator state. CS/MM/CH transition order is checked on the outgoing wire; no stale queued ON survives a newer OFF decision. |
| **LL-014** | Requested/hold/flow below, at, above user/boiler/system MaxTset; missing/rejected bounds; lower the limit during ON and between 3 s refreshes. | Every emitted CS obeys the approved effective ceiling, including startup, refresh and recovery. Identify the actual bound/value IDs from the protocol rather than treating a UI slider as boiler acknowledgement. No limit exemption is inferred from the suggested max-setpoint startup method. |
| **LL-015** | Gas, heat pump and hybrid configured independently of radiators/underfloor. HP periods1800/2400 s; gas3/4 cycles/h. Test automatic entry, duty boundaries, physical restart delay and exits. | Exercise full-loop automatic mode selection, not just mapper arithmetic. Minimum **requested** ON180 s and period arithmetic agree with entries40/50/52; device-specific compressor rules are measured/approved separately. Q45's heat-pump exclusion must be settled. |
| **LL-016** | Steady occupied room target, warm-up, falling demand and external gains; compare requested/observed duty and calibrated delivered heat/energy under PH-FIELD. | Demonstrate comfort noninferiority and report actual heat delivery, fuel/electricity input and uncertainty. A lower CS or return temperature alone cannot pass this contract. |
| **LL-017** | Overshoot protection ON/OFF while continuous, PWM ON, PWM OFF and waiting for ignition; old persisted settings. | Observed phase, CS, MM and CH are mutually consistent with the approved option. A scheduler claiming OFF while retaining an ON CS/CH must not be reported as effective capacity control. Resolve the intended switch semantics before asserting the oracle. |
| **LL-018** | Return sensor absent, invalid, stale and later restored; boiler with/without a PIC; the proposed first20 s CS/PM25 sequence. | Missing return is distinguished from measured0°C. Execute the approved ignition strategy and measure polling/command timing. No20 s flood test passes if the ON phase aborts after one tick; Q35/Q40 and TASK-1371 dependencies apply. |
| **LL-019** | ON/OFF entry with flame already burning; ignition never arrives; flame lags after OFF; DHW edge during OFF; fourth PWM start; wrap; duty-range changes. | Phase clocks, first-flame sync, minimum request durations and actual start counts follow the recorded decisions. Distinguish mapped period, PWM starts and physical ignitions; Q35/Q38/Q48–Q51 remain visible. |
| **LL-020** | Clock grid10/30/60/120/300 s, ±jitter, delayed loop, missed tick catch-up, millis wrap; PID/command/refresh timers independently observed. Sweep deadband, solar freeze, missing/recovered room input, rising/falling request versus previous CS, overshoot/underheat equality and a one-sample dropout. | PID at the approved60 s cadence, including startup; command updates at approved cadence; no duplicate I catch-up for the same interval. P/I/D and missing-room recovery follow recorded decisions. The continuous clamp follows the approved previous-CS rule rather than a presumed flow-only rule (TASK-1381). LL-001–004 remain correct through the real call path. Check Q43/Q44 boundaries, Q46 jitter and Q47 same-tick mode transition. |
| **LL-021** | Setpoint change within the classifier tail, missing samples, DHW tail, short cycle, IDLE and boundary percentiles; feed the same trace to real Python and C++ classifiers. | Compare per-sample inputs and window rules as well as decision-table labels. Do not enable the cycle-end mode switch before TASK-1405's prerequisite is proven. |
| **LL-022** | Queue rejection, delayed acknowledgement, stale sensor, mode switch and restart while polling MQTT/REST/logs. | Published state separates requested/queued/acknowledged/measured values and ages; percentages have percentage units. A setpoint or flame shown on a graph is traceable to its source and timestamp. |
| **LL-023** | Compare current source, task AC/final summaries, open questions, correction entries and evidence links. | Historical decisions stay intact; add dated corrections for stale claims. Every claimed difference has independently checked citations. No Done task or accepted design statement becomes a physical-performance certificate. |

For LL-001–023 also sweep cold cutoffs21/28.2, five duty ranges and each threshold ±one representable step; flame already ON at PWM entry; no ignition/slow ignition/flame loss; boiler minimum OFF0/60/180/300/600 s. Include ramp-down after a large room-target drop and transient flow spikes after pump-speed changes: the application specification warns these can resemble low load. Inspect the learned-temperature window at179/180/181 s and after a relight, retaining the approved release-pin behaviour (entry1, `pwm.py:143-149`). These additions are not already implemented by the runner.

## 5. Real hardware verification: PH-PIC and PH-OTD

Use one PIC board and one OT-Direct board, recording the **actual detected mode**, board identity, running firmware hash, PIC version, boiler model/firmware, boiler parameters, sensor capability and SAT settings. A board containing a PIC but running OT-Direct is not PIC-path evidence. Use existing provisioned test equipment and controlled heating conditions; this plan does not authorize unattended flashing or configuration changes.

**PH-PROFILE is a prerequisite**, completed for each equipment configuration before actuator tests. Record the manufacturer's required minimum ON time, minimum OFF/restart time, minimum water flow, maximum supply temperature, boiler/compressor startup characteristics and any mandatory pump/bypass arrangement, with source and units. Mark inapplicable entries with a reason. An empty/unknown applicable limit blocks equipment acceptance; the generic180 s SAT setting cannot stand in for a compressor manufacturer's rule. Agree the proposed engineering thresholds below, capture frequency and sensor/meter uncertainty before examining the outcome.

1. Record a steady-load baseline with the existing controller and the candidate. Record settings before/after each run and restore the controlled test configuration. Keep the raw log and hashes; do not retain only chart screenshots.
2. Capture all four relevant wire perspectives where available: thermostat request, gateway-to-boiler request, boiler reply, gateway-to-thermostat reply. Timestamp them on one monotonic clock. Include IDs0,1,3,14,17,25,28,57 and the relevant unsupported/invalid responses. ID0/HB0 is **master CH enable**; ID0/LB1 is **slave CH mode**; ID0/LB3 is reported flame. Pump overrun after CH disable is a separate observation.
3. Capture intended demand, P/I/D, requested CS, actual emitted CS, MM, CH, phase, planned ON/OFF seconds, phase anchor, real flame edge, queued/acked command and per-ID measurement age. Existing telemetry may not expose all of these: record the gap and add instrumentation before treating the case as verified.
4. Independently observe physical flame/burner operation at ≥1 Hz where the appliance provides a trustworthy output or meter. An injected/replayed flame bit is not a physical burner measurement. Record ignition request-to-flame latency, flame-on duration, uncommanded extinguishes and actual OFF duration. Separate burner activity from pump operation and DHW.
5. Exercise first cold ignition, five successive low-load cycles, demand ramp upward back to continuous, demand ramp downward, DHW interruption and one realistic boiler anti-cycle delay. Repeat at two distinctly low heat demands below minimum continuous output. Repeat each critical transition at least three times; longer field observation follows below.
6. For a boiler where ID14 genuinely limits output: verify MM0 means minimum **relative** output, not burner OFF, and compare the chosen CS strategy with the specified max-CS method. For a boiler without effective ID14: verify the flow-following strategy using actual fresh ID25 measurements and actual emitted ID1 commands.

Proposed engineering acceptance targets, to be approved before running; these are **not extra OpenTherm mandates**:

| Measurement | Proposed target and interpretation |
|---|---|
| No-ID14 feedback | During established PWM ON, latest ID25 age at CS computation ≤3 s and compute-to-outgoing-ID1 ≤0.5 s under normal load. Report every exception and worst case. The 2–3 s refresh comes from the suggested method in2.1.1.2.5; the0.5 s budget is ours. A3 s timer with6–15 s-old input fails this target. |
| Suppression tracking | With approved offset1°C, emitted CS matches the latest valid flow−offset within0.2°C (format/quantization allowance), subject to the explicitly logged ceiling. Startup hold and clamps are separate phases. |
| Phase/command transitions | No requested ignition at mappedON0. No unexplained CH/CS toggling before approved ignition timeout. Command application delay≤one approved update period+measured bus latency. Measure, do not infer, actual wire order. |
| Minimum ON | No controller-commanded stop before the approved180 s minimum measured from the approved phase/first-ignition anchor, except a logged overriding condition. Report every relight's actual burn duration separately; the pinned release does not promise a fresh180 s minimum after every relight, and changing that requires a new policy. A boiler self-cutout is recorded as a control-performance problem, not hidden as a scheduler pass. Boiler faults, DHW and closed valves are classified separately. |
| Recovery | No maximum-power surge from restoring MM before a safe normal CS; within two valid control updates after a fault-free recovery, actual commands match current demand. Boiler anti-cycle delay is measured separately. |
| Repeatability | Critical transitions pass all three repetitions on each claimed transport/source class; no unexplained frame gaps, queue drops or settings changes. |

`scripts/verify_pic_cs_keepalive.py --help` documents a separate PIC override-expiry bench test. That script **changes SAT state** and its existing recipe assumes no boiler/thermostat. It is not run by the new runner and must not be copied onto a live heating setup as the physical low-load test. Likewise, `scripts/gen_sat_pwm_replay.py` and the existing replay fixture are transport/control regression tools; a replay is never a substitute for steps4–6.

## 6. Field performance and decision gate: PH-FIELD and PH-PROFILE

Use an A/B/B/A comparison against a competent reference controller or the previous firmware, with the same room target/schedule, emitters, valve positions, boiler parameters and DHW schedule. Proposed minimum: four comparable24 h windows, including at least two periods where required heat is below the measured minimum continuous boiler output. Record outdoor temperature, wind/solar/internal gains and occupancy changes. If conditions are not comparable, extend the trial; do not normalize away unexplained differences.

Measure:

- Room error: mean, MAE,95th percentile absolute error, maximum overshoot, and duration beyond ±0.3°C; split steady maintenance from scheduled warm-up. Proposed steady occupied comfort target:95% of absolute room errors≤0.3°C; MAE noninferiority margin≤0.05°C against the matched reference, only if sensor uncertainty supports that distinction. Otherwise improve measurement or agree a justified margin before the run. These targets are engineering choices, not specification limits.
- Actual ignition count/h, distribution of flame-on/OFF times, delay until flame, commanded versus uncommanded extinguishes, pump time and DHW interruptions. Proposed target: no more short uncommanded burns and no more actual CH ignitions per delivered kWh than reference. The configured SAT PWM-start cap is a different counter; report both.
- Delivered heat with a calibrated heat meter, or measured water flow and supply/return temperatures. For water, approximate `heat_kW = 1.163 * flow_m3_per_hour * (Tflow - Treturn)` and integrate over time; disclose calibration, sampling and the water/glycol assumption. Low flow temperature alone is not delivered heat.
- Fuel/electricity input measured separately from delivered heat. Allocate/exclude DHW explicitly. Gas input needs the applicable calorific conversion; heat-pump electrical input alone is not heat output. Compare efficiency only with meter uncertainty and matched operating conditions. A lower return temperature is useful evidence, not a measured efficiency percentage.

Claim an energy benefit only if comfort meets the agreed noninferiority criterion, equipment limits were never violated, and the uncertainty/confidence interval of the matched comparison excludes no improvement. Declare the interval method and how day-to-day weather variation and meter error were handled; extend the trial when four windows cannot support that conclusion. Cost, primary energy, heat delivered and heat-pump COP are separate outcomes and must not be interchanged.

Gross observed duty is `physical CH flame-on seconds / CH-demand wall interval`. The denominator includes ignition waiting and boiler anti-cycle lockout, so delayed heat delivery cannot be hidden by removing unavailable time. Report DHW preemption separately, with its duration and heat allocation. If an additional available-time-only duty is useful, name its distinct denominator and report it alongside gross duty. Compare these with the time-varying computed duty and report bias; none is an energy meter. Proposed target: for settled repeated cycles, observed delivered heat does not show a systematic shortage/excess larger than the stated meter uncertainty or10% of the reference demand, whichever is larger. Investigate exceptions before claiming improved efficiency.

An independent reviewer checks the raw frame/measurement locations and tries to refute each conclusion, including a current-versus-upstream difference. LL-023 and PH-PROFILE record the chosen pending design answers, accepted targets, exclusions, source/firmware hashes, missing cases and review findings. A test changing boiler control cannot supply its own missing design approval.

Acceptance requires all applicable deterministic contracts, protocol/capability checks, both claimed transport paths and the physical/field checks. If no heat pump was tested, explicitly scope the result to the tested gas-boiler configuration. No boiler measurement means **host/replay verified, physical low-load performance unverified**. The runner's current blocked verdict is the correct starting point for this programme, not a reason to weaken its checks.
