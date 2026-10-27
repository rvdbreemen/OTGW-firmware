# SAT control algorithm: source reconstruction and comparison with established control patterns

Date: 2026-10-28. Task: TASK-1411. Status: research and recommendations; no control change is authorized by this document.

## Assessment

SAT is a weather-compensated water-temperature controller with a deliberately switched room correction and a separate minimum-output cycling supervisor. Its strongest idea is the separation of room demand from the boiler setpoint needed to keep a minimum-output burn alive. Its weakest evidential point is the conversion of a temperature ratio into delivered heating duty: that conversion is an empirical approximation, and the present evidence does not establish its accuracy across changing hydraulics, boiler ignition delays, or heat-pump operating constraints.

The appropriate next step is to establish a correct, observable implementation of the decided SAT behaviour and compare it with a simple baseline on real equipment. Replacing it immediately with a conventional PID, different upstream branch, or predictive controller would confound implementation defects with algorithm quality. No measured efficiency superiority or universal stability claim is made here.

## Scope, authority and evidence

All `TN file:line` references below are relative to `custom_components/sat` in the read-only copy `other-projects/SAT-releases-thermo-nova/`, at **`7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`**. Each cited location was opened during this review. Firmware references describe `dev` at **`4c7498a28945b510166999cb7c27e0d1b889d4bc`**, not the desired SAT design. The working checkout contained unrelated changes, which this document does not incorporate or alter.

The review read the corrections first and follows ADR-196, whose source order governs SAT intent, while its Scope explicitly leaves the OpenTherm specification's authority unchanged. ADR-197, ADR-198 and ADR-199 were also read: all three remain **Proposed** in their frontmatter. Their source reconstructions are useful; a Proposed ADR is not evidence that its implementation has shipped or that an outstanding policy has been accepted.

| Recorded decision | Use in this research | Agreement or open issue |
|---|---|---|
| Entries 1 and 3 | ON-temperature learning; fixed radiator/underfloor bases | Agrees with the pinned implementation. Holding the learned ON temperature is intentional, not silently replaced with OFF temperature. |
| Entries 5, 7, 9, 11, 12 and 17 | PWM OFF; cold threshold; CH decision and reset | Agrees with the port's recorded intent, while retaining the distinction between exact words and interpretation. The `<=` boundary remains Q20. |
| Entries 20, 21, 22 and 23 | Suppression; MM source exceptions; entry into PWM; startup hold | Agrees with the recorded defaults and exceptions. The assumption that these requests achieve minimum physical output still requires equipment evidence. |
| Entries 24, 28 and 30 | Curve, custom PID, source versus emitter type | Agrees. This research does not retune the curve, restore integration outside the deadband, or conflate heat source and emitter. |
| Entries 31, 40, 42, 43, 44, 50 and 52 | Timing, minimum ON, PID/command cadence, source periods | Agrees with the decided settings; distinguishes commanded phase duration from actual uninterrupted flame/compressor runtime. |
| Entries 45, 48, 51 and 54 | Missing-return ignition and MaxTSet | Reports unresolved ignition and protocol-mapping issues; does not silently implement a competing interpretation. |
| Entries 58 and 59 | PWM-phase starts versus unrelated ignitions | Agrees that unrelated DHW/relight edges are not counted as PWM starts. The point at which a phase is counted remains Q49. |
| Entries 64, 65 and 66 | Different upstream PWM algorithms and retained pin | Preserves the maintainer's retained pin. The Git ancestry interpretation in entry 66 is contradicted by the fresh API evidence below; the original statement and record are left intact. |

The control literature is a comparator, not a new SAT authority. No recorded SAT decision establishes a benefit from actuator-tracking anti-windup, bumpless state matching, or a predictive controller; those are research candidates that require their own decision and evidence.

## 1. The released pin and `develop` are different algorithms; `develop` is not newer in the verified Git graph

The following was read from the public GitHub API at approximately 00:08 CET on 2026-10-28, using fixed commit IDs in both comparison directions:

