# Phase 1B: Architecture & Design Review

Range: `ace21a48^..fa5ef3c5` (8 commits, 12 source files, 1 new ADR).

## Summary

The session work is architecturally coherent. Two substantial additions land
cleanly: the OTDirect TT/TC state machine and the SAT BLE port to NimBLE
2.x with Home Assistant auto-discovery. Both honour the dual-struct settings
/ state model, both extend per-component types files in line with ADR-079,
and the dependency graph between SAT, OTDirect, MQTT, and BLE remains
unidirectional (SATble depends on MQTTstuff via `OTGW-firmware.h` forward
decls, not the other way around). ADR-092 is well-formed and discharges
the four verification gates honestly.

That said, three concerns are worth addressing before the next release:
the platformio.ini reformat silently dropped institutional knowledge and
bumped a pinned tooling version with no rationale; the BLE HA-discovery
helpers explicitly opt out of ADR-077's two-pass / chunked-streaming
contract in favour of a 768-byte stack buffer; and the BLE scan callback
runs on a separate FreeRTOS task on ESP32 but writes `_bleSensors[]`
without a guard, while the loop-task reader has no synchronisation with
those writes (the OT-Thing reference uses a `SensorLock` for exactly this
reason). None of these are blockers, but the platformio.ini regression
should not be allowed to set precedent.

## Critical findings

(none)

## High findings

### H1. platformio.ini reformat erased institutional rationale and bumped a pinned tool version

- **Severity**: High
- **Architectural impact**: Build reproducibility, project memory, and
  documented platform pins.
