# SAT transport delay and budgets (TASK-1412.5 AC3)

Status: **measurement plus proposal for review**. The budgets below are a proposal, not a decision. A change to the
flow freshness limit N goes back to Sergeant D (entries 218, 241). Nothing here changes firmware.

AC3 of TASK-1412.5: "Timestamp traces measure source sample age plus scheduler/queue/wire delay against reviewed
transport budgets; infeasible fallback is rejected for that profile."

Tool: `scripts/sat_transport_delay_1412_5.py` (stdlib only, aggregates only, safe on private captures).

## 1. Summary

- **PIC, flow feedback.** The flow value SAT uses at a tick is 3.2 s old at p50 and 9.8 s old at the worst tick.
  That is 1527 ticks over 12.8 h, with no tick at 30 s or more. Entry 241's limit N = 60 s has a margin of 6x
  over the worst tick seen.
- **PIC, command path.** From SAT's CS decision to the boiler's Write-Ack with the new value takes p50 3.9 s,
  p95 5.1 s and at most 11.5 s. Queue and PIC echo take less than 0.1 s of that. The rest is the wait for the next
  MsgID 1 frame, which the thermostat sends about every 5 s.
- **OT-Direct.** No capture with a real boiler exists. All OT-Direct figures here are code-derived, not measured.
- **Fallbacks to reject** (section 6):
  - PWM low-load control on a PIC in PS=1 summary mode.
  - The PIC board-sensor return on OT-Direct.
  - Link liveness or gateway-made frames counted as flow freshness.
  - Any N shorter than the transport worst case of the installation.

## 2. Captures used

All timestamps are on the device clock: the otframes.log `device_time` and the telnet line prefix. These are the
same clock: the telnet `processOT` line of a frame is 2 ms after its otframes.log entry. Host time is not used.

| Label | Transport | Firmware | Length | What it can measure |
|---|---|---|---|---|
| F1104 | PIC (gateway, thermostat present), Intergas, field | 2.0.0-beta.68 | 12.8 h | Everything on PIC: MsgID 25 supply, age at tick (with firmware `fage`), full CS chain. Primary source. |
| F1030 | PIC, same unit, field | 2.0.0-alpha.549 | 3.9 h | Cross-check. It has no otframes.log, so frames come from telnet `processOT`. Ticks are the older `SAT loop:` line, with no `fage`. Its queue is the old fixed 1 s tick (before TASK-1571). |
| B63 | PIC bench, Classic + S3, no boiler | 2.0.0-beta.79 | 6 min | Only the time from queue insert to queue pick. `SATsimulation=true` keeps CS off the serial line, the OT bus is offline, and the unit rebooted during the capture. Its `fc=V fage=0` values are synthetic. |
| D61 | OT-Direct bench (OTGW32) | 2.0.0-alpha.570 | 6.0 h | **Excluded from the numbers.** SAT simulation is on, with "no bus traffic". The frame stream holds a constant MsgID 25 value with `/api/v2/simulate` polled all the way through, very probably a replay. SAT's CS changes never reach the bus (103 of 103). It only shows that the script runs on hourly OT-Direct segments. |

Earlier figure, not re-measured here (single source: the TASK-1412.5 notes): a field capture from the same unit on
2026-10-31 had MsgID 25 gaps of median 5.0 s, p95 6.9 s and max 40.8 s, one of them over 30 s. That capture is the
reason N moved from 30 s to 60 s (entry 241).

## 3. Timing model

The flow path has these segments, in time order. The last column says whether these logs can observe each one.

| # | Segment | Observable? |
|---|---|---|
| 1 | The boiler samples its flow sensor | No. The boiler does not say how old its value is. |
| 2 | Wait for the next MsgID 25 read (thermostat poll on PIC pass-through, scheduler on OT-Direct) | Yes: the interval between valid B/25 frames |
| 3 | Slave answer and wire: 34 bit periods per frame (about 34 ms each way); answer 20-400 ms, typically 100 ms or less (OT spec v4.2 §4.3.1) | No. It falls below the log resolution and inside segment 4 |
| 4 | PIC to ESP serial (about 11 ms at 9600 baud), serial task and queue to loop() | No. It is folded into the arrival time |
| 5 | **Delay buffer**: the firmware processes frame k when frame k+1 arrives (OTGW-Core.ino:5856-5867 at the worktree HEAD). The SAT feed stamp is taken then (OTGW-Core.ino:6085-6090). | Yes: true arrival of k = device time on the line of frame k-1 |
| 6 | Wait until the next SAT tick (30 s, `iControlInterval`, SATtypes.h:494) | Yes: age at tick |

