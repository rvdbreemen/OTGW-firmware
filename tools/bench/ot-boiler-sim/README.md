# OpenTherm boiler simulator (TASK-1654)

This is a second OTGW32 that acts as a scripted OpenTherm boiler for SAT wire-level bench tests (TASK-1412.20 R-1).
A replay fixture feeds boiler answers to the decoder on the board, but those answers never reach SAT's flow and
return feeds, and a fixed timeline cannot answer SAT's own commands (`docs/sat/bench-simulator-1412-20.md`,
section 2). This simulator answers on the wire.

It is not part of the OTGW firmware build. `build.bat`, `build.sh` and `build.py` do not see this folder.

## Status

| AC | What | State |
|---|---|---|
| AC1 | Answers MsgID 0/1/3/17/25/28 with scripted values and faults from a scenario file | The engine is host-tested: `python test/host/test_ot_boiler_sim.py`, 17 cases, plus 5 mutants that each turn it red. The sketch glue (`src/main.cpp`) has **not been compiled** and has not run on a board. |
| AC2 | The bench unit sees the simulator as a boiler on the wire | Not run: there is only one OTGW32. |
| AC3 | SB05-SB09 and SB11 on the wire, with the SAT reactions recorded | Not run: there is only one OTGW32. |

## Files

| Path | Role |
|---|---|
| `src/boiler_sim.h`, `src/boiler_sim.cpp` | The engine. It is pure C++ with no Arduino headers, so the sketch and the host test run the same file. |
| `src/main.cpp` | The sketch glue: pins, bus power, the OpenTherm library in slave mode, scenario loading from LittleFS, the USB log and serial commands. |
| `host/sim_driver.cpp` | The host driver. It reads requests (`t_ms HEX8`) on stdin and prints the answers. |
| `export_scenarios.py` | Writes `data/*.scn` from `scripts/sat_bench_scenarios_1412_20.py`. It only imports the generator, so the generator's `VERSION` and fixture hashes stay valid. |
| `data/` | The LittleFS image: the exported SB05-SB09 and SB11 scenarios, `index.txt` (generator version plus a sha256 per file) and the hand-written `SIM-dhw-draw.scn`. |
| `../../../test/host/test_ot_boiler_sim.py` | The host test. `scripts/run_tests.py` runs it together with the other `test/host` harnesses. |

## How the boiler behaves

The boiler reacts to each request, as a real boiler does:

- **MsgID 0.** The answer echoes the master flags in the high byte. The low byte holds the slave flags:
  - bit 0: fault, from the scenario;
  - bit 1: CH active. It follows the master's CH enable, unless the boiler is faulted or a DHW draw runs;
  - bit 2: DHW active. It is set while the scenario has a DHW draw and the master enables DHW;
  - bit 3: flame. It needs heat demand: CH active and a written TSet (MsgID 1) of at least `demand_min` (default
    20 °C). Before any TSet write, the boiler counts as demanding on its own setpoint. The flame lights
    `ignition_delay` seconds after demand starts and goes out when demand ends; with `-1` it never lights. It is also
    lit during a DHW draw.

  SAT's PWM OFF phase keeps CH=1 and sends CS=10 (`src/OTGW-firmware/SATcontrol.ino:1601`, `SAT_MIN_SETPOINT` at
  `:132`). CH=0 is sent only on the summer and valves-closed paths (`:7080`, `:7096`). Because of the demand rule, every
  PWM ON phase ignites again. Without it the flame would light once and burn through every OFF phase, and SB11 would
  only test the first cycle. The host case `SB11_every_pwm_on_phase_ignites_again_after_cs10_off` checks this.