- **Where**: `platformio.ini`, commit `59b1478d`
- **Detail**: The NimBLE-add commit auto-reformatted the entire file
  (spaces -> tabs, comments collapsed). Diff stats: -137 / +45 lines, of
  which roughly 90 removed lines were rationale comments. Concretely lost:
  - The full `[env:esp8266]` rollback rationale (Core 2.7.4 LTS choice,
    the Arduino-Core 3.1.0 `Update`-path regression citation, the
    `WiFiClient::stopAll` PR esp8266/Arduino#8598 reference). This is
    direct prose about ADR-082.
  - The `-D OpenThermResponseStatus=int` / `-D OTDirectMode=int`
    explanation (ctags forward-decl pipeline, why it is safe).
  - The `-D OTGWFirmware=int` ESP32 rationale.
  - The `lib_ignore = OpenTherm Library` and `lib_ignore = OTGWSerial`
    rationale (LDF scanner ignores `#if` guards).
  - The `build_src_filter` rationale (ctags scans excluded files).
  - The `-Wno-unused-function` rationale (ctags-generated dead helpers
    on the inactive platform).
  - The `platform_packages = platformio/tool-esptoolpy @ ~1.30000.0`
    rationale on ESP32 (pioarduino's bundled esptoolpy-v5.1.0 needs the
    IDF tools installer, fails on MSys/MinGW).
- **Tooling-version regression**: `tool-esptoolpy` was bumped from
  `~1.30000.0` to `^2.41100.0` AND added to `[env:esp8266]` (it was
  previously only on ESP32 with an explicit Windows/MinGW justification).
  No commit message line, no ADR, no justification. Caret-pin on a
  Windows-fragile dependency is the opposite direction from the original
  decision.
- **Why High, not Medium**: Several of these comments documented the
  intent of an Accepted ADR (esp. ADR-082 for the ESP8266 Core 2.7.4 pin)
  in the file where future contributors will look first. Re-deriving them
  is hours of digging; the institutional knowledge sat in this file
  precisely because it is the first place a build issue gets investigated.
  The tool-version change is the kind of silent regression that causes
  "works on my machine" Windows breakage with no audit trail.
- **Recommendation**: Restore the pre-`59b1478d` comment blocks and the
  esptoolpy `~1.30000.0` pin (or open a ticket with explicit justification
  for the `^2.41100.0` bump and re-record the rollback path). The TASK-487
  / TASK-488 commit only needed `+ h2zero/NimBLE-Arduino @ ^2.1.0` under
  `[env:esp32].lib_deps`. The reformat was not part of the feature work
  and should be reverted on the same diff.

## Medium findings

### M1. ADR-077 spirit not honoured by `bleSensorPublishOneDiscovery`

- **Severity**: Medium
- **Architectural impact**: ADR-077 conformance, stack-budget consistency.
- **Where**: `src/OTGW-firmware/MQTTstuff.ino:1990-2056` (the
  `bleSensorPublishOneDiscovery` helper).
- **Detail**: ADR-077 prescribes (a) two-pass MEASURE/WRITE so the byte
  count is known before `beginPublish`, (b) 128-byte chunked publish via
  `writeMqttChunk` with `feedWatchDog()` between chunks, (c) "no single
  buffer holds the full payload". The new helper allocates `char
  payload[768]` on the stack, fills it with `snprintf_P`, then calls
  `writeMqttChunk(payload, n)` once. This is functionally heap-safe (the
  buffer is on the stack, not the heap) but the deviation from ADR-077 is
  explicitly acknowledged in the comment block at line 1968. The author's
  reasoning ("payload bounded under 600 bytes") is plausible, and the
  ESP8266 4 KB CONT stack risk is moot because the helper is
  `#if defined(ESP32)`-only with an 8 KB main task stack.
- **Why this still matters**: ADR-077's contract is uniform across the
  discovery pipeline. Adding a per-payload exception (768 B is bounded;
  payload X is bounded; payload Y is bounded ...) erodes the "every
  publish goes through the same shape" property over time. The ADR-077
  drip-bitmap can also not pace these publishes, because BLE MACs are not
  OT IDs — drip pacing relies on the caller cadence (one BLE scan per
  `iBleInterval`) plus the one-shot `bDiscoveryPublished` flag, which is
  caller-private and not visible from `loopMQTTDiscovery`.
- **Recommendation**: Either (a) refactor `bleSensorPublishOneDiscovery`
  to use the existing two-pass `MqttJsonWriter` + `writeMqttChunk` shape
  in `MQTTHaDiscovery.cpp`, dropping the 768-byte stack buffer, or (b)
  amend ADR-077 with an explicit "bounded-payload single-buffer
  exception" clause and record what the bound is (768 bytes? 1024?), how
  it is enforced, and the rationale. The current state is "ADR is in
  effect except where this comment opts out", which is exactly the
  binding-on-paper-unchecked-in-practice failure mode ADR-080 tries to
  prevent.

### M2. BLE scan callback runs on FreeRTOS BLE host task; `_bleSensors[]` access is unguarded

- **Severity**: Medium
- **Architectural impact**: ADR-090 re-entrancy / data-race envelope.
- **Where**: `src/OTGW-firmware/SATble.ino:193-270` (callback writes),
  `:332-375` (loop-task `satBLEUpdateState` reads), `:430-449`
  (loop-task `satBLEPublishMQTT` reads).
- **Detail**: NimBLE 2.x on ESP32 dispatches `onResult` on the BLE host
  task, not the Arduino main loop. The new code performs slot writes
  in the callback (`_bleSensors[slot].fTemperature`,
  `bDiscoveryPublished = false`, `sMacAddress` strlcpy, `bValid = true`)
  while the loop task reads the same array in `satBLEUpdateState` and
  iterates it in `satBLEPublishMQTT`. There is no mutex, no atomic
  ordering hint, no `volatile`. The OT-Thing reference implementation at
  `other-projects/OT-Thing-OTGW32/Firmware/src/sensors.cpp:367-388`
  takes a `SensorLock` (RAII) inside `onDiscovery` for exactly this
  reason.
- **Realistic impact**: Low. Slot writes target a single slot; the worst
  observable race is the loop reading a slot mid-write and seeing
  `sMacAddress` half-updated, which `bleMacToCompact` rejects as
  malformed (returns `""`) and the publish is skipped that cycle. Float
  reads are 4-byte aligned and atomic on Xtensa LX7. The
  `bDiscoveryPublished` racing back to `false` after the loop set it to
  `true` causes only a duplicate retained-config publish (idempotent on
  the broker side).
- **Why Medium, not Low**: ADR-090's "no volatile required on ESP8266"
  rule explicitly notes "A future SAT/OTGW32 introduction of FreeRTOS-style
  threading would require revisiting this rule". This commit is the
  introduction. The ADR's sub-rule 4 is now silently invalidated for
  this code path. ADR-090 should be amended (or a new pattern-level ADR
  written) to record the ESP32 BLE callback as a guarded re-entrancy
  point, or the code should adopt a guard explicitly even if the
  observable race is benign today.
- **Recommendation**: Either (a) wrap callback writes and loop-task
  reads in a `portMUX` / FreeRTOS critical section, or a NimBLE-friendly
  `SemaphoreHandle_t` mirroring OT-Thing's `SensorLock`, or (b) document
  the specific tear-resistance argument in `SATble.ino` (which writes
  are atomic, why `bValid = true` ordering is sufficient, what the
  worst-case is) and amend ADR-090 to acknowledge the new ESP32 case.
  The current state has neither and the next reviewer will trip over
  this question again.

### M3. New BLE helpers in `MQTTstuff.ino` break the `satBLE*` naming convention

- **Severity**: Medium
- **Architectural impact**: API consistency across `OTGW-firmware.h`.
- **Where**: `src/OTGW-firmware/OTGW-firmware.h:255-257` and the
  corresponding definitions in `MQTTstuff.ino`.
- **Detail**: The existing SAT BLE API uses the `satBLE*` prefix:
  `satBLEInit`, `satBLELoop`, `satBLEUpdateState`, `satBLEPublishMQTT`,
  `satBLESendStatusJSON`, `satBLEGetTemperature`, `satBLEGetHumidity`.
  The three new symbols drop the `sat` prefix:
  - `void bleMacToCompact(...)`
  - `void bleSensorPublishStateTopics(...)`
  - `void bleSensorPublishHaDiscovery(...)`
  These functions are conceptually part of the SAT BLE subsystem (the
  caller is `satBLEPublishMQTT` in `SATble.ino`), even though they live
  in `MQTTstuff.ino`. Mixing prefixes for one logical subsystem makes
  the API harder to grep.
- **Recommendation**: Rename to `satBleMacToCompact`,
  `satBlePublishStateTopics`, `satBlePublishHaDiscovery` (camelCase
  matches the existing `satBLE` shape; the lowercase `Ble` after `sat`
  matches the rest of the project's compound-noun camelCase). Touches
  three forward-decls plus three definitions plus one or two callers.

### M4. `tests/test_evaluate.py` is in `00-scope.md` but not in the diff

- **Severity**: Medium (process / scope-doc accuracy)
- **Architectural impact**: Reviewer trust in the scope manifest.
- **Where**: `.full-review/00-scope.md:30` lists
  `tests/test_evaluate.py` as in-scope; `git log` shows the file's last
  touch was `20b84762` (TASK-470, prior session).
- **Detail**: TASK-482 (`ace21a48`) updated `evaluate.py` to skip
  `*.ino.cpp` and macro continuations, and the scope manifest claims new
  tests for the gate; `git diff --stat` for the range only shows
  `tests/otdirect_pic_parity_fixture.md` changes. The TASK-482 commit
  delivered only the production fix, not corresponding tests. ADR-080's
  explicit rule is "binding ADRs need a CI gate"; the inverse (gate
  fixes need a regression test) is not formally bound, but the scope doc
  asserting tests-that-don't-exist is a process smell.
- **Recommendation**: Either add the missing tests in a follow-up task
  (TASK-482 follow-up) or correct `00-scope.md` to remove the
  `tests/test_evaluate.py` line. The current state misleads later
  reviewers about what TASK-482 actually shipped.

## Low findings

### L1. SATble.ino comment about NimBLE units is technically correct but easy to misread

- **Severity**: Low (doc clarity)
- **Where**: `src/OTGW-firmware/SATble.ino:285-286`
- **Detail**:
  ```cpp
  _pBLEScan->setInterval(160);   // 100 ms BLE scan-interval
  _pBLEScan->setWindow(80);      // 50 ms BLE scan-window (50% radio duty)
  ```
  NimBLE's `setInterval(N)` / `setWindow(N)` argument is in 0.625 ms
  units. 160 * 0.625 = 100 ms, 80 * 0.625 = 50 ms — the comments are
  correct values but omit the unit-conversion step, which is what the
  next reader will want to know when changing them.
- **Recommendation**: Adjust to
  `setInterval(160);   // 160 * 0.625 ms = 100 ms` etc. The OT-Thing
  reference at `sensors.cpp:362-363` has the same numbers without
  comments; OTGW-firmware should be the more readable of the two.

### L2. `setRemoteOverride` enqueues a write while ALSO activating the repeater override

- **Severity**: Low (PIC-parity question, not a bug)
- **Where**: `src/OTGW-firmware/OTDirect.ino:1888-1905` (setter calls
  `enqueueWriteCommand`, which internally calls `setOverride` to
  activate the repeater path).
- **Detail**: After `setRemoteOverride(TEMPORARY, 20.0)` the boiler will
  see two things on every cycle: (a) the gateway's standalone
  `WRITE_DATA MsgID 16 0x1400` from the master scheduler, and (b) the
  thermostat's own MsgID 16 frame rewritten to `0x1400` by
  `applyOverrides`. PIC behaviour does the same per the test fixture
  notes, so this is intentional. Worth a code comment recording that
  the dual-path is deliberate, because at first glance it looks like
  a bug (why both?).
- **Recommendation**: One-line comment in `setRemoteOverride` body: "The
  repeater override (via setOverride called from enqueueWriteCommand)
  rewrites thermostat-originated frames; the standalone WRITE_DATA
  pushes our value into the periodic schedule. Mirrors PIC dual-path."

### L3. `OT_OVERRIDE_HONOR_DELTA_F88` constants computed at compile time look right but use float arithmetic

- **Severity**: Low (style)
- **Where**: `src/OTGW-firmware/OTDirect.ino:328-330`
- **Detail**:
  ```cpp
  static constexpr uint16_t OT_OVERRIDE_HONOR_DELTA_F88   = (uint16_t)(0.25f * 256.0f); // 0x0040
  static constexpr uint16_t OT_OVERRIDE_RELEASE_DELTA_F88 = (uint16_t)(0.50f * 256.0f); // 0x0080
  ```
  The hex annotations match (0.25 * 256 = 64 = 0x40; 0.50 * 256 = 128 =
  0x80). The float arithmetic is constexpr and the result is a clean
  integer for these specific values, but if a future tuner changes 0.25
  to 0.3 the cast truncates 76.8 to 76, not rounds — surprise on the
  next change. Direct integer literals (`0x0040`, `0x0080`) with the
  Celsius-equivalent in a comment would remove the foot-gun.
- **Recommendation**: Optional. If kept, add a comment noting that
  changing the float multiplier will silently truncate fractional results.

### L4. SATble.ino re-publishes per-MAC state topics every scan cycle; no rate-limit

- **Severity**: Low (MQTT load)
- **Where**: `src/OTGW-firmware/SATble.ino:430-449`
- **Detail**: `satBLEPublishMQTT` iterates every valid slot and calls
  `bleSensorPublishStateTopics` (4 topics per MAC, non-retained). With
  4 sensors and a default 30 s interval, that is 16 publishes per
  cycle, cleanly within MQTT capacity. With the maximum
  `iBleInterval = 10 s` (per `restAPI.ino:2070`), worst case 96
  publishes/min which is still fine but pushes against ADR-088's
  status-burst cooldown territory if it overlaps the OT status burst.
- **Why Low**: The message rate is well within budget. Worth a sanity
  check rather than a code change. Watch the ADR-088 metrics during
  hardware soak.

## ADR conformance check

| ADR | Verdict | Rationale |
|---|---|---|
| ADR-004 (no String in hot paths) | **Pass** | NimBLE `std::string` replaces Arduino `String` in the BLE callback. Three new MQTT helpers use `char[]` + `snprintf_P` exclusively. |
| ADR-051 (settings/state architecture) | **Pass** | New fields land in `state.otd.eOverrideMode` / `state.otd.iOverrideF88` and `BLESensorData::bDiscoveryPublished`, all with correct Hungarian prefix. The `e`-for-enum prefix is established convention (used in `state.net.eMode`, `state.hw.eMode`, `state.sat.eBoilerStatus`). |
| ADR-077 (streaming HA discovery) | **Concern** (M1) | `bleSensorPublishOneDiscovery` uses a 768-byte single-buffer model, opting out of the two-pass MEASURE/WRITE + 128-byte chunked publish prescribed by ADR-077. The deviation is acknowledged in a comment but not amended into ADR-077 itself. |
| ADR-079 (per-component types headers) | **Pass** | `OTRemoteOverrideMode` enum and `OTRemoteOverrideState` struct land in `OTDirecttypes.h`, alongside the existing `OTDirectSection`. Bundling matches the ADR-079 grouping guidance: enum + struct for the same subsystem in one file. |
| ADR-089 (heap tier machine) | **Pass** | `bleSensorPublishOneDiscovery` calls `canPublishMQTT()` and `MQTT_DISCOVERY_HEAP_MIN`-checks before each publish. State-topic helper is on the cheaper publish path (`sendMQTTData`) which has its own gates. |
| ADR-090 (re-entrancy guards) | **Concern** (M2) | The cooperative-ESP8266 reasoning of ADR-090 sub-rule 4 ("no volatile required") is silently broken by the new ESP32 BLE-host-task callback. The data race is benign in practice but the ADR's scope statement is now stale. |
| ADR-092 (NimBLE adoption) | **Pass** | Status=Accepted with all four verification gates (Completeness, Evidence, Clarity, Consistency) explicitly discharged in the body. Decision is one-way and acknowledged as such. Pinned `^2.1.0`; the OT-Thing reference at `sensors.cpp:355-365` is correctly cited. The "structural ADR per ADR-080" classification is honest — the change is one-line lib_deps + a file rewrite, no automated gate is feasible. |

## Strengths observed

1. **OTDirect TT/TC state machine is the right shape.** Three small
   functions (`setRemoteOverride`, `clearRemoteOverride`,
   `onThermostatMsgID16`) share one file-static struct with explicit
   "not persisted, mirrors PIC behaviour" semantics. Counter-based
   honour/release with named tunable constants is exactly the kind of
   small state machine that fits OTGW-firmware's "clear over clever"
   ethos. The gateway.asm citation in the comment block makes the PIC
   parity claim verifiable.
2. **Sign-extension fix in `onThermostatMsgID16` (TASK-491) shows
   careful thinking about edge cases.** The cast through `int16_t`
   before widening to `int32_t` and the inline rationale comment cover
   exactly the kind of bug that bites embedded code three years after
   it ships. Negative TrSet for frost-protect or freezer-room
   deployments is rare but real.
3. **`OTRemoteOverrideState` placement in `OTDirecttypes.h` is exemplary
   ADR-079 conformance.** Enum, struct, default values, all in the same
   file as the other OTDirect types. Pre-empts the
   "where does this go?" question for the next state machine.
4. **Dependency direction is clean.** `MQTTstuff.ino` does NOT include
   any SAT or BLE header. It exports flat-typed primitives
   (`const char*`, `float`, `uint8_t`, `int8_t`); `SATble.ino` consumes
   them via `OTGW-firmware.h` forward decls. No circular dependency,
   no leakage of `BLESensorData` into the MQTT module.
5. **The `bDiscoveryPublished` flag reset on slot recycling
   (TASK-489)** is the kind of subtle lifecycle correction that
   prevents a real-world bug (HA losing a discovery config when a sensor
   goes stale and its slot gets reused for a different MAC). The
   implementation is one extra `strcmp` in the existing flow, not a
   new state machine.
6. **ADR-092 follows the discipline ADR-080 prescribes.** Honest
   "structural, no CI gate" labelling. Verification gates filled in
   with concrete file:line evidence rather than hand-waved
   "documented" claims.
7. **Test fixture growth tracks the feature.** `tests/otdirect_pic_parity_fixture.md`
   gains 9 new rows for TT/TC behaviour including the
   negative-TrSet boundary case and the
   "TT then TC" replacement case. The fixture is the contract; growing
   it alongside the implementation keeps it honest.