The script reports two ages per tick:

- `age_bus` = tick minus the true bus arrival.
- `age_esp` = tick minus the processing time, which is what the firmware's `fage` measures.

Their difference is segment 5.

For the command path (PIC) the stages are, in time order:

1. SAT inserts `CS=` into the queue (`addCommandTo ... Insert`). This line comes 10-15 ms before the SATTICK line.
2. `Sending to Serial`.
3. The PIC echo `CS: x` (checkCommand).
4. The first frame to the boiler with MsgID 1 equal to the new value. This is R when the PIC substitutes, or T when
   the thermostat already writes that value. The script uses its true arrival.
5. The boiler's B/01 Write-Ack with that value.

The script measures the wire stages only for CS value changes, because the 5 s repeats (SAT_CMD_RESEND_MS,
SATcontrol.ino:582) carry the same value. A change counts as `superseded_before_bus` only when a newer CS reached
the PIC before the first boiler-bound MsgID 1 frame after this change's echo. A change that missed a MsgID 1 slot
and was replaced later counts as `not_delivered`. A frame that completes within 0.1 s after the next send still
counts for the older value, because it was already on the wire.

## 4. Measured numbers

### 4.1 MsgID 25 supply (PIC, thermostat polls)

| | F1104 (beta.68) | F1030 (alpha.549) |
|---|---|---|
| Valid B/25 Read-Acks | 8939 (no Data-Invalid, no Unknown-DataID) | 2765 |
| Interval p50 / p95 / p99 / max | 5.01 / 6.01 / 8.15 / 13.0 s | 4.98 / 6.12 / 8.96 / 12.1 s |
| Intervals > 30 s / > 60 s | 0 / 0 (1 interval across a capture gap skipped) | 0 / 0 |
| Bus arrival to firmware processing (delay buffer) p50 / p95 / max | 0.877 / 0.909 / 1.151 s | 0.871 / 0.904 / 1.164 s |
| Thermostat MsgID 1 interval p50 / p95 / max | 5.01 / 6.01 / 12.0 s | 4.98 / 6.01 / 12.0 s |

Cross-check on F1104: the frame file and the independent telnet stream both hold 8939 B/25 frames, so the telnet
stream lost no frames.

### 4.2 Flow age at the SAT tick (PIC)

| | F1104 | F1030 |
|---|---|---|
| Ticks (interval p50 / p99 / max) | 1527 (30.0 / 30.35 / 60.0 s) | 447 (30.0 / 30.01 s; one 750 s gap in the telnet stream) |
| `age_bus` p50 / p95 / p99 / max | 3.17 / 5.74 / 7.12 / 9.84 s | 3.77 / 5.72 / 6.57 / 7.38 s |
| `age_esp` p50 / p95 / p99 / max | 2.29 / 4.86 / 6.24 / 8.98 s | 2.83 / 4.85 / 5.72 / 6.48 s |
| Ticks with age >= 30 s / >= 60 s | 0 / 0 | 0 / 0 |
| Firmware `fage` p50 / max | 2.28 / 8.97 s | not logged in alpha.549 |
| `fage` minus recomputed `age_esp` | -2 ms to +1 ms over all 1527 ticks | n/a |
| Flow class at tick | V on all 1527 | n/a |

The firmware's own `fage` and the age recomputed from the frame file agree within 2 ms. Those are two independent
paths: the feed stamp in SATboilerFeed.h, and the frame log plus the delay-buffer model. `fage` reads about 0.9 s
younger than the true bus age, which is the delay buffer (segment 5).

### 4.3 CS command path (PIC)

