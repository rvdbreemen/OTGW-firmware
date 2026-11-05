# SAT bench simulator: operator procedure (TASK-1412.20)

| | |
|---|---|
| Procedure version | **v1**, 2026-11-05 |
| Fixture generator | `scripts/sat_bench_scenarios_1412_20.py`, version `1412.20-v1` |
| Generator test | `tests/test_sat_bench_scenarios_1412_20.py` |
| Firmware read for this version | `dev` at `7e05aed7d` (code reading only; no bench run yet with this procedure) |
| Task | TASK-1412.20 (R41), epic TASK-1412; feeds TASK-1412.21 (execution) and TASK-1412.22 (acceptance) |

This procedure lets a second operator repeat the SAT bench scenarios on the two bench boards. It covers AC3 of TASK-1412.20. Section 8 is the calibration plan for AC1 and AC2. Those two ACs are **not** met by this document: AC1 needs a wire-level OpenTherm slave that the bench does not have yet (section 8.1).

A change to this procedure, the generator or the fixture layout raises the procedure version and the generator `VERSION`. A run report names both versions, the firmware version and githash, and the `sha256` of every fixture from `index.json`.

## 1. What the bench simulator is

The simulator is the firmware's own frame replay (`POST /api/v2/simulate/start`, ADR-209, TASK-1508). The replay reads `/otgw_simulation.log` from LittleFS and hands each line to the OpenTherm decoder at the recorded gap. SAT runs on the board as usual and reacts to the decoded state. The firmware holds every command back from the bus while the replay runs (`satSimulationBlocksBusTx()`, TASK-1580) and writes it to telnet as a trace line instead.

| Board | Transport | What runs | What the trace line shows |
|---|---|---|---|
| OTGW Classic + S3 Mini, **.63** (hostname `zaphod-classic-s3`, PIC 6.8 at last readout) | PIC | Replayed lines go to the decoder. The PIC keeps running and talking to its own bus, but the firmware discards what the PIC reports (docs/api/README.md, Simulation). | `OTGW-SIM trace: <cmd> (src=pic-serial)` |
| OTGW32, **.61** (hostname `marvin-otgw32`) | OT-Direct | Replayed lines go to the decoder. The OT master sends nothing on the bus. Loopback (`GW=L`) must be off. | `OTGW-SIM trace: MID=<id> VAL=<v> (src=otdirect-tx)` |

Modules move between carriers. Confirm the identity by readout at the start of each run (section 3), not from this table.

**Label every result "board-executed controller replay".** It is never PIC wire delivery, OT-Direct wire delivery, or boiler or compressor proof (handoff doc, `sat-low-load-agent-handoff-2026-10-28.md`). Keep PH-PIC, PH-OTD, PH-PROFILE and PH-FIELD distinct.

## 2. What a replayed line can and cannot reach

This table is the boundary of the simulator. It was found by reading the code at `7e05aed7d`. Scenario SB05 measures it on the bench (calibration point C4).