- **MsgID 1.** Write-Ack echoes the TSet the master wrote. A later read returns that value.
- **MsgID 3.** Read-Ack with `slave_hb` and `member_id`.
- **MsgID 17.** Reports `mod` while the flame burns, otherwise 0.
- **MsgID 25 and 28.** Report `flow` and `ret`.
- **Other IDs that the bench unit sends** (`src/OTGW-firmware/OTDirect.ino` `otSchedule[]`, from line 131). Without an
  answer to these, every wire run would also carry an Unknown-DataID that the scenario did not ask for. So:
  - writes to 2, 14, 16, 24, 27, 56, 57, 124 and 126 get a Write-Ack;
  - reads of 15, 18, 26, 27, 125 and 127 get a default value;
  - reads of 1, 14, 56 and 57 return the stored value.

  Every other ID gets Unknown-DataID. A real basic boiler does the same. OTDirect disables that schedule entry after
  three Unknown-DataID answers (`OTDirect.ino:1496-1503`).
- **Requests that get no answer:** a request with bad parity, and any frame type other than Read-Data or Write-Data.
- **Answer delay.** The default is 100 ms. The parser accepts 20 to 800 ms (the OT spec v4.2 slave window). A slow
  ignition is modelled as a late flame bit, never as a late answer.

### Scenario file (`.scn`)

```
scenario SB05-id25-data-invalid
source sat_bench_scenarios_1412_20 1412.20-v1
default flow=40 ret=30 pressure=1.5 outside=8 member_id=4 slave_hb=0 fault=0 ignition_delay=0 mod=30
phase 60
phase 120 reply25=data_invalid
phase 60
phase 0  # healthy hold after the scripted timeline
```

- **Phases.** Each `phase <seconds> key=value ...` starts from the `default` line. The clock starts at the first valid
  request. The last phase holds forever.
- **Keys:** `flow`, `ret`, `mod`, `pressure`, `outside`, `dhw_temp`, `slave_hb`, `member_id`, `max_kw`, `min_mod`,
  `fault`, `dhw_draw`, `ignition_delay`, `demand_min`, `silent`, `answer_ms` and `reply<ID>=ack|data_invalid|unknown|none`.
- **Errors.** An unknown key is a parse error, so the simulator never runs a scenario it misread.

### What the export keeps from the 1412.20 generator

- **Kept:** the boiler-side values and the per-ID faults.
- **Dropped:** the values the master decides on the wire (`ch_enable`, `dhw_enable`, `tset`), and the status that the
  replay scripts (`ch`, `flame`, `mod`, `dhw`).
- **Added:** a healthy hold after the timeline. Without it, SB06 and SB09 could not show the recovery that their
  `expected_wire` names.
- **SB11.** It gets `ignition_delay=120`, from its `expected_wire` ("reactive slave: flame 120 s after CH=1"). The
  replay variant has a fixed flame at 150 s instead.

## Wiring

```
   bench unit (OTGW32 .61, or Classic .63)          simulator (second OTGW32)
   +---------------------------+                    +---------------------------+
   | boiler side  (OT master)  |==== 2-wire OT ====| thermostat side (OT slave)|
   |                           |                    |  powered by the step-up   |
   | thermostat side: unused   |                    | boiler side: unused       |
   +---------------------------+                    +---------------------------+
```

- **Connect** the bench unit's **boiler** OpenTherm terminals to the simulator's **thermostat** terminals. OpenTherm
  is polarity-free.
- **Nothing else** goes on that pair: no real boiler and no thermostat.
- **Bus power.** The simulator powers the line, as a boiler does. Its step-up (`PIN_STEPUP_ENABLE`, GPIO 10) is
  switched on at boot.
- **Bypass relay.** `PIN_BYPASS_RELAY` (GPIO 47) is held LOW, so the simulator's two OT ports stay apart.
- **Pins** are copied from `src/libraries/Platform/src/boards.h:75-76, :79 and :85`. The host test fails if they drift.

## Build, flash and run (once there is a second OTGW32)

Robert decides on this. These commands are written down here and have not been run.

The firmware rule "build only through `build.bat`/`build.sh`" covers the firmware. This is a separate PlatformIO
project, so it can only be built with `pio run` from this folder. That bypasses the machine-wide build lock
(TASK-1488). Run it only when no other build is running, and say so to the lead session first.