| Stage | F1104 (beta.68, echo-paced queue) | F1030 (alpha.549, fixed 1 s queue tick) |
|---|---|---|
| All CS sends: insert to `Sending to Serial` p50 / p99 / max | 0.004 / 0.036 / 1.313 s (n 9856) | 0.426 / 0.989 / 1.001 s (n 747) |
| All CS sends: serial to PIC echo p50 / p99 / max | 0.022 / 0.034 / 0.356 s (n 9853, no timeouts) | 0.023 / 0.034 / 0.079 s (n 747) |
| CS value changes | 1364: 1327 measured, 37 superseded (3 of them reverts), 0 not delivered | 366: 359 measured, 7 superseded, 0 not delivered |
| Echo to first boiler-bound MsgID 1 with the value, p50 / p95 / p99 / max | 3.71 / 4.96 / 6.91 / 11.3 s | 3.18 / 4.55 / 5.84 / 6.92 s |
| Decision (insert) to boiler request, p50 / p95 / max | 3.75 / 4.99 / 11.4 s | 3.61 / 4.90 / 7.73 s |
| Decision (insert) to boiler Write-Ack, p50 / p95 / p99 / max | 3.85 / 5.10 / 7.07 / 11.5 s | 3.72 / 5.02 / 6.50 / 7.88 s |

Second source for the echo: F1104's own `cmdq_echo_*` counters in device/info at the end of the capture show 13410
echoes, avg 22 ms, min 14 ms, max 357 ms, and 1 timeout (all commands, not CS only). This matches the 22 ms / 356 ms
measured from the telnet lines.

The wait for the next MsgID 1 slot is the dominant stage. In these captures every boiler-bound MsgID 1 frame (R
or T) came right after a thermostat T/01; the PIC never wrote MsgID 1 on its own. This is observed, not taken
from the PIC source (otgw-6.6 was not opened for it). The thermostat's MsgID 1 cadence (5 s here, 12 s at most)
therefore bounds this stage on this installation. A thermostat with a slower MsgID 1 cadence would move this
number directly.

Firmware observations from the superseded cases (F1104, beta.68). They are reported for the lead, not acted on:

- **CS changes between ticks.** Many CS changes are inserted between the 30 s ticks, in steps of 0.1-0.2 °C every
  1-6 s (for example 40.8, 40.9, 41.1, 41.3, 41.4, 41.6 within 24 s). The superseded cases inspected by hand were steps
  of such a ramp that the next step overtook before the next MsgID 1 slot. The code path that makes these steps was not
  traced here.
- **Three reverts.** In 3 cases the 5 s repeat inserted a CS 0.1 °C above the value of the last tick (for
  example 45.5 then 45.6, 34.5 then 34.6), and a CS with the tick's value followed about 1-1.4 s later (in the
  two cases inspected), before the next MsgID 1 slot. So
  the boiler never saw the 0.1 °C blip. The source is not traced. A rounding difference between the tick path and
  the repeat path (`satCapRepeatCS`, SATcontrol.ino:582-590) is one candidate.

The bench (B63) measures only the queue pick: p50 3 ms, max 46 ms over 45 inserts. There is no echo and no wire,
because the simulation keeps CS off the bus.

### 4.4 OT-Direct: code-derived, not measured

No capture has OT-Direct against a real boiler. From the code at the worktree HEAD:

- MsgID 25 is read every 10 s (`OT_TEMP_INTERVAL_MS`, OTDirect.ino:59, 148), in a round-robin over the schedule
  table (OTDirect.ino:1604-1650).
- Commands in the ring buffer go before the schedule (OTDirect.ino:1590-1598). A CS change should therefore be on the
  bus one transaction later, after the minimum message gap `otMinIntervalMs`. That gap defaults to 100 ms
  (OTDirecttypes.h:117), and SAT never sends MI=500 on OT-Direct (SATcontrol.ino:1823-1826, entry 141).
- Possible stretch (estimate):
  - Nominal demand with every table entry enabled is about 4.5 transactions/s: status every 0.8 s, 13 reads every
    10 s, up to 9 writes every 15 s, 81 slow polls every 60 s. Entries disabled after Unknown-DataID lower this.
  - One transaction is 34 ms + answer + 34 ms. With the spec's 400 ms maximum answer that is about 0.47 s
    (about 2 transactions/s). At a typical 100 ms answer it is about 0.17 s.
  - So on a slow-answering boiler the round-robin can stretch the MsgID 25 interval well beyond 10 s: about 20 s if
    every entry stays enabled.
  - This is an estimate from the table, not a measurement. Whether entries drop out on a real boiler is not traced.
- If the slave is itself a gateway (a heat-pump OT adapter, for example), the spec sets no maximum answer time
  (OT spec v4.2 §4.3.1-4.3.2). Its transport delay cannot be assumed and must be measured per installation.