| Input SAT reads | Fed by a replayed line? | Code |
|---|---|---|
| MsgID 3 HB bit 4 permission (`slave_cfg_valid`, `slave_low_off_denied`) | **Yes** | `print_slavememberid()` calls `satNoteSlaveConfig()` for any boiler Read-Ack; it has no replay filter |
| MsgID 0 status bits: flame, CH, DHW, fault (`OTcurrentSystemState`) | **Yes** | same decode path as live frames |
| Whole-link liveness (`iBoilerLastSeenMs`, `sat/ot_link_lost`) | **Yes**, every `B` line | `processOT()` stamps it for every `B` frame |
| MsgID 1 (`TSet`), MsgID 18 (pressure): `setpoint_sync`, `pressure_health` | **Yes** | observed on the bench 2026-11-04/05, as reported to this task |
| MsgID 27 outside temperature (SAT's outside-temperature gate) | **Probably yes, verify** | the stamp at `OTGW-Core.ino:6076-6078` has no `bReplayed` check, while `docs/api/MQTT.md` (`sat/outside_temp_missing`) says "live Read-Ack only". This is open item R-3 |
| Flow MsgID 25 and return MsgID 28 feeds (validity, age, Data-Invalid, Unknown-DataID) | **No** | `satFeedEventForFrame()` (`SATboilerFeed.h`) returns `None` for a replayed frame; `test_only_a_boiler_reply_refreshes_the_flow` |
| MsgID 57 write verdict (`sh_rejected`) | **No** | `satShVerdictForFrame()` |
| The "does not implement" bitmap (Unknown-DataID verdicts, `/api/v2/otgw/ot-support`) | **No** | docs/api/README.md, Simulation |

Three consequences:

1. **Replay-only mode** (replay on, SAT simulation off). SAT reports the flow as missing for the whole run (`SATTICK ... fc=N fage=4294967295`). Entries 218 and 241 then keep PWM idle and send the continuous setpoint. A replay-only fixture cannot run a PWM phase.
2. **SAT simulation plus replay** (`SATsimulation=true` plus replay). SAT's control reads flow, return and flame from the model (`satGetFlowTemp()`, `satGetReturnTemp()` and `satIsFlameOn()` return `state.sat.fSim*` / `bSimFlameOn`). The replayed MsgID 25, 28 and 17 answers still update the decoded values and their MQTT topics, but SAT does not read them. The MsgID 0 CH-active bit that `SATcycles.ino:1020` reads still comes from the replay. The DHW bit is the replayed bit OR the model's (`SATcontrol.ino`, `dhwBitNow`). SAT simulation on its own never gets a valid MsgID 3 reply. By entry 215, PWM therefore stays refused in plain simulation, so a fixture has to supply the MsgID 3 reply (R-5; this is the reason `B40030004` was replayed on 2026-11-04/05).
3. **Wire-slave required.** The flow and return feed cases, the Unknown-DataID verdict and every reactive case need a real OpenTherm slave answering on the bus. Examples of reactive cases: a flame that follows SAT's own CH/CS command after a delay, and a boiler anti-cycle lockout. The bench has no such slave yet (section 8.1). The generator still writes these timelines, so that a future slave can play them, and their `expected_wire` outputs.

## 3. Equipment and identity record (start of every run)

Record into `run.md` in the run folder:

1. `curl -s http://<ip>/api/v2/device/info`: firmware version and githash, detected mode (PIC or OT-Direct), `picfwtype` and PIC version on .63, MAC, hostname. On a combo board a PIC that is present but not in use is not PIC-path evidence.
2. `curl -s http://<ip>/api/v2/simulate`: `active`, `source`, `timing`.
3. Generator version, the fixture's `sha256` from `index.json`, and the procedure version.
4. Whether a boiler or thermostat is wired (none for these scenarios). The replay is for a bench only: on a PIC board the PIC still drives its bus.

## 4. Setup

Run the steps in order, one HTTP request at a time (ADR-165). `<ip>` is the board. If an admin password is set, add `-u admin:<pw>`.

1. **Save the settings.** `curl -s http://<ip>/api/v2/settings > settings-before.json`. Check the size: the device can serve a truncated file.
2. **Generate the fixtures.** `python scripts/sat_bench_scenarios_1412_20.py --out build/sat-bench-1412-20`. Then confirm the `sha256` against the run plan.
3. **Upload the scenario fixture as `/otgw_simulation.log`.** The upload handler stores the file under the multipart filename, so copy the fixture to a file named `otgw_simulation.log` first:
   `cp build/sat-bench-1412-20/SB01-support.otgw_simulation.log /tmp/otgw_simulation.log`
   `curl -s -F "file=@/tmp/otgw_simulation.log" http://<ip>/upload`
   Expect a 303. A 507 means nothing was stored. Then `GET /api/v2/simulate` must show `"source":"file"`.
4. **Set the debug flags.** `POST /api/v2/debug/flags` with `{"name":"sat","value":true}`, then the same with `sattrace` (TASK-1561, ADR-206). Debug flags are transient.
5. **Set the scenario mode.** For satsim+replay, `POST /api/v2/settings {"name":"SATsimulation","value":true}`. The value is unquoted JSON `true`: a quoted `"1"` is ignored silently.
6. **SAT must be active before the replay starts.** It needs `SATenabled` true and, after a restart, a boiler reply and a fresh outside temperature (entries 121 and 126). Push the outside temperature over MQTT: `<top>/set/<id>/sat/outdoor_temp = 8.0`, at most 1 h before use. Then check `GET /api/v2/sat` for `"active":true`. Do not rely on the replayed MsgID 27 until R-3 is settled.
7. **Start the independent capture** (section 7): `python scripts/otgw_capture.py --device <ip> --broker <broker> --duration <s>`. It records telnet, MQTT, `otframes.log` and the device snapshots.
8. **Open the host action log** (section 7) and write a `capture-start` line.
9. **Start the replay.** `curl -s -X POST http://<ip>/api/v2/simulate/start`. Log `replay-start` immediately before the call and `replay-started` after the response.

## 5. Run

- Run the operator actions of the scenario at their `t_s` (from `<id>.expected.json`, `actions`). Each action gets a host action log line before and after.
- The replay plays the file and then **starts again at the top**. After the wrap the first line follows the fixed 750 ms interval, not its recorded time (ADR-209). Stop with `POST /api/v2/simulate/stop` at the end, or at the `t_s` the scenario names.
- The replay pauses while the network is down, during an OTA upload and during a PIC flash. A network fault (section 7) therefore also stops the stimulus.
- Do not run a second HTTP client against the board during a scenario. SB16 is the exception, with a deliberate and bounded burst.

## 6. Reset and rollback (after every scenario)

1. `POST /api/v2/simulate/stop`. `GET /api/v2/simulate` must show `"active":false`.
2. Delete the fixture: `curl -s "http://<ip>/api/listfiles?delete=/otgw_simulation.log"`. Expect `File deleted`. Then `GET /api/v2/simulate` must show `"source":"builtin"`.
3. Restore every setting the run changed (`SATsimulation`, `SATenabled`, ...) from `settings-before.json`, one POST at a time.
4. **Reboot** (`curl -s -X POST http://<ip>/ReBoot`). Replayed state lives in RAM and only a restart clears it: `slave_cfg_valid`, `slave_low_off_denied`, `ot_link_lost`, the decoded status bits and the debug flags.
5. **Rollback check.** Diff `GET /api/v2/settings` against `settings-before.json`; the only allowed difference is a changed counter. `GET /api/v2/sat` must show `slave_cfg_valid=false` until a real MsgID 3 reply arrives; with no boiler wired it stays false. The scenario is not complete until this check passes. Record its result in `run.md`.

Do not use a USB flash or a filesystem image to reset: it wipes `settings.ini` (CLAUDE.md, Test automation).

## 7. Observation and independent timestamps

| Source | What it gives | Clock |
|---|---|---|
| `otgw_capture.py` `otframes.log` | every decoded frame from the device's `/ws` | `host_time` (this computer) **and** `device_time` |
| `otgw_capture.py` `telnet.log` | `SATTICK`, `SAT:` narration (`PWM refused (path N): ...`), `OTGW-SIM trace` / `SAT-SIM trace` (held-back commands) | host receive time |
| `otgw_capture.py` `mqtt.log` | `sat/mode`, `sat/ot_link_lost`, `sat/pwm_state`, `<top>/value/<id>/flamestatus`, `slave_configuration` | host receive time |
| `otgw_capture.py` `snapshots.log` | `/api/v2/device/info`, `/api/v2/sat?detail=full` every 300 s (`slave_cfg_valid`, `pwm_veto_reason`, `pwm_veto_path`, `ot_link_lost`, `last_blocked_cmd` in satsim) | host time |
| Host action log `actions.log` | every operator action, before and after | host time |

The host clock is the independent clock: `device_time` comes from the device under test. Write the action log with the same clock as the capture, for example:

```bash
act() { printf '%s\t%s\n' "$(date +%Y-%m-%dT%H:%M:%S.%3N%z)" "$*" >> actions.log; }
act replay-start; curl -s -X POST http://<ip>/api/v2/simulate/start; act replay-started
```

**Filtering command traces.** The trace shows every command the replay holds back, not only SAT's. On .61 every master request is traced at the one emitter, the gateway's own polling included, at about one line per second. On .63 the firmware's own PIC commands (PR= polls, the PM chain) trace too. Judge SAT's commands from `CS=`, `CH=` and `MM=` lines on .63, and from `MID=1` (CS) and `MID=14` (MM) lines on .61. On .61 CH is bit 0 of the master status byte in the `MID=0` value (`VAL` & 256). `last_blocked_cmd` in REST is only filled in SAT simulation.

The capture's frame recorder drops event lines (`*`), so the "OTGW simulation enabled [replay active]" event is not in `otframes.log`. The `/ws` OT log also has no replay marker per frame. Prove that a frame is a replayed one from the action log (start/stop) and `GET /api/v2/simulate`, not from the frame itself (R-2).

## 8. Calibration plan (AC1, AC2)

### 8.1 AC1: transport identity

Replay cannot meet AC1 ("a real ESP32-S3 board drives a safe emulated OT slave/plant ... with actual transport identity verified"). The replay feeds the decoder and keeps everything off the bus. What replay **can** show, per run:

- PIC path: `device/info` reports PIC mode and `picfwtype`, and held-back commands trace with `src=pic-serial`.
- OT-Direct path: `device/info` reports OT-Direct, and held-back commands trace with `src=otdirect-tx`.

AC1 itself needs an OpenTherm slave on the boiler side of each board, with the replay **off** and SAT simulation **off**. Candidates (open item R-1, Robert's equipment and scope decision; none checked on hardware in this session):

| Option | What it is | For | Against |
|---|---|---|---|
| A. Library slave on a spare ESP32 | the vendored Ihor Melnyk OpenTherm library (`src/libraries/OpenTherm`, `OpenTherm(in, out, isSlave=true)`, `buildResponse()`) plus an OpenTherm slave-side interface | open source and already in the tree; can play the same timelines and the reactive rules in `expected_wire` | needs interface hardware and a small sketch; its own timing must be calibrated |
| B. Thermostat-side port of a second OTGW32 | the OTGW32 has an OT slave port (`PIN_OT_SLAVE_IN/OUT` in `boards.h`) | known hardware | needs a slave-emulator firmware for that board; it ties up a second OTGW32 |
| C. Firmware injection hook | option B of `docs/plan/SAT_SIM_F7_BOILER_EMULATOR_OPTIONS.md` | no hardware | not a wire: it does not give transport identity, so it does not meet AC1 |

Identity evidence with a wire slave:

- **PIC (.63):** the PIC prints the slave's `B` lines from its UART (source `OTFRAME_SRC_PIC`, not `OTFRAME_SRC_REPLAY`). Three things must agree: `GET /api/v2/simulate` shows `active:false` for the whole run, `/api/v2/otgw/ot-support` gains acknowledged bits (a replay never sets them), and the slave's own log of each sent frame matches the PIC's `B` lines frame for frame.
- **OT-Direct (.61):** `otDirectBoilerPresent()` turns true, which loopback and replay never do. The flow feed shows `fc=V` with `fage` below 60 s, which replay can never show (calibration point C4). Matching the slave's log against `otframes.log` is the same check as on PIC.

### 8.2 AC2: timestamped faults and calibration numbers

Every injected stimulus gets a host time from one of three sources:

1. The fixture line, through `otframes.log` `host_time`. Each frame is timestamped when the capture receives it.
2. An operator action in `actions.log`.
3. A MikroTik action for network faults: `ssh -i ~/.ssh/mikrotik admin@192.168.88.1` with the router's log, plus the host action log line around each command. A network fault also pauses the replay (section 5), so log the pause and the resume.

Record these numbers per board, per firmware build, in `calibration.md` of the run folder:

| ID | Number | How | Accept (proposed, to approve before the run) |
|---|---|---|---|
| C1 | Replay dispatch error per line: median, p95, max | compare the gaps in `otframes.log` `device_time` with the fixture's gaps. The device logs a frame one frame late, so pair capture gap i+1 with fixture gap i (ADR-209 Consequences). Use SB01 (1 s and 0.15 s gaps) for at least 10 minutes | ADR-209 contract: ≤ 50 ms per gap. Reported to this task: within 10 ms on the bench on 2026-11-04/05 (not re-measured here). The host harness in the generator test shows 0 ms, but its clock is ideal and says nothing about the device |
| C2 | Capture latency: `host_time − device_time` offset, p50, p95, max, and drift over the run | from `otframes.log` | report only; the offset bounds how precisely a host-timed action can be placed against a frame |
| C3 | Capture completeness: frames captured ÷ fixture lines, per pass | count | 100 %; any loss is reported with its time |
| C4 | Replay boundary: `SATTICK fc/rc` during SB05/SB06 | telnet | `N` with `fage=4294967295` throughout (section 2). Any other letter means the boundary moved: stop and re-read the code |
| C5 | Reaction latency: stimulus host time → first observed reaction host time, per scenario | `otframes.log` vs `telnet.log`/`mqtt.log` | report; compare with the expected bound in the scenario (one control tick, one MsgID 3 cycle ≤ 14 s, 30 s link loss) |
| C6 | Rollback check result | section 6 | pass, every scenario |
| C7 | With a wire slave only: slave send time → PIC/OT-Direct `B` line host time, p50/p95/max | slave log vs `otframes.log` | report; needed before any wire timing claim |

## 9. Scenarios

`<id>.expected.json` holds the stimulus timeline, the preconditions, the operator actions, `expected_replay` (what a replay run must show), `expected_wire` (what a wire-slave run must show) and `does_not_prove`. Every expected output cites a decision entry in `sergeant-d-corrections.md`, a host test, a code symbol or an ADR. The generator test fails when a citation does not exist. Where nothing pins an output, it reads "expected: open, ask".

Every fixture is a 1 s request/answer stream. MsgID 0 is answered every 2 s, and IDs 25, 1, 3, 28, 17, 18 and 27 once per 14 s cycle. Defaults: CH and DHW enabled, flow 40 °C, return 30 °C, pressure 1.5 bar, outside 8 °C, member ID 4.

| ID | Mode | Stimulus | Expected (replay run) | Source | Observed via |
|---|---|---|---|---|---|
| SB01-support | replay-only | MsgID 3 Read-Ack, HB bit 4 = 0, 120 s | `slave_cfg_valid=true`, `slave_low_off_denied=false`, `pwm_veto_reason=0` within ≤ 14 s; flow `fc=N` throughout | entries 213, 215; `test_decoder_hook_stores_bit4_and_the_time_of_the_change`, `test_decoder_feeds_only_the_boilers_valid_read_ack`; `test_only_a_boiler_reply_refreshes_the_flow` | REST `/api/v2/sat`; MQTT `slave_configuration` = `00000000`; telnet SATTICK |
| SB02-veto | replay-only | HB bit 4 = 1; at 30 s MQTT `sat/control_mode = pwm` | `slave_low_off_denied=true`; `pwm_veto_reason=2`, `pwm_veto_path=3`; telnet `SAT: PWM refused (path 3): the boiler does not allow master low-off (MsgID 3 HB bit 4 = 1)`; `sat/mode` stays continuous | entry 213; `test_manual_pwm_command_is_blocked_without_permission` | REST, telnet, MQTT |
| SB03-veto-during-pwm | satsim+replay | bit 4 = 0 for 240 s (operator sets PWM), then bit 4 = 1; stop at 470 s | within one tick of the first bit-4 answer (~245 s): continuous, `pwm_veto_path=4`, next SAT-SIM trace CS is the continuous setpoint | entry 214; `test_bit4_rising_during_pwm_leaves_pwm_in_that_tick` | REST, MQTT `sat/mode`, telnet SAT-SIM trace |
| SB04-id3-unknown | replay-only | MsgID 3 answered Unknown-DataID; `pwm` command at 30 s | `slave_cfg_valid=false`; `pwm_veto_reason=1`, path 3; `PWM refused (path 3): no valid MsgID 3 reply from the boiler yet` | entry 215; same two tests as SB02/SB01 | REST, telnet |
| SB05-id25-data-invalid | replay-only | MsgID 25 Data-Invalid from 60 to 180 s | **boundary:** `fc=N`, no PWM. Wire: `I`, missing at 60 s age, recovers | entries 218, 241; `satFeedEventForFrame`; wire: `test_flow_data_invalid_is_missing_once_the_last_valid_sample_is_60_s_old_and_recovers` | telnet SATTICK; C4 |
| SB06-id25-unknown | replay-only | MsgID 25 Unknown-DataID from 60 s | boundary `fc=N`; no unsupported verdict. Wire: `U`, a later Read-Ack retracts it | `test_flow_unknown_dataid_is_unsupported_and_a_later_ack_retracts_it` (wire) | telnet; REST `/api/v2/otgw/ot-support` |
| SB07-id25-no-answer | replay-only | no answer to MsgID 25 from 60 to 150 s, others answered | boundary `fc=N`; `sat/ot_link_lost` stays false. Wire: missing from 60 s age | entries 79, 241; `test_a_alive_premise`; `test_flow_59_s_old_is_valid_and_61_s_old_is_missing` (wire) | telnet, MQTT |
| SB08-id28-data-invalid | replay-only | MsgID 28 Data-Invalid for 6 min | boundary `rc=N`. Wire: no return temperature (`rsrc=-`) after 5 min | entry 217; `test_return_data_invalid_for_5_minutes_counts_as_no_return_temperature` (wire) | telnet SATTICK |
| SB09-id28-unknown | replay-only | MsgID 28 Unknown-DataID | boundary `rc=N`. Wire: `U`, no return at once without a board sensor | entry 84; `test_unknown_dataid_with_no_board_sensor_is_no_return_at_once_as_before` (wire) | telnet |
| SB10-link-silence | replay-only, **SAT simulation off** | requests only, no `B` line from 60 to 105 s; stop at 160 s | `sat/ot_link_lost=true` ~30 s after the last `B`; one hand-back CS=0, CH=1, MM=T in the SAT command traces (section 7 filter), then no SAT command; false again on the first `B` | entries 47, 79; `test_b_loss_hands_back_once_then_silence`, `test_d_resume_on_first_reply` | MQTT, REST `ot_link_lost`, telnet |
| SB11-slow-ignition | wire-slave-required | CH active at 30 s, flame at 150 s | replay: `flamestatus` ON at 150 s; no PWM (flow missing). Wire, reactive slave (flame 120 s after CH=1): ON phase re-anchors at the ignition, lasts max(on_time, 180 s), no abort | `processOT`; entries 80, 218, 241; `test_d3_slow_ignition_at_120_s_times_on_from_the_ignition`, `test_d8_late_ignition_keeps_on_until_180_s_after_flame_on` | MQTT; wire capture |
| SB12-lockout-fault | replay-only | fault bit, no flame, 60 to 180 s | `SAT: boiler fault flag detected, skipping control cycle` each tick; no 5 s CS/MM/CH resend while the fault bit (MsgID 0 slave bit 0) is set. Wire (slave refusing to light): unlit ON ends at on_time, counts once | entries 221, 80; `satCmdResend()` (`SlaveStatus & 0x05`); `test_live_fault_and_dhw_bits_pause_the_resend`; `test_d4_no_flame_within_180_s_does_not_abort_on`, `test_e1_unlit_on_start_counts_once`, `test_flameless_on_restarts_count_against_the_limit` (wire) | telnet OTGW-SIM trace |
| SB13-dhw-draw | replay-only | DHW bit + flame from 60 to 180 s | MM to the maximum for the draw; on .63 the 30 s CS keepalive keeps running (PIC only); no 5 s resend | entries 220, 221, 232; `test_dhw_draw_sends_mm_max_keeps_the_keepalive_and_drops_the_resend` | telnet OTGW-SIM trace, SATTICK `dhw=1` |
| SB14-dhw-during-pwm | satsim+replay | bit 4 = 0; `pwm` at 30 s; DHW bit from 300 to 390 s; stop at 715 s | no PWM ON start while the hot-water flame burns, start after it, counted; the hot-water flame adds no start | entries 89, 106, 234; `test_h1_no_on_start_while_the_hot_water_flame_burns`, `test_e3_hot_water_flame_inside_on_adds_no_count` | telnet SAT log |
| SB15-pic-restart-banner | replay-only, **.63 only** | line `OpenTherm Gateway 6.8` at 60.5 s | MM, CH and CS once more with the latest values right after the banner | entries 237, 238; `test_mm_and_ch_only_on_change_and_again_after_pic_restart`, `test_pic_reset_resends_all_three_with_the_latest_values` | telnet OTGW-SIM trace |
| SB16-rest-burst | replay-only (SB01 fixture) | 30 s of 4 concurrent `GET /api/v2/sat` from the host | 503 for requests above the cap, no reboot. SAT tick delay: **expected: open, ask** (R-6) | ADR-165 | host burst log; MQTT `stats/max_loop_gap_ms` |
| SB17-pic-queue-full | host-only | none on the bench | rejected enqueue never cached as delivered; superseded command never on the wire | `test_full_queue_rejects_and_the_rejected_state_is_never_cached_as_delivered`, `test_superseded_command_never_reaches_the_wire` | host test only: during a replay commands are held back before the PIC queue |

Notes on individual fixtures:

- The SB15 banner is written in the four-column layout as source `O` and payload `penTherm Gateway 6.8`. The replay rebuilds `<source><payload>` = `OpenTherm Gateway 6.8` and keeps the line on time. The generator test checks this against the firmware's parser. On .61 a replayed banner still calls `satNotifyPicRestart()` (R-4), so SB15 is a PIC-only scenario.
- SB03, SB04 and SB14 need a fresh boot: a valid MsgID 3 reply from an earlier run stays until a restart.
- SB10 needs SAT simulation off: the loss check is not used in simulation (`test_e_simulation_is_exempt`).

## 10. Open items found while writing this procedure

None of these is decided here.

**For Sergeant D (SAT behaviour, one question per message per the SAT rules):**

- **Q-1412.20a.** After a valid MsgID 3 reply, the boiler answers MsgID 3 Data-Invalid or Unknown-DataID, or stops answering it. Today the firmware keeps the last permission until a restart (`satNoteSlaveConfig()` is the only writer of `bSlaveCfgValid`; code reading). Entries 213-215 settle "no valid reply yet" and "bit 4 turns 1", not this case. The verification plan (LL-010) asks for a separate timeout policy. thermo-nova only shows the bit as a sensor (`coordinator/serial/binary_sensor.py:54`, `DATA_SLAVE_MASTER_LOW_OFF_PUMP`, "Device Pump Commands Support", opened 2026-11-05) and never acts on it, so it has no answer to copy. Options:
  - A: keep the last valid permission (no change; MsgID 3 is configuration).
  - B: withdraw it after N minutes without a valid reply. PWM then ends as with entry 214.
  - C: withdraw it on the first invalid answer.

  Recommendation: A, because the bit describes the boiler's configuration, not its state. The risk is a boiler swapped without a restart.

**For Robert (equipment, tooling, firmware; not SAT design):**

- **R-1.** AC1 needs a wire-level OpenTherm slave (section 8.1, options A/B/C). Without one, TASK-1412.20 AC1 and the wire half of AC2 stay open.
- **R-2.** The `/ws` OT log has no per-frame replay marker, and `otgw_capture.py` drops event lines. A capture therefore cannot tell a replayed frame from a wire frame by itself. Possible fixes: poll `GET /api/v2/simulate` in the capture snapshots, or mark replayed lines in the OT log.
- **R-3.** Replayed MsgID 27 frames reach SAT's outside-temperature stamp (`OTGW-Core.ino:6076-6078`, no `bReplayed` check), but `docs/api/MQTT.md` says "live Read-Ack only". Verify on the bench, then fix either the doc or the code.
- **R-4.** On OT-Direct, a replayed `OpenTherm Gateway` banner calls `satNotifyPicRestart()` (`OTGW-Core.ino:6188`) before the `!isOTDirectEnabled()` guard. It looks harmless (a resend that is held back anyway) but is unintended by its comment.
- **R-5.** Plain SAT simulation never permits PWM (entry 215, no MsgID 3 source). Decide whether the simulation should model a MsgID 3 reply, or whether the bench keeps supplying it by replay.
- **R-6.** Is a bound on SAT tick delay under a REST burst wanted (SB16)?

## 11. What the bench run needs (TASK-1412.21 and the AC1/AC2 calibration)

1. On .63 and .61, with the same firmware build: run SB01 for 10 minutes with the capture and record C1, C2, C3.
2. On both boards, run SB05 and record C4. This confirms the replay boundary of section 2.
3. Run SB01, SB02, SB04, SB10, SB12 and SB13 on both boards, SB15 on .63 only, and SB03 and SB14 (satsim+replay) on both. Do the rollback check (C6) after each.
4. Run SB16 on one board.
5. Check R-3 once: replay SB01 with no MQTT outdoor push after a reboot, and read `sat/outside_temp_missing`.
6. Wire-slave scenarios (SB05-SB09 wire half, SB11, SB12 wire half) and C7: blocked on R-1.
