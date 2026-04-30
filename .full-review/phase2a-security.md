# Phase 2A: Security Audit

## Summary

Net security posture of the in-scope changes is **good** for the project's
trusted-LAN appliance threat model. The two new untrusted-input parsers
(BLE ATC and BTHome v2 in `SATble.ino`) have sound bounds checks, sanity
ranges on temperature/humidity, and explicitly reject encrypted BTHome
advertisements. The new MQTT-publishing helpers in `MQTTstuff.ino` build
all topics and JSON payloads from values that are either MAC strings
already validated to `[a-f0-9:]` by `bleMacToCompact` or come from
admin-configured settings. NimBLE-Arduino is used in scan-only / passive
mode (no advertising, no GATT server, no pairing), so the BLE attack
surface is bounded to advertisement parsing.

The most notable gap is in OpenTherm frame synthesis: `setRemoteOverride()`
casts `(float celsius * 256.0f)` to `int16_t` with **no input clamping**
when reached via the local TT/TC command path (REST `/api/v2/command`,
MQTT raw command, telnet). For values outside roughly -128..127 °C the
cast is undefined behaviour per C/C++ spec and emits an unpredictable
f8.8 value onto the OT bus. The MQTT helper `otdMqttSetRoomSetpoint`
already clamps to -40..127 °C; that same range needs to apply to the
TT/TC code path. This is Medium because the OT bus is local hardware
and the device is LAN-trusted, but it's a free fix.

No Critical findings. One High (UB on float-to-int cast in OT frame
synth), three Medium, two Low. NimBLE-Arduino 2.5.0 (the resolved
version of `^2.1.0`) has no public CVEs at the time of audit; the
passive-scan posture would also limit exposure if one surfaced.

## Critical findings

None.

## High findings

### H1: Unclamped float-to-int cast in `setRemoteOverride()` (UB on overflow)

- **Severity**: High (CVSS 4.4 — AV:A / AC:L / PR:L / UI:N / S:U / C:N / I:L / A:L)
- **CWE**: CWE-681 (Incorrect Conversion between Numeric Types), CWE-190 (Integer Overflow)
- **File**: `src/OTGW-firmware/OTDirect.ino:1888-1889`
- **Code**:
  ```cpp
  static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius) {
    uint16_t f88 = (uint16_t)((int16_t)(celsius * 256.0f));
  ```
- **Attack scenario**: An attacker on the trusted LAN (or a compromised HA
  integration, or a misconfigured automation) sends `TT=200` or `TT=-200`
  via REST `/api/v2/command`, MQTT `<top>/set/raw`, or the telnet command
  console. The result `200 * 256 = 51200` exceeds `INT16_MAX`. Per C++17
  §7.10/3 ("If the destination type is signed, the value is unchanged if
  it can be represented in the destination type; otherwise, the value
  is implementation-defined") and C99 §6.3.1.4 (a float that cannot be
  represented as the destination integer type produces undefined
  behaviour). On Xtensa/ARM toolchains this typically produces an
  arbitrary 16-bit value, which then enters the OT bus as MsgID 16
  (TrSet). The OT thermostat or boiler will receive a malformed
  setpoint. Not a remote-code-execution path, but a way to push junk
  setpoints onto a heating system that is supposed to be honouring
  user intent.
- **Defence in depth**: `otdMqttSetRoomSetpoint()` at OTDirect.ino:1642
  already does this exact range check (`if (tempC < -40.0f || tempC >
  127.0f) return;`). The TT/TC handlers and `setRemoteOverride` should
  honour the same contract.