## 5. Proposed budgets (for review)

These are **transport** budgets, used as acceptance and diagnostic limits for the bench and field steps
(TASK-1412.21, .23, .25). They are not new control thresholds. The control rule stays Sergeant D's:

- Flow invalid or older than N = 60 s counts as missing: PWM idle, timers reset (entries 218, 241).
- Return invalid or absent for 5 min counts as no return temperature (entry 217).

Every budget below was checked against entries 217, 218 and 241, and against entries 40, 50 and 52 (PWM timing,
used only in the reasoning). It agrees with all of them. None of the budgets changes N.

The budgets come in two kinds:

- **Acceptance budgets (p95/p99).** A bench or field run must meet them.
- **Diagnostic flags (max).** A single breach is reported and explained (lost answer, thermostat pause, capture
  gap); it does not fail the run by itself. The reference unit has already breached one flag once: the
  2026-10-31 capture had a 40.8 s MsgID 25 gap, which trips both max flags below. That is exactly the case the
  flags exist to surface. It stayed below N = 60 s, so the control rule was not triggered.

| Budget | Gas boiler (PIC) | Gas boiler (OT-Direct) | Heat pump | Hybrid |
|---|---|---|---|---|
| Flow `age_bus` at tick, p99 (acceptance) | <= 10 s (measured 7.1 s) | <= 15 s (10 s poll + 1 frame + margin; to measure) | <= 15 s, only after a per-installation measurement | as heat pump |
| Flow `age_bus` at tick, max (flag) | > 30 s flagged (one tick; measured 9.8 s; 2026-10-31 would trip it) | > 30 s flagged | > 30 s flagged | > 30 s flagged |
| MsgID 25 interval, max (flag) | > 30 s = N/2 flagged (measured 13.0 s; 2026-10-31: 40.8 s) | > 30 s flagged | > 30 s flagged | > 30 s flagged |
| MsgID 25 interval, p99 (acceptance) | <= 10 s (measured 8.2 / 9.0 s) | <= 15 s | <= 15 s after measurement | as heat pump |
| CS decision to boiler Write-Ack, p95 (acceptance) | <= 6 s (measured 5.1 s) | <= 2 s (command goes before the schedule; to measure) | <= 6 s, measured per installation | <= 6 s |
| CS decision to boiler Write-Ack, max (flag) | > 15 s flagged, half a tick (measured 11.5 s) | > 5 s flagged | > 15 s flagged | > 15 s flagged |
| Queue + echo (PIC), p99 (acceptance) | <= 0.1 s (measured 0.04 + 0.034 s) | n/a | as gas | as gas |

Reasoning:

- **Age limits relative to the tick.**
  - A value that is younger than one tick (30 s) at every tick means that every tick uses a sample no older than
    the previous tick.
  - N = 60 s is the control limit. The flag sits at half of it, so one lost or Data-Invalid answer on a healthy
    installation shows up as a flag well before the "missing" branch.
  - On the measured PIC unit the worst tick is 9.8 s, which leaves room both for thermostats that poll more slowly
    and for an extra lost answer.
- **Command limits relative to the actuator.**
  - LL-015 in the verification plan lists a minimum requested ON time of 180 s and heat-pump periods of 1800/2400 s.
    Entries 40 and 50 record those for heat pumps; entry 52 sets 3 cycles per hour (a 1200 s period) as the gas
    default. This document did not open thermo-nova for these values; they are taken from those records.
  - An 11.5 s worst-case command delay is about 6% of a 180 s ON phase, and the p95 of 5.1 s is about 3%. Against
    a 1200 s gas period the share is smaller still.
  - Half a tick (15 s) as the maximum keeps a CS change from landing after the next tick has already decided again.
  - On a heat pump the binding limits are the manufacturer's compressor rules (PH-PROFILE), not the transport.
