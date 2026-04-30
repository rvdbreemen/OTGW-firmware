# Phase 2: Security & Performance Review

Consolidated from `phase2a-security.md` (`comprehensive-review:security-auditor`)
and `phase2b-performance.md` (`comprehensive-review:code-reviewer` in
performance-engineer mode).

## Security Findings (Phase 2A)

**Counts**: 0 Critical, 1 High, 3 Medium, 2 Low.

### High

- **2A-H1**: `setRemoteOverride()` in
  `src/OTGW-firmware/OTDirect.ino:1888-1889` performs
  `(int16_t)(celsius * 256.0f)` with no range clamp on `celsius`.
  For inputs outside roughly `-128..127 °C` this is C/C++
  **undefined behaviour** (the float-to-signed-narrow cast). All
  three command-entry paths reach this — REST `/api/v2/command`,
  MQTT raw command, telnet — on the trusted LAN. The sibling
  `otdMqttSetRoomSetpoint()` already has the right pattern
  (`-40..127` clamp); TT/TC silently skips it. Fix: 3-line range
  guard inside `setRemoteOverride()` mirroring the existing MQTT
  helper. Closes 1A-M3 too.

### Medium

- **2A-M1**: NimBLE 2.x scan callback runs on a separate FreeRTOS
  task and writes `_bleSensors[]` while the loop task reads it
  with no synchronisation. Same defect as 1B-M2 (architecture)
  and 2B-M2 (performance), now from the radio-driven attack
  angle: a hostile broadcaster in radio range can write to
  the slot at any moment, including mid-read by SAT's control
  loop. Real-world consequence is bounded (4 slots, 32-bit
  fields), but the contract is wrong.
- **2A-M2**: Default-allow MAC filter — when `settings.sat.sBleMAC`
  is empty, ANY sensor in radio range that emits a parseable BTHome
  v2 / ATC payload feeds SAT's input. A neighbour with a Xiaomi
  sensor on the same encryption-free firmware can spoof room
  temperature readings (15-25 °C is the parser's accept band, easy
  to forge). Recommend either (a) defaulting to a single-MAC pin
  with explicit "accept-all" override, or (b) requiring at minimum
  a configured RSSI floor.
- **2A-M3**: BTHome v2 parser at `SATble.ino:120-148` returns on
  the first unknown object ID — robust-by-truncation. Spec-compliant
  devices that emit a `0x00` packet-ID byte before temperature
  would silently drop the temperature reading. Fix: known
  variable-length ID lookup table or a least-restrictive skip-byte
  policy.

### Low

- **2A-L1**: MsgID 100 reserved-bits-zero handling looks correct
  but should be documented in the parity fixture for audit trail.
- **2A-L2**: `bleSensorPublishOneDiscovery`'s 768-byte truncation
  path is currently safe; would become risky if ADR-077's cap is
  ever lifted.

### Out-of-scope notes

- HTTPS / WSS / TLS not assessed (project posture).
- WiFi credential handling not assessed (not in this diff).

## Performance Findings (Phase 2B)

**Counts**: 0 Critical, 1 High, 3 Medium, 6 Low.

### High

- **2B-H1**: `feedWatchDog()` is only called on the success path
  inside `bleSensorPublishOneDiscovery()` and the four-publish loop
  inside `bleSensorPublishHaDiscovery()`. A string of broker
  failures across the 16-publish first-scan burst can traverse all
  four nested helpers without a single watchdog kick while four
  PubSubClient sockets each work through their default 15-second
  timeout. ESP32 HW watchdog is generous enough that this has not
  bitten yet, but the pattern needs `feedWatchDog()` before every
  `return false` that follows a network attempt. Trivial one-liner
  fix that also closes 1A-M4 (incomplete `endPublish` cleanup on
  error path).

### Medium

- **2B-M1**: Worst-case 32 sequential MQTT publishes on the first
  `satBLEPublishMQTT` after MQTT-connect (4 slots × (4 discovery
  retained + 4 state)). Gated by `canPublishMQTT()` and
  `MQTT_DISCOVERY_HEAP_MIN`, so backpressure works, but combined
  with 2B-H1 the burst is unwatchdogged.
- **2B-M2**: Cross-task `_bleSensors[]` access without lock.
  Performance impact is genuinely sub-microsecond on the
  ESP32-S3's coherent caches; the architectural concern (1B-M2)
  and the security concern (2A-M1) are the real angles, not
  performance. Recommend leaving the data race alone purely from
  performance perspective.
- **2B-M3**: OT command queue depth (12 entries) stays in budget
  for the new TT/TC sequences (2 frames per `setRemoteOverride`,
  1 for `clearRemoteOverride`) but is borderline if any other
  caller stacks up commands in the same tick. Recommend documenting
  the worst-case stack-up in the OTDirect ADR (currently ADR-064
  / ADR-087).

### Low

- 2B-L1..L6: minor allocation, comment, and documentation
  observations. See full report.

## Strengths observed

- NimBLE 2.x port eliminates per-advertisement Arduino-`String`
  churn — the biggest pre-session heap-fragmentation source.
- Async 3-arg `start()` reclaims roughly 10% of
  `doBackgroundTasks()` wall-clock that the synchronous Bluedroid
  scan was blocking.
- OTDirect TT/TC state machine is `O(1)` per frame and
  `OTRemoteOverrideState` is a single cache line (16 bytes
  packed). No allocation pressure.
- `evaluate.py` regex changes are linear; no ReDoS risk.
- NimBLE-Arduino 2.5.0 has no known CVEs at audit time.

## Cross-phase signal

The cross-task `_bleSensors[]` access surfaced from THREE
independent angles:

| Phase | ID | Angle |
|---|---|---|
| 1B | 1B-M2 | architecture: ADR-090 cooperative-loop assumption violated |
| 2A | 2A-M1 | security: radio-driven attacker writes during loop-task read |
| 2B | 2B-M2 | performance: real impact sub-microsecond, leave it |

When the same defect lights up in three orthogonal reviews, it is
a real concern. The verdict combining all three: **add a critical
section** around the slot writes in the scan callback, narrow the
data race surface. Implementation cost is small (one
`portMUX_TYPE` and a pair of `portENTER_CRITICAL` / `EXIT`).

## Critical issues for Phase 3 context

The findings that warrant test/docs follow-through in Phase 3:

1. **2A-H1** (TT/TC unclamped celsius): needs a fixture row in
   `tests/otdirect_pic_parity_fixture.md` for the boundary-value
   case (e.g. `TT=200`, `TT=-200`).
2. **2B-H1** (watchdog on failure path): needs a code path covered
   by a soak test, but no automated harness exists in this project
   for that — document in the test plan as owner-only.
3. **2A-M2** (default-allow MAC filter): needs a doc line in the
   SAT BLE settings page warning about radio-range trust
   assumptions, OR a settings UI tweak to require an explicit
   "accept any sensor" toggle.
4. **2A-M3** (BTHome parser unknown-object truncation): needs a
   fixture row with a packet-ID-prefixed payload to reproduce the
   silent-drop case.
5. **1B-H1** (platformio.ini reformat) and **1B-M4** (scope
   manifest error): docs / build-config issues; should surface
   in Phase 4's CI/DevOps review.
