# Phase 1: Code Quality & Architecture Review

Consolidated summary of findings from `phase1a-code-quality.md`
(`comprehensive-review:code-reviewer` agent) and `phase1b-architecture.md`
(`comprehensive-review:architect-review` agent).

## Code Quality Findings (Phase 1A)

**Counts**: 0 Critical, 1 High, 5 Medium, 8 Low.

### High

- **1A-H1**: `bDiscoveryPublished = true` is flipped unconditionally
  in `satBLEPublishMQTT()` after calling `bleSensorPublishHaDiscovery()`,
  but the helper has eight gating early-returns
  (`!settings.mqtt.bEnable`, `!MQTTclient.connected()`,
  `!isValidIP(MQTTbrokerIP)`, `!canPublishMQTT()`, low-heap,
  `endPublish` failure, …) and silently returns `void`. A transient
  failure (e.g. first BLE scan landing before MQTT has connected)
  permanently suppresses HA discovery for that sensor until the slot
  is recycled with a different MAC (TASK-489) or the device reboots.
  Fix: change the helper signature to return `bool`, gate the flag
  flip on success.

### Medium

- **1A-M1**: `setRemoteOverride()` writes state first, then enqueues
  two frames via `otCmdEnqueue`. If the queue is full the override
  state machine claims `mode=TEMPORARY` with no frame in flight, and
  the auto-clear honour-state-machine waits forever.
- **1A-M2**: `lastThermostatVal = 0xFFFF` sentinel collides with the
  valid f8.8 value `-0.0039 °C` (raw `0xFFFF` interpreted as
  `int16_t` is `-1`, and `/ 256.0` is roughly `-0.0039`). Harmless
  today since the field is not read for control flow, but the
  contract is fragile.
- **1A-M3**: TT/TC handlers call `setRemoteOverride(temp)` and
  `clearRemoteOverride()` but the threshold for "clear" is exact
  zero (`temp == 0.0f`); any negative value would set a TEMPORARY
  override of negative °C, which silently slips past the safety
  intent.
- **1A-M4**: `bleSensorPublishOneDiscovery()` does not check
  `endPublish()` exit cleanly on the `writeMqttChunk` failure path;
  retains a partial publish state.
- **1A-M5**: BLE state-topic format (`/sat/ble/<mac>/temp`) and
  discovery `stat_t` format (`/value/<uniq>/sat/ble/<mac>/temp`) are
  duplicated in two places (state-publish helper + discovery JSON).
  Silent drift would break HA dashboards in a hard-to-diagnose way.

### Low

- 1A-L1..L8: cumulative naming, comment, and minor style nitpicks.
  See full report.

## Architecture Findings (Phase 1B)

**Counts**: 0 Critical, 1 High, 4 Medium, 4 Low.

### High

- **1B-H1**: `platformio.ini` was auto-reformatted in commit
  `59b1478d`, erasing about 90 lines of build-rationale comments
  (including the ADR-082 ESP8266 Core 2.7.4 LTS rollback explanation,
  the ctags `-D` flag justifications, and the Windows/MinGW
  esptoolpy pin rationale). The `tool-esptoolpy` pin was silently
  bumped from `~1.30000.0` to `^2.41100.0` and the same pin was
  added to the ESP8266 env (which originally did not pin
  esptoolpy). No commit-message line, no ADR, no review trail.
  Recommend reverting the reformat to the original prose-rich
  shape on the same diff, OR documenting the dependency bump in
  a follow-up ADR.

### Medium

- **1B-M1**: `bleSensorPublishOneDiscovery()` opts out of ADR-077's
  chunked two-pass MEASURE-then-WRITE shape with a 768-byte stack
  buffer. The comment in the body acknowledges this (TASK-490) but
  ADR-077 itself was not amended to whitelist the bounded-payload
  exception.
- **1B-M2**: NimBLE 2.x scan callback runs on a separate FreeRTOS
  task on ESP32; `_bleSensors[]` is written from the scan task and
  read from the loop task in `satBLEPublishMQTT()` /
  `satBLEUpdateState()` without a guard. ADR-090's "no volatile
  required if cooperative" assumption is silently invalidated by
  the parallel-task model. Real risk: torn read of `bDiscoveryPublished`
  or `iLastSeenMs` (8-bit and 32-bit fields). For 4-slot data this
  is unlikely-but-possible.
- **1B-M3**: New exports `bleMacToCompact`,
  `bleSensorPublishStateTopics`, `bleSensorPublishHaDiscovery`
  break the established `satBLE*` naming convention used by
  `satBLEInit`, `satBLELoop`, `satBLEUpdateState`,
  `satBLEPublishMQTT`, `satBLESendStatusJSON`. Either rename the
  new exports to `satBLE*` or document the naming distinction.
- **1B-M4**: `00-scope.md` (this review's own scope manifest)
  claims `tests/test_evaluate.py` was touched in the diff window;
  `git diff ace21a48^..fa5ef3c5 -- tests/test_evaluate.py`
  shows zero changes. Cosmetic but undermines the review's own
  precision.

### Low

- 1B-L1..L4: documentation and naming consistency nitpicks.
  See full report.

## ADR conformance verdict

| ADR | Status | Note |
|-----|--------|------|
| ADR-004 (no String hot-path) | Pass | NimBLE port replaced Arduino `String` with `std::string`; new helpers use `char[]` exclusively. |
| ADR-051 (settings/state) | Pass | `state.otd.eOverrideMode` / `iOverrideF88` follow Hungarian + two-level structure. |
| ADR-077 (streaming HA discovery) | Concern | Single-buffer publish (1B-M1); spirit honoured but ADR not amended. |
| ADR-079 (per-component types) | Pass | OTDirect types in `OTDirecttypes.h`. |
| ADR-089 (heap tier machine) | Pass | `canPublishMQTT()` + `MQTT_DISCOVERY_HEAP_MIN` gates per call. |
| ADR-090 (re-entrancy guards) | Concern | Cross-task BLE access (1B-M2) is not the cooperative-loop model the ADR assumes. |
| ADR-092 (NimBLE adoption) | Pass | Status=Accepted, four verification gates discharged. |

## Critical issues for Phase 2 context

The two High items are not classical security/performance concerns but
warrant flagging:

1. **1A-H1** (bDiscoveryPublished sticky on failure) is functionally
   silent — Phase 2 performance pass should cross-check the heap-tier
   gates (ADR-089) for early-boot timing where MQTT is not yet
   connected when the first BLE scan lands.
2. **1B-H1** (platformio.ini reformat) is build-config drift; not
   security/performance per se, but the silent esptoolpy bump from
   `~1.30000.0` to `^2.41100.0` could surface as an upload-time
   regression on Windows/MinGW environments where the original pin
   was rationalised.

Phase 2 will additionally examine the cross-task BLE access (1B-M2)
through the concurrency-issues lens — there it could rise to a
performance/scalability concern.