- **Heat pump and hybrid have no data.**
  - No capture exists with either profile.
  - If their OT side is an adapter acting as a gateway, the spec sets no bound on its answer time (§4.3.2). This
    is not known for any installation yet.
  - Their budgets therefore apply only after PH-PROFILE has recorded the installation and a capture has measured its
    MsgID 25 interval and its MsgID 1 Write-Ack latency.
  - Until then the result is scoped to gas boilers on PIC, as the verification plan requires ("If no heat pump was
    tested, explicitly scope the result to the tested gas-boiler configuration").

## 6. Fallbacks rejected per profile and transport (AC3)

1. **PWM low-load control on a PIC in PS=1 summary mode: reject (all profiles).**
   - In PS=1 the PIC sends summary lines instead of frames.
   - According to the TASK-1412.5 notes (ADR-181), the summary does not stamp the feed, so the flow counts as missing
     60 s after start and PWM stays idle for good.
   - The continuous fallback then becomes the only mode, silently. Low-load operation needs PS=0.
   - This document did not re-trace that path; it relies on the TASK-1412.5 notes.
2. **PIC board-sensor return (`rsrc=BRD`, PR=E) on OT-Direct: reject.**
   - That source exists only on a PIC board (`picBoardTretValid()`, SATcontrol.ino:2526).
   - On OT-Direct a boiler without MsgID 28 has no return temperature, and entry 217's 5 min rule applies.
3. **Link liveness as flow freshness: reject (all).**
   - MsgID 0 that keeps arriving says nothing about MsgID 25 (LL-011, R09).
   - Already enforced: only a boiler Read-Ack of MsgID 25 stamps the feed (SATboilerFeed.h `satFeedEventForFrame`).
4. **Gateway-made or replayed frames as flow samples: reject (all).**
   - This covers the OT-Direct local answer, loopback, the master-mode cache and the `/simulate` replay.
   - Already enforced by the same function. The D61 capture shows why it matters: a replayed constant 37.0 at every
     tick would otherwise look like a valid sensor.
5. **N shorter than the transport worst case of the installation: reject.**
   - N = 30 s (entry 218) was infeasible on the 2026-10-31 capture, which had a 40.8 s gap. Entry 241 moved N to 60 s.
   - On OT-Direct with a slow or gateway slave the code-derived MsgID 25 interval can approach 20 s or more
     (section 4.4). N = 60 s then holds only about 3 polls, and that must be confirmed on the bench (TASK-1412.21)
     before OT-Direct counts as feasible for low-load.
6. **Not rejected, open: a PIC with a thermostat that does not read MsgID 25.**
   - On PIC pass-through the flow supply is entirely the thermostat's choice of polls.
   - A thermostat that never reads MsgID 25, or reads it more slowly than N, leaves SAT without flow, so it always
     falls back.
   - Whether the gateway should then ask for MsgID 25 itself (PR or PM) is a design question for Sergeant D. It is
     not decided here.

## 7. Not measured

- **OT-Direct against a real boiler.** Section 4.4 is code-derived. The D61 capture is a simulation or replay.
- **Heat pump and hybrid.** There is no capture of either.
- **Segments below the log resolution:**
  - the boiler's internal sample age (segment 1);
  - the wire time and slave answer time (segment 3);
  - the PIC to ESP serial transfer (segment 4).

  These sit inside the measured "arrival" or cannot be seen at all. The spec bounds segment 3 at about 0.47 s; a
  gateway slave has no bound.
- **A PIC without a thermostat** (the PIC as master). Its own MsgID 1 and MsgID 25 cadence was not captured.
- **The MsgID 25 cadence during the ignition PM=25 flood** (entry 45). It was not isolated in these captures.
- **Only one field installation.** Thermostat, boiler (Intergas) and firmware are each a single sample. The two runs
  of the same unit agree, but they do not show a second installation.

## 8. Reproduce

```
python scripts/sat_transport_delay_1412_5.py --label F1104 --otframes <run>/otframes.log --telnet <run>/transcript-....txt --json f1104.json
python scripts/sat_transport_delay_1412_5.py --label F1030 --telnet <run>/transcript-....txt
python scripts/sat_transport_delay_1412_5.py --label D61 --otframes <hour1>/otframes.log --telnet <hour1>/telnet.log --otframes <hour2>/otframes.log --telnet <hour2>/telnet.log ...
```

The output is aggregates only. Raw field captures stay outside the repository.

Related: TASK-1412.5, TASK-1412.21, TASK-1571 (echo-paced queue), LL-011/LL-011b in
`tests/test_sat_lowload_contract.py`, entries 217, 218 and 241 in `docs/sat/sergeant-d-corrections.md`,
`docs/sat/sat-low-load-verification-plan-2026-10-28.md` (LL-015, PH-PROFILE).