- **Fix**:
  ```cpp
  static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius) {
    // Defence in depth: clamp to the same OT-realistic range
    // otdMqttSetRoomSetpoint() uses. f8.8 cannot represent values
    // outside roughly -128..+127 anyway.
    if (celsius < -40.0f || celsius > 127.0f) {
      OTDDebugTf(PSTR("OTD: TT/TC %.2f out of range, ignored\r\n"), celsius);
      return;
    }
    uint16_t f88 = (uint16_t)((int16_t)(celsius * 256.0f));
    ...
  }
  ```
  Alternatively clamp at the parse site in `handleOTDirectCommand`
  alongside the existing `if (temp == 0.0f)` check; same effect, one
  centralised place either way.
- **Cross-references**: this is adjacent to but distinct from finding
  1A-M3 (TT=negative-temp slips past zero-clear). Phase 1 caught the
  semantic gap; this finding is the UB-cast layer underneath.

## Medium findings

### M1: NimBLE scan callback runs on a separate FreeRTOS task — `_bleSensors[]` is read/written without synchronisation

- **Severity**: Medium (CVSS 3.7 — torn read of metadata only)
- **CWE**: CWE-362 (Race Condition), CWE-366 (Race in Single-Threaded
  Resource)
- **File**: `src/OTGW-firmware/SATble.ino:67` (the static array),
  `194-269` (writer in scan callback), `332-375`, `405-449` (readers
  in `satBLEUpdateState`/`satBLEPublishMQTT`)
- **Attack scenario**: An attacker in radio range floods the device
  with crafted BTHome/ATC advertisements at high rate. NimBLE 2.x runs
  the scan callback on the BLE host FreeRTOS task; `satBLELoop()` and
  `satBLEPublishMQTT()` run on the Arduino loop task. There is no
  mutex, atomic, or `volatile` discipline on `_bleSensors[]`. A torn
  read of `iLastSeenMs` (32-bit on Xtensa, single instruction so
  likely safe) or `bDiscoveryPublished` (1-bit field, read-modify-write
  on adjacent struct fields could race) is theoretically possible.
  Worst case: a discovery config gets published twice for the same MAC,
  or a slot's `bValid` flag is read as true while `sMacAddress` is
  still being copied — leading to a malformed MAC string entering
  `bleMacToCompact`, which would defensively bail with empty output.