| Item | Verified result |
|---|---|
| Release pin | `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, committer date `2026-03-18T20:23:38Z` |
| Recorded develop snapshot | `7d43560342ca9d854e92b8b5a91bdfbf9cbc2152`, committer date `2026-03-01T14:10:06Z` |
| `pin...develop` | `status=behind`, `ahead_by=0`, `behind_by=294`; merge base is the develop snapshot |
| `develop...pin` | `status=ahead`, `ahead_by=294`, `behind_by=0`; same merge base |
| Branch heads at this check | `develop` and `releases/thermo-nova` resolved to those same respective SHAs |

The develop snapshot is an **ancestor** of the release pin; the pin is 294 commits ahead. The comparison does not establish that every algorithmic choice in the pin is better, but it refutes a Git-history claim that this develop snapshot is a newer descendant. Evidence: [fixed comparison from pin to develop](https://api.github.com/repos/Alexwijn/SAT/compare/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb...7d43560342ca9d854e92b8b5a91bdfbf9cbc2152), [reverse comparison](https://api.github.com/repos/Alexwijn/SAT/compare/7d43560342ca9d854e92b8b5a91bdfbf9cbc2152...7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb).

At that fixed develop snapshot, `pwm.py:125-132` updates its stored boiler temperature with an EMA during startup and live flow afterward while the PWM state is ON; `pwm.py:139-142` forces a target of flow +10 C when flame is inactive and average burn length is missing or under 60 s. The code was retrieved and opened at the fixed SHA: [develop PWM source](https://github.com/Alexwijn/SAT/blob/7d43560342ca9d854e92b8b5a91bdfbf9cbc2152/custom_components/sat/pwm.py#L124-L144). This is +10 C, not multiplication by ten.

The release pin instead uses a learned effective ON temperature for duty and return-plus-offset for pre-ignition CS (`TN pwm.py:130-149,229-239`; `TN heating_control.py:392-408`). Those differences are real. Calling the develop snapshot a newer release or automatically importing it is not justified. Entries 64 and 65 retain the pin; entry 66 records a later explanation and an open maintainer question. A factual clarification should be appended through the project's decision-record process, not by rewriting the original statement.

## 2. Trace the outer controller before judging the PWM

### 2.1 Weather compensation provides the baseline water temperature

The actual pinned formula is:

```text
x = outsideTemperature - 20
H = 4 * (targetRoomTemperature - 20) + 0.03 * x^2 - 0.4 * x
curve = round(base + coefficient / 4 * H, 1)
```

`TN heating_curve.py:28-39`; bases are 20 C for `UNDERFLOOR`, 27.2 C otherwise (`TN types.py:110-118`). The pin places `HEAT_PUMP` on this same enumeration and therefore gives it 27.2 C. The firmware's separate heat-source and emitter axes follow entry 30; that deliberate port departure is not a rounding error.

For target 20 C, coefficient 2.0 and radiator base 27.2 C, direct evaluation gives:

| Outside C | 20 | 15 | 10 | 5 | 0 | -10 |
|---|---:|---:|---:|---:|---:|---:|
| Curve C | 27.2 | 28.6 | 30.7 | 33.6 | 37.2 | 46.7 |

These are arithmetic results, not measured suitable temperatures for a particular building. Increasing target by 1 C raises the unrounded curve by `coefficient` C; with coefficient 2.0 that is a 2 C water-temperature increase. In automatic-gain mode, coefficient changes also change PID gains, so they change both feedforward and feedback strength (`TN pid.py:70-93`). Manual Kp/Ki/Kd do not follow that formula (`:64-65,76-77,87-88`). A field comparison must hold the coefficient and gain mode constant, or explicitly account for both effects.

The polynomial is empirical. It does not contain a measured building heat-loss coefficient, radiator area, water flow or thermal capacitance (`TN heating_curve.py:28-39`). Its convex shape is explicitly intended by entry 24. Whether it is a good fit is an installation question, not something established by the curve's name. Algebraically its outdoor slope changes sign above about 26.7 C; consequently its useful heating-season domain and separate summer-off policy should be stated rather than extrapolating the curve to arbitrary hot-weather inputs. That is a domain check, not a claim that the whole controller necessarily heats in summer; entry 32 retains a separate summer feature.

### 2.2 The room correction is intentionally not a conventional continuously integrating PID

Error is `round(target - measuredRoom, 3)` (`TN temperature/state.py:15-18`). The output is `round(curve + P + I + D, 1)` (`TN pid.py:127-132`). With automatic gains:

```text
Kp = round(coefficient * curve / divisor, 6)
divisor = 4 for UNDERFLOOR, otherwise 3
Ki = round(Kp / 8400, 6)
Kd = round(0.07 * 8400 * Kp, 6)
P = round(Kp * error, 3)
D = round(Kd * storedRawDerivative, 3)
```

`TN pid.py:62-114`. Automatic gains are the pin's default (`TN const.py:120`); manual mode passes through the configured Kp/Ki/Kd instead (`TN pid.py:64-65,76-77,87-88`). In engineering terms, P, I and D are corrections in water-temperature degrees. Kp maps room-temperature error to water-temperature correction; Ki has an additional inverse-second dimension, and Kd an additional second dimension when the raw derivative is in room-temperature degrees per second. The automatic formulas are empirical gain scheduling; they are not an identified model of building inertia.

The two deadband regions are materially different (`TN const.py:16`; `TN pid.py:185-230`):

| Condition | Integral | Stored raw derivative |
|---|---|---|
| `abs(error) <= 0.1 C` | Add `Ki * error * 60`, clamp to `[-curve,+curve]` | Keep the previous value; update derivative timestamp |
| `abs(error) > 0.1 C` | Reset to zero | Recalculate from negative measured-temperature slope when its other gates permit |
| Solar compensation active and integral-freeze option on | Skip the entire integral update, including its reset | Follow the normal derivative path |

The integral is a slow bias compensator around the target, consistent with entry 28. Outside the band, weather compensation and P/D must remove the error without accumulated I. This can be a coherent design choice; interpreting it as accidentally reversed integral logic would undo an explicit decision.

The derivative calculation uses sensor `last_changed`, skips `deltaTime <= 60 s`, caps raw slope magnitude at 5, and otherwise filters with `alpha = deltaTime / (60 + deltaTime)` (`TN pid.py:199-232`). `PID.update()` still overwrites its last room temperature on every update (`:170-171`). Thus derivative reference and time advancement require careful parity tests, not a generic formula copied from a textbook.

**Frozen raw derivative is not always a constant D correction.** `storedRawDerivative` is held inside the band, but `Kd` is a property recomputed from current curve/coefficient; the weighted D can therefore change while raw derivative stays fixed (`TN pid.py:85-114,205-207`). The displayed `raw_derivative` property rounds to three decimals, while `derivative` uses the internal raw value before that display rounding (`:109-119`). A display of zero raw derivative does not establish an absent D correction.

For example, curve 35 C and coefficient 2 on radiators give approximately Kp=23.333333, Ki=0.002778 and Kd=13720. A room warming at 0.6 C/hour corresponds to raw slope about -0.0001667 C/s and D about -2.287 C, before later deadband holding. At error -0.05 C, one correct 60 s integral step is about -0.008334 C; holding the assumptions for an hour gives about -0.50004 C. These are calculations from the formula, not observed room dynamics.

### 2.3 Two important nonlinearities deserve controlled experiments

First, a stored integral disappears at the deadband boundary. For a controller holding I=-4 C, a small error crossing from +0.099 to +0.101 C resets that -4 C correction; before other changes, requested water temperature rises by roughly 4 C. The source establishes the discontinuity (`TN pid.py:185-192`). Whether it creates noticeable cycling depends on sensor noise, filtering, room dynamics, P/D, and output limits. The recommendation is a boundary sweep with measured room disturbance and all terms logged, not an unapproved smoothing change to the PID.

Second, requested output is clamped after PID and room aggregation (`TN climate.py:370-394`), but PID receives temperature state and an optional solar freeze, not the final actuator saturation or PWM duty (`TN pid.py:159-171`; `TN climate.py:499-502`). Its symmetric integral bound prevents unlimited accumulation. It does not by itself provide feedback about what an inhibited boiler or a max-limited heat source actually delivered. That distinction matters when comparing it with actuator-aware anti-windup.

### 2.4 Timing is part of the algorithm

The pin schedules PID every 60 s and heating control every 5 s; scheduling debounces a heating run by 1 s (`TN climate.py:515-531,706-715`; `TN pid.py:20`). Entry 42 chooses that 60 s PID cadence and entry 44 chooses command output on every heating update. Entry 23 separately preserves the port's faster approximately 3 s suppression refresh.

At the reviewed firmware HEAD, `SATpid.ino:190` adds a full 60 s integral step per actual update, while `:301-307` permits an update sooner when error changes; the control tick defaults to 30 s (`SATtypes.h:469`; call `SATcontrol.ino:5297`). For varying in-band error and no solar freeze, that permits approximately twice the intended integration rate at 30 s, up to six times at 10 s. With unchanged error the gate holds it to 60 s. This was independently checked against the pin during this review. It is a port discrepancy against an already decided cadence, not evidence that the intended SAT integral formula is intrinsically too aggressive.

## 3. PWM converts requested water temperature into time, then separately controls the burner

### 3.1 Duty calculation and its physical interpretation

The pinned formula is:

```text
BT = effectiveOnTemperature if one is learned, otherwise currentFlowTemperature
BT = max(BT, base + 1)
duty = clamp((requestedSetpoint - base) / (BT - base), 0, 1)
```

`TN pwm.py:229-239`. It receives the requested setpoint after the room/area/solar calculation, not the suppression CS transmitted to the boiler (`TN climate.py:370-394`; `TN heating_control.py:211-225`). This agrees with entries 1 and 8.

The simplest model that would justify this ratio assumes useful emitter output is proportional to water temperature above the fixed base, OFF output is negligible in the averaging interval, and the learned ON value represents repeatable ON output. Under those assumptions the time fraction can reproduce the desired average output. These assumptions are an analysis model; they are not equations measured by SAT.

Actual water-side heat transfer obeys `heatRate = massFlow * waterSpecificHeat * (flow - return)`. Emitter output also depends on the temperature difference to the room and stored heat. Two installations can have the same requested temperature and flow temperature but different return temperatures, pump flows, valve positions and useful output. SAT's duty equation cannot distinguish those states because those variables do not enter it (`TN pwm.py:229-239`). This proves an observability limitation; it does not prove the room loop cannot compensate over time.

Do not equate a 25% temperature-derived duty with a measured 25% of required building heating power. A credible efficiency assessment must measure actual flame/compressor operation, startup delay, room error, and energy input or delivered heat.

### 3.2 Learning the denominator: a plausible approximation with a specific adaptation limit

The effective ON temperature is seeded from live flow at the first flame, at an ON start with flame already active, and when a waiting ON phase detects ignition (`TN pwm.py:130-131,166-167,187-191`). While ON with flame, it receives EMA updates `0.3*newFlow + 0.7*oldEffective` during the first 180 s after the stored ignition timestamp (`:144-149`; `TN const.py:18`). It is held afterward. Duty is calculated before the current update's EMA sample (`:134-149`). Entry 1 explicitly approved this released behaviour after the earlier 30 s claim was corrected.

Holding an ON estimate through OFF is defensible: a cooled flow sensor during OFF is not the temperature at which the next minimum-output burn emits heat. Substituting that cooled value may shrink the denominator and inflate calculated duty. Conversely, a held estimate may no longer represent output after TRV/pump/hydraulic changes during a long phase. Both are hypotheses to test with measured output. Neither proves that replacing it with raw OFF temperature is an improvement.

The fixed 0.3 EMA is per update, rather than explicitly time-normalized (`TN pwm.py:149`). Its effective smoothing speed therefore depends on update spacing; for a constant spacing `h`, its equivalent exponential time constant is `-h / ln(0.7)`. At 5 s that is about 14 s; at 30 s about 84 s. This is arithmetic explaining why sample cadence must accompany temperature-filter comparisons. It does not establish that every coordinator event produces a PWM update at those spacings.

### 3.3 Five ranges impose minimum pulse lengths and an ultra-low dead zone

Let `P = 3600 / cyclesPerHour`, `m = 180 s`, `a = m/P`. The source defines lower duty `a`, upper `1-a`, minimum `a/2`, maximum `1-a/2` (`TN pwm.py:52-61`). Its duration mapping is (`:247-301`):

| Range | ON duration | OFF duration |
|---|---:|---:|
| `duty < a/2`, no flame or DHW active | 0 | `2P` |
| Same ultra-low range, flame active and no DHW | `m` | `2P-m` |
| `a/2 <= duty <= a` | `m` | `m/duty-m` |
| `a < duty <= 1-a` | `P*duty` | `P*(1-duty)` |
| `1-a < duty <= 1-a/2` | `m/(1-duty)-m` | `m` |
| Above maximum | `2P` | 0 |

Integer conversion can shorten a reported duration by a fraction of a second. At 3 cycles/hour, `P=1200`, minimum duty=7.5%, lower=15%, upper=85%, maximum=92.5%. With effective ON=45 C, base=27.2 C, and request=36.1 C, duty is 50% and the nominal mapping is 600 s ON / 600 s OFF. With request=28.268 C, duty is 6%; with no flame the mapping is zero ON / 2400 s OFF.

Crucially, the latter does not guarantee a pulse after 2400 s. The pin always requires calculated ON >=180 s before starting (`TN pwm.py:154-155`); at an unchanged 6% duty with no flame it remains OFF. An execution of the actual upstream PWM class, with imports replaced by dependency stubs only, remained OFF at t=0, 2400, 4800 and 7200 s in this scenario. This is a host logic result, not a boiler test. Entry 31 explicitly describes ultra-low operation as staying off.

The port currently lacks that normal-start gate (`SATcontrol.ino:998-1000,1057-1076`). For the example above, requested 28.268 C remains above its radiator cold threshold 28.2 C; after the OFF wait and with hourly allowance available it can request ON despite calculated ON=0. `satPwmBeginOn` sets the phase (`:867-883`), the ignition branch supplies a heating CS (`:1114-1116`), and `SATchEnable.h:49-51` permits CH=1. Both versions and all these predicates were independently opened by a second reader. This is the concrete Q38 port discrepancy; actual ignition still depends on the boiler and subsequent scheduling.

### 3.4 Startup and suppression timeline

The following describes the pin with overshoot protection **enabled**, demand above its cold threshold, and no DHW:

1. Continuous mode normally operates first after reset (`TN pwm.py:87-109`; `TN heating_control.py:197-228`). It is not always literal CS=request: continuous control can hold a CS close to active boiler temperature (`:450-508`).
2. Runtime switching enables PWM after flow remains >= requested+3 C for 60 s; DHW and its 300 s guard suppress this (`TN heating_control.py:262-305`; `TN cycles/const.py:10`; `TN const.py:19,21`). Entry 22 deliberately selects 2 C for the port. The pin also enables/disables PWM from completed-cycle classifications (`TN pwm.py:219-227`).
3. IDLE starts ON immediately only if mapped ON>=180 s, subject to its counter check (`TN pwm.py:154-177`). With flame absent it waits; detected ignition resets the phase timestamp and denominator (`:185-193`).
4. Before flame, transmitted CS is return+offset, clamped from 22 C to the maximum; default offset 18 C (`TN heating_control.py:392-408`; `TN const.py:151`).
5. After detected ignition, keep the hold CS for the configured delay, default 20 s (`TN heating_control.py:410-433`; `TN const.py:152`).
6. After the delay, transmit live flow minus the configurable suppression offset, default 1 C (`TN heating_control.py:435-448`; `TN const.py:153`). Independently, duty continues to use requested temperature and learned ON temperature.
7. Scheduled OFF transmits CS=10 (`TN heating_control.py:388-389`); CH follows final transmitted CS>22 (`:236-238`), so this gives CH=0. The serial coordinator maps that to the master CH-enable bit (`TN coordinator/serial/__init__.py:210-214`). MM is 0 while PWM enabled unless DHW is active, with Geminox floor 10 (`TN heating_control.py:134-145,510-523`). The port's source exceptions follow entry 21.

There are significant qualifications. `_compute_pwm_control_setpoint` returns before its OFF branch if overshoot protection is disabled (`TN heating_control.py:385`), and the pin's fresh default is false (`TN const.py:122`). Then scheduling alone does not establish a physical OFF command. The port's true default is deliberate entry 20. The pin's CH-from-transmitted-CS threshold differs from the approved port CH rule in entries 7/9/11/17; these are not interchangeable implementations.

### 3.5 A minimum requested phase is not a guarantee of minimum physical burn time

The pin protects the initial ignition anchor for 180 s (`TN pwm.py:185-207`). If the boiler extinguishes and relights inside that phase, its stored `_flame_on_at` is not restarted by that relight (`:144-149,185-207`). An actual-class host trace with a 180 s mapped ON time produced ON at t=0, flame absent at t=60, relit at t=120, and scheduled OFF at t=180: the second physical burn could be only 60 s.

This is not a claim that SAT caused the original extinction, nor that this exact sequence has occurred on hardware. It proves why entry 31's minimum-flame wording must be distinguished from what the state machine can enforce. A boiler may also extinguish independently in response to its own temperature or safety limits. No thermostat can guarantee continued combustion merely by requesting it.

The verified port additionally checks phase expiry using an older ignition anchor before its ignition-wait handling (`SATcontrol.ino:1024-1037`); Q35 already records the delayed-ignition discrepancy. A correct implementation must be established before comparing intended pulse lengths with measured efficiency.

### 3.6 Mode changes and start counts need their own definitions

Runtime exit from PWM checks sustained underheat during ON for 180 s, or zero calculated OFF time for 300 s (`TN heating_control.py:307-376`; `TN const.py:22-23`; `TN cycles/const.py:11`). Completed-cycle UNDERHEAT also disables PWM; continuous OVERSHOOT enables it (`TN pwm.py:219-227`). These classifications use water/setpoint error percentiles, not measured gas savings or complete room-comfort performance (`TN cycles/classifier.py:39-88`). A label GOOD is not an efficiency result.

The pin increments its count at a PWM ON-phase start (`TN pwm.py:154-160`), resets the count in an anchored one-hour window (`:138-141`), and compares it with `duty_cycle_seconds` (`:156`), making it ineffective as a normal small cycles/hour cap. The source comment calls the window rolling, but it is not a sliding timestamp window. Entry 59 deliberately specifies a real limit on PWM-phase starts, excluding unrelated flame edges. The port implements a timestamp ring (`SATcycles.ino:285-325`) and currently counts a phase at its first flame (`SATcontrol.ino:851-883`); Q49 records that remaining policy difference. Count definitions must be stated before comparing start totals.

## 4. What established control patterns add to the review

These are narrow comparisons, not whole-system replacements. The cited online pages were read on 2026-10-28; each source supports the limited description below, not the SAT performance hypotheses.

| Primary source | Relevant pattern | What it helps evaluate; applicability limit |
|---|---|---|
| [MathWorks: anti-windup control](https://www.mathworks.com/help/simulink/slref/anti-windup-control-using-a-pid-controller.html) | Back-calculation, conditional integration and external tracking account for actuator saturation; cascaded actuators and feedforward need appropriate feedback signals. | SAT bounds I but does not track delivered output. Test prolonged max limits, ignition inhibition and recovery. This reference does not justify feeding suppression CS directly back as room demand: the two signals have different meanings. |
| [MathWorks: bumpless transfer](https://www.mathworks.com/help/simulink/slref/bumpless-control-transfer-between-manual-and-pid-control.html) | Matching controller state to the signal currently driving the actuator can reduce a control jump at handover. | Measure continuous/PWM and fallback handover jumps. The example is manual/PID switching, not an OpenTherm implementation; translating it to SAT requires a defined equivalent heat-demand signal. |
| [ESPHome: PID climate](https://esphome.io/components/climate/pid/) | A configurable PID exposes integral bounds, derivative/output averaging and different gain multipliers inside its deadband; update rate follows sensor updates. | A useful contrast with SAT's reset-I/hold-raw-D rule. Its gains and output scale cannot be copied into a water-temperature controller or compared numerically without normalization. |
| [ESPHome: slow PWM](https://esphome.io/components/output/slow_pwm/) | A duty command becomes a proportion of an explicit period; cycle restart on state change is configurable. | A clear baseline for requested ON fractions and timing semantics. The generic output component does not itself establish OpenTherm permission, ignition success or boiler anti-cycle behaviour. |
| [ESPHome: thermostat](https://esphome.io/components/climate/thermostat/) | Heating minimum-run and minimum-OFF actions are explicit constraints, separate from thermostat hysteresis. | Useful for specifying state-machine invariants and restart behaviour. An action timer still does not prove physical flame/compressor runtime or manufacturer suitability. |

The engineering inference is that SAT should be evaluated as a cascade: room correction -> requested water temperature -> duty/state supervisor -> OpenTherm messages -> independently controlled boiler -> emitters/building. A downstream refusal or cap is not necessarily a room-PID tuning problem. Conversely, a stable room trace alone does not prove clean burner operation.

An actuator-aware PI/PID is a reasonable **experimental comparator**, particularly when room bias remains under sustained output limits. It is not automatically better than SAT's narrow-band compensator, which deliberately prevents large accumulated error outside the band. A fair test first matches weather compensation, sensors, constraints and comfort targets; otherwise differences may come from those choices rather than anti-windup.

Similarly, separating minimum-run/minimum-OFF constraints from the requested duty could make behaviour easier to specify and test. That is an architectural research direction, not authorization to redesign SAT or substitute generic relay timing for a boiler's own controls.

## 5. OpenTherm boundaries that algorithm research must retain

The OpenTherm Application Functional Specification v1.1f is a black-box functional specification, not a mandate for a particular PID or duty equation (local markdown line 112, introduction). Its low-load proposal distinguishes starting a burner, controlling minimum output during ON, and clearing heat demand during OFF (§2.1.1.2.3-.6). It also discusses ignition/anti-cycle delay correction (§2.1.1.5). These are directly relevant to the state-machine proof above.

Its interoperability section is stronger: read ID3 at startup and honor HB bit 4, which can disallow master low-load control; Unknown-DataID has its explicitly stated fallback (§2.1.1.2.10). Neither a successful PID nor upstream parity can excuse ignoring that veto. ID3 HB1 identifies modulating/on-off control type; it is not the same permission bit. Evidence: local functional-spec markdown lines 128-150,227-243; protocol v4.2 `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md:1603-1611`.

With ID14 supported, the suggested minimum-output path uses an appropriate high setpoint and a 0% relative modulation cap; without it, live flow-following CS is the described workaround (§2.1.1.2.5-.6). Relative 0% requests minimum modulation, not zero physical heat. The port's heat-pump/hybrid and manufacturer exceptions are recorded in entry 21 and must be retained unless explicitly revised on equipment evidence.

Entry 45's proposed no-return-temperature path starts directly with flow-based suppression. The specification's example instead raises the request until flame appears. This is a conflict to investigate rather than proof that one pulse pattern will ignite every boiler. Faster CS/PM commands must be assessed on the actual bus with sample age, queue delay and flame response; sending more commands does not prove fresher measurements or ignition.

This document leaves the full clause-by-clause compliance assessment and remediation sequencing to the companion low-load review and verification plan. It does not reinterpret advisory algorithm details as mandatory numeric requirements.

## 6. Recommendation: a staged comparison that can refute our preferred explanation

1. **Establish the decided baseline.** Fix/verify the known phase-start gate, ignition anchor, PID cadence and command-cadence discrepancies under their existing decisions and required approval process. Add the mandatory ID3 permission contract. Keep the release pin and each recorded source/emitter exception explicit. Comparing different algorithms while these discrepancies remain would be inconclusive.
2. **Prove delivered behaviour.** Capture timestamped master CH enable, slave CH mode, flame, request, final CS, actual bus ID1, MM request/acknowledgement, flow age and optional return/modulation. Separate PWM phase starts, physical CH ignitions, DHW ignitions and relights. Record actual ignition and minimum-OFF delays. The criterion is what the boiler did, not what its graph label appeared to say.
3. **Stress the specific approximations.** Sweep the 0.1 C error boundary, 7.5% ultra-low threshold at 3 cycles/hour, a change in valve/pump condition after the 180 s learning window, missing/stale return temperature, and output-ceiling recovery. Use safe bench/simulation tests first; approved hardware trials then determine applicability.
4. **Compare one change at a time.** Against the corrected pinned SAT, compare simple weather compensation with modest room correction, then an actuator-aware PI/PID or explicitly constrained duty supervisor if justified. Hold sensors, schedules, hydraulic settings and comfort objectives constant. Do not silently combine a branch change, a new curve, a new PID and new cycle limits.
5. **Measure the trade-off.** Report room error distribution and recovery time, actual CH starts, burn/stop durations, missed/late ignitions, flow/return behaviour, and energy input or delivered heat normalized for conditions. An improvement is not established by fewer starts while comfort deteriorates, or by tighter temperature control while energy use rises.

For a modulating boiler, first test whether an accurately applied low water request and its own modulation can meet demand continuously; cycling research becomes relevant below its useful minimum output or where the slave permits/requires master low-load control. For a heat pump, do not infer that gas-burner-friendly timing is compressor-friendly merely because the same OpenTherm messages are accepted. Device-specific minimum flow, defrost, allowed cycling and buffer/emitter dynamics require manufacturer evidence and a separate applicability decision. The companion comparison of manufacturer controls addresses those constraints.

Predictive control can be investigated only after the baseline and measurements exist. Its potential benefit depends on a usable building/plant model, forecasts, constraints and the objective being optimized. Nothing in this research establishes that added model complexity is necessary to fix SAT's current port discrepancies, or that predictive control will always beat a well-commissioned weather-compensated controller.

## Evidence register and reproducibility limits

Reproduce the duration table and the ultra-low/relight source traces from the repository root:

```powershell
python scripts/inspect_sat_reference.py --output "$env:TEMP/sat-reference.json"
```

The [reference adapter](../../scripts/inspect_sat_reference.py) executes the unchanged pinned PWM class after removing module-level imports and supplying Home Assistant/type/config/clock dependencies. It reads the startup constant from the actual pinned `const.py`, checks the pin marker, records source hashes and returns nonzero if the selected traces disagree. It is not a Home Assistant integration test, a complete upstream test suite or a physical boiler model. The separate port verification runner does not reproduce these upstream scenarios by itself.

| Evidence | Location / source | What it establishes |
|---|---|---|
| Fixed SAT source | [release source tree](https://github.com/Alexwijn/SAT/tree/7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb); local `UPSTREAM_COMMIT` | Identity of the algorithm reviewed; source references above are at this pin. |
| Curve and gain equations | `TN heating_curve.py:28-39`, `types.py:110-118`, `pid.py:62-132` | Actual calculations and rounding; numerical examples here are arithmetic. |
| PID state rules | `TN pid.py:159-232`, `temperature/state.py:15-18`, `helpers.py:140-150` | Integral reset/clamp, stored derivative and timing behaviour. |
| Requested versus transmitted output | `TN climate.py:370-394`; `heating_control.py:197-240,380-523` | Why suppression CS cannot be treated as requested room heating temperature. |
| PWM range and phase logic | `TN pwm.py:45-61,87-109,117-301` | Duration mapping, denominator estimator, ignition synchronization and counter semantics. |
| Independent port comparison | `SATcontrol.ino:867-883,998-1000,1024-1076,1114-1116,5297`; `SATpid.ino:190,301-307`; `SATchEnable.h:49-51` | Ultra-low start-gate and PID-cadence discrepancies checked by two readers in this review. |
| Fixed ancestry check | GitHub API comparisons linked in section 1 | Develop snapshot is an ancestor of the pin; does not decide algorithm merit or change the pin. |
| Host executions | Actual `PWM` class executed with imports removed and dependency types supplied; scenarios in sections 3.3 and 3.5 | Deterministic Python state transitions only. No physical thermal/combustion model was substituted as hardware evidence. |
| External control patterns | Five primary implementation-documentation sources in section 4 | Narrow comparators; no direct field proof about SAT or any particular boiler. |
| Decision provenance | Corrections entries and ADR status table above | Intent, exceptions and open choices; neither a decision nor a task status proves implementation. |

No boiler was operated for this research, no new firmware was flashed, and no A/B energy comparison was performed. The review proves source-level mechanisms and counterexamples, identifies approximations to test, and proposes a reviewable experiment sequence. It does not certify OpenTherm compliance or a universal improvement to the SAT algorithm.