```
cd tools/bench/ot-boiler-sim
git submodule update --init ../../../src/libraries/OpenTherm
<pio> run                   # build
<pio> run -t upload         # flash over USB (the simulator board only, never a bench unit)
<pio> run -t uploadfs       # LittleFS with the scenarios in data/
<pio> device monitor        # USB log, 115200
```

- `<pio>` is the PlatformIO in its own venv (`~/.platformio/penv/Scripts/python.exe -m platformio`), because the host
  Python may be 3.14.
- **Serial commands:** `list`, `run SB11-slow-ignition.scn`, `status` and `restart`. The active scenario is kept in
  `/active.txt`.
- **Log lines:** `<ms> T<hex8>` is a request, `<ms> B<hex8>` an answer, `<ms> -` a deliberate silence and
  `<ms> X<hex8>` an invalid frame. This log is the simulator side of every capture.

### AC2: the bench unit sees a boiler on the wire

1. Wire as above. Load `SB05-id25-data-invalid.scn`, then `restart` just before the capture.
2. On the bench unit:
   - turn SAT off, and replay off (`GET /api/v2/simulate` must show no active replay);
   - with no thermostat attached, the bench unit must originate its own requests:
     - **OTDirect `.61`** does this in gateway mode, the default. Monitor mode sends nothing of its own
       (`OTDirect.ino:2182-2186`). It probes with MsgID 0 until the simulator answers, then runs `otSchedule[]`.
     - **Classic `.63`** has not been checked. Whether the PIC sends its own requests with no thermostat on its
       thermostat side is not verified here. Check that before using `.63` as the bench unit;
   - start `scripts/capture-mqtt-debug.bat -DeviceHost <ip> -DurationSeconds 300`.
3. Pass when:
   - the capture shows B frames for MsgID 0/3/25/28 with the values from the scenario (flow 40, return 30, member ID 4);
   - the frame times match the simulator's USB log (same frames, one answer per request);
   - the bench unit's OT bus status is online.

   A replayed fixture cannot produce this, because replay is off.

### AC3: SB05-SB09 and SB11 on the wire

For each scenario:

1. `run <id>.scn`, then `restart`.
2. Turn SAT on on the bench unit, with `debug sat` on for the `SATTICK` lines and replay off.
3. Record the telnet log, the capture and the simulator USB log for the duration of the scenario plus 60 s.
4. Compare with the `expected_wire` outputs in `<id>.expected.json`. Generate those with
   `python scripts/sat_bench_scenarios_1412_20.py --out build/sat-bench-1412-20`.
   - SB05: `fc` goes V, then I, then missing once the last valid sample is 60 s old, then V again.
   - SB06: `fc=U`, retracted by the Read-Ack in the healthy hold.
   - SB07: `fage` grows from 60 s and recovers.
   - SB08: `rsrc=-` after 5 minutes of Data-Invalid.
   - SB09: `rc=U`, and no return temperature at once when the board has no sensor.
   - SB11: the PWM ON phase re-anchors at the flame (120 s after the ON phase's CS) and lasts max(on_time, 180 s), with no
     ignition abort.
5. The run report names:
   - the generator version and `data/index.txt` sha256 of the scenario;
   - the simulator commit;
   - the firmware version and githash of the bench unit.

### Transports

The 1412.20 scenarios list both transports. With one OTGW32 flashed as the simulator, the Classic `.63` can be the
bench unit (PIC transport), if the PIC sends its own requests (see AC2). The cost is that `.61` is then not available as the OT-Direct unit under test. A second
OTGW32 removes that trade-off. The choice belongs to the lead session and Robert.

## What this does not prove

- **No boiler physics.** Flow and return follow the scenario, not the burner.
- **Host test scope.** The host test proves the engine's answers. It does not prove the OpenTherm library's timing in
  slave mode, the bus power, or how the bench unit decodes the answers. Those are for AC2.