- **Real-world risk**: low. ESP32 32-bit aligned reads are atomic at
  the instruction level for the fields in question, and
  `bleSensorPublishHaDiscovery` checks `if (macCompact[0] == '\0')
  continue;`. But the **architectural assumption** that earlier
  cooperative-loop ADRs were built on (ADR-090 says "no volatile
  required if cooperative") is silently invalidated by the parallel-task
  model NimBLE 2.x introduces. This was already flagged as 1B-M2 in
  Phase 1 architecture review; reinforced here from a security angle:
  an attacker can drive the cross-task contention point with broadcast
  radio traffic.
- **Fix**: Document the cross-task access in ADR-092 (or amend ADR-090),
  and either (a) wrap `_bleSensors[]` access on the loop side in a
  short critical section (`portENTER_CRITICAL` / `portEXIT_CRITICAL`),
  or (b) snapshot the slot under critical section into a stack-local
  before publishing, releasing the lock before MQTT I/O. Approach (b)
  keeps lock hold-time bounded.

### M2: Default-allow MAC filter — empty `settings.sat.sBleMAC` accepts any sensor

- **Severity**: Medium (CVSS 3.1 — A:L only, integrity of sensor data)
- **CWE**: CWE-285 (Improper Authorization), CWE-1188 (Insecure Default
  Initial Value)
- **File**: `src/OTGW-firmware/SATble.ino:166-170`
- **Code**:
  ```cpp
  static bool bleMatchesConfiguredMAC(const char* mac)
  {
    if (settings.sat.sBleMAC[0] == '\0') return true;  // No filter: accept all
    return (strcasecmp(mac, settings.sat.sBleMAC) == 0);
  }
  ```
- **Attack scenario**: Out of the box, with no MAC configured, the
  device accepts any BTHome/ATC advertisement in range. An attacker
  in radio range can inject crafted temperature readings (within the
  -40..60 °C sanity band) and influence the SAT control loop's room
  temperature input. SAT consumes `state.sat.fBleTemp` as its room
  temperature for autotune and PID; bogus readings could push the
  heating up or down.
- **Real-world risk**: low to medium. Requires radio proximity, and
  the SAT algorithm has its own sanity behaviour. But the trust model
  here is "anyone in radio range" — which, unlike the trusted-LAN
  posture for IP, is *not* under the operator's control.
- **Fix**: Two layers of defence:
  1. Document in the user-facing setup that an explicit MAC is
     strongly recommended for production use.
  2. Consider tightening the default to "no sensor accepted until a
     MAC is configured" (current default is "accept all"). Trade-off:
     the current default is a UX win for first-boot ("plug in a sensor
     and it just works") and the fact that we use the *configured*
     MAC's data preferentially in `satBLEUpdateState` does mitigate.
     Recommend documenting rather than changing default behaviour.
- **Note**: This is a pre-existing pattern; the diff under audit did
  not introduce it but it is now load-bearing for the multi-sensor
  publish (TASK-488), since *every* discovered MAC now gets four
  retained MQTT topics + four HA-discovery configs published. An
  attacker spoofing many MACs would cause discovery-config spam in
  the broker (capped by `SAT_BLE_MAX_SENSORS = 4` slots).

### M3: BTHome v2 unknown-object-ID early termination silently drops valid trailing data

- **Severity**: Medium (CVSS 3.1 — Integrity:L)
- **CWE**: CWE-754 (Improper Check for Unusual or Exceptional
  Conditions), CWE-1287 (Improper Validation of Specified Type of
  Input)
- **File**: `src/OTGW-firmware/SATble.ino:153-155`
- **Code**:
  ```cpp
  default:
    // Unknown object: we cannot determine its length, so stop parsing
    return gotTemp;
  ```
- **Issue**: BTHome v2 uses TLV-style packed data with object IDs
  whose lengths are *defined per object ID in the spec* (e.g. 0x00
  = packet ID, 1 byte; 0x05 = illuminance, 3 bytes; 0x09 = count,
  1 byte). Real-world BTHome devices typically emit the packet-ID
  byte (0x00) **first**, then temperature, humidity, etc. The current
  parser bails on the first unknown object — meaning the very first
  byte after flags being a packet ID (object 0x00, length 1) terminates
  parsing before reading temperature. We currently happen to get
  lucky with sensors that put temp/humidity/battery first, but the
  spec does not guarantee that ordering.
- **Security angle**: an attacker who knows the parser stops at unknown
  IDs can craft an advertisement that includes a stub of recognised
  data followed by garbage past the unknown ID — the garbage is
  effectively skipped without validation. Not exploitable in itself
  (no buffer over-read; the loop exits cleanly), but it does mean
  the parser is robust-by-truncation rather than robust-by-parsing.
- **Fix**: Either (a) add a minimal length lookup for the most common
  BTHome object IDs (0x00, 0x01, 0x02, 0x03, 0x05, 0x09, 0x10, 0x11),
  or (b) consume known-safe object IDs and skip unknown ones with a
  conservative single-byte advance (will break payload sync but
  won't over-read since `pos < len` is the loop guard). Option (a)
  is preferable for correctness; option (b) for code minimality.

## Low findings

### L1: `flags` synthesis in MsgID 100 leaves upper byte at zero — ambiguous OT v4.2 reserved-bit handling

- **Severity**: Low (CVSS 2.0 — informational / spec compliance)
- **CWE**: CWE-1068 (Inconsistency Between Implementation and Documented
  Design)
- **File**: `src/OTGW-firmware/OTDirect.ino:1903`
- **Code**:
  ```cpp
  uint16_t flags = (mode == OT_OVERRIDE_TEMPORARY) ? 0x0002 : 0x0001;
  enqueueWriteCommand(100, flags, "OVR-flags");
  ```
- **Note**: OT v4.2 §6.3 defines MsgID 100 (RemoteOverrideFunction) low
  byte (HB1 in OT terms) as flag bits and the high byte as reserved.
  Setting reserved bits to zero is the conformant choice and matches
  the gateway.asm SetSetPoint reference. Comment in the diff explicitly
  states "high byte unused per OT v4.2", which is correct. No fix
  needed; flagging only because reserved-bit handling on a write to
  the OT bus is a thing reviewers should consciously verify.

### L2: `bleSensorPublishHaDiscovery` payload truncation silently produces malformed JSON

- **Severity**: Low (CVSS 2.6 — A:L, broker hygiene)
- **CWE**: CWE-209 (Generation of Error Message Containing Sensitive
  Information — partial), CWE-755 (Improper Handling of Exceptional
  Conditions)
- **File**: `src/OTGW-firmware/MQTTstuff.ino:1996-2000`
- **Issue**: If `snprintf_P` returns `n >= sizeof(payload)` the
  function returns false without publishing — that part is fine.
  But the truncation log message includes only `macCompact` and
  `kindKey`. If a future ADR-077 amendment relaxes the 768-byte
  cap (the comment explicitly says one might), an attacker who
  controls `settings.mqtt.sUniqueid` (admin-only via web UI) could
  push the payload over the cap and observe the log to confirm.
  Out of practical concern given ADR-056 admin auth is already in
  place, but worth a tracking task if the bound is ever lifted.
- **Fix**: When the cap is lifted, switch `bleSensorPublishOneDiscovery`
  to the same chunked MEASURE-then-WRITE pattern ADR-077 prescribes
  for unbounded payloads, removing the truncation path entirely.

## Out-of-scope notes

These were observed during the audit but are out of the threat model
or out of the diff window:

1. **HTTPS / WSS / TLS**: Per project posture, plain HTTP/MQTT on a
   trusted LAN. Not flagged.
2. **No authentication on the MQTT broker side**: the device's MQTT
   client supports `settings.mqtt.sUser` / `sPassword` which is
   admin-configurable; broker-side ACL is the operator's
   responsibility. Out of diff scope.
3. **Telnet debug stream on port 23 with no auth**: pre-existing,
   project posture, LAN trusted. Out of scope.
4. **`atof()` on user input**: `atof` returns 0.0f on parse error
   with no way to distinguish from a legitimate `0.0`. The TT/TC
   handler treats 0.0f as "clear override", so a malformed value
   like `TT=garbage` would clear an active override rather than
   error out. Existing project pattern (CS=, MM=, etc. all use
   `atof`); not introduced by this diff. Worth a future hardening
   task but out of scope here.
5. **NimBLE-Arduino 2.5.0**: no public CVEs at audit time. Project
   uses scan-only / passive mode, no GATT server, no pairing — so
   the typical NimBLE attack surface (pairing exchanges, GATT
   characteristic writes) is not exposed. Pin `^2.1.0` is acceptable;
   recommend pinning more tightly to `~2.5.0` once the 2.x line
   stabilises (separate ADR-092 follow-up, not a security concern).
6. **`evaluate.py` regex DoS**: all new patterns are linear with no
   nested quantifiers; no ReDoS risk. `Path.glob` follows symlinks
   by default but the helper is only called with a fixed
   `config.FIRMWARE_ROOT` from a CI checkout — no untrusted-path
   injection.
7. **`bleMacToCompact` malformed-input handling**: writes `out[0] =
   '\0'` on every error path; the index `o` is bounded to 12 by the
   loop structure (5 of 17 positions are colon-skips); cannot
   over-write the 13-byte caller buffer. Sound.
8. **Topic injection via `settings.mqtt.sHaprefix` containing `+`
   or `#`**: those are MQTT wildcard characters. An admin who
   configures a prefix like `home+assistant` would publish to a
   weird topic but not overlap a different client's subscription
   tree. Admin-controlled, not adversarial. Out of scope.
