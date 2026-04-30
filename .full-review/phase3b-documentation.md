# Phase 3B: Documentation & API Review

## Summary

The session lands one new ADR (ADR-092 NimBLE adoption — accurate, all four
verification gates discharged), substantial inline header comments on every
non-trivial change in `SATble.ino` and `OTDirect.ino`, and a thorough
expansion of the parity fixture (`tests/otdirect_pic_parity_fixture.md`)
covering all TASK-466 / TASK-491 / TASK-494 behaviours. Inline documentation
is the strongest part of this session: each new control-flow step, each
type-conversion subtlety, and each cross-task synchronisation point carries
a "why" comment, not just a "what". The platformio.ini lib_deps addition is
also annotated with the right pointers (ADR-092, version-pin rationale).

The weak side is documentation that lives outside the source tree.
User-facing surfaces (CHANGELOG, MQTT topic reference, REST schema, WebUI
tooltip, MANUAL chapters) have not been updated to reflect (a) the new
per-MAC `sat/ble/<mac>/{temp,rh,bat,rssi}` topics, (b) the BLE HA-discovery
contract, (c) the change in `iBleInterval` semantics from "scan rate" to
"publish-rate" (the WebUI tooltip still actively misleads the user), and
(d) the TT/TC PIC-parity behaviour change (auto-clear, MsgID 100 flag
synthesis). The `c4-code-sat.md` and `c4-code-otdirect.md` code-level
documents have measurable drift from the new implementation. Nothing here
breaks shipping behaviour, but the next user reading the docs to integrate
or troubleshoot will hit a mismatch within the first BLE topic they
subscribe to.

## Critical findings

(none)

## High findings

### 3B-H1: Per-MAC BLE state and discovery topics undocumented in the public MQTT contract

**Where**:
- `docs/api/MQTT.md` — BLE Temperature Sensor section at lines 461-472
  lists only the legacy flat topics (`sat/ble_temp`, `sat/ble_humidity`,
  `sat/ble_sensor_rssi`, `sat/ble_battery`, `sat/ble_sensor_count`,
  `sat/ble_temp_valid`).
- The new per-slot topic family introduced by TASK-488 in
  `src/OTGW-firmware/MQTTstuff.ino:1958-1986` (`bleSensorPublishStateTopics`)
  publishes `sat/ble/<mac>/temp`, `sat/ble/<mac>/rh`, `sat/ble/<mac>/bat`,
  `sat/ble/<mac>/rssi` for every valid slot, and the helper
  `bleSensorPublishHaDiscovery` retains discovery configs at
  `<HaPrefix>/sensor/<uniqueId>_ble_<mac>_<kind>/config`.
- Neither family appears in `docs/api/MQTT.md`.

**Why this matters**: External consumers (Home Assistant, Node-RED,
custom dashboards) cannot reliably wire to the new contract without
inferring the topic shape from source code. HA users get the entities
auto-discovered, so they will not notice; everyone else (especially
non-HA users on Mosquitto + dashboards) will subscribe to the
documented `sat/ble_temp` and miss the per-MAC stream entirely.

**Recommendation**: Add a "Per-sensor topics (TASK-488)" subsection
under `docs/api/MQTT.md:461`. List the four state topics, the discovery
topic shape, the lowercase-no-colon MAC formatting rule
(`bleMacToCompact`), and the one-shot retain-config semantics
(`bDiscoveryPublished` per slot, retried on transient publish failure
per TASK-493). Reuse the table format already used for the legacy flat
topics.

### 3B-H2: `iBleInterval` semantics shift not reflected in the WebUI tooltip or MANUAL

**Where**:
- `src/OTGW-firmware/data/index.js:5833` — settings label literally
  "SAT BLE Scan Interval (sec)".
- `src/OTGW-firmware/data/index.js:5948` — tooltip "How often to scan
  for BLE sensor advertisements in seconds (10-300). Default 30 seconds.
  Lower values give faster updates but use more CPU."
- `docs/api/MQTT.md:809` — "BLE poll interval (s)".
- `SATtypes.h:414` was updated correctly: "Publish/state-update cadence
  (sec, 10-300). NOT scan rate: TASK-494 made the BLE scan continuous
  to match OT-Thing."

The WebUI is the user's primary configuration surface. After TASK-494
(commit `805c6728`) the BLE scan runs continuously from `satBLEInit()`
onward; `iBleInterval` only throttles how often `satBLEPublishMQTT()` /
`satBLEUpdateState()` snapshot the slots and push to MQTT. The user
reading the tooltip will assume that lowering the interval makes the
scanner more responsive — it does not (responsiveness is bound by the
BLE radio interval/window, set to 100/50 ms in `satBLEInit`). They will
also assume that raising the interval saves CPU/radio energy — it
does not (only the publish cost shrinks).

**Recommendation**: Update both strings in `index.js`. Suggested:
- Label: `"SAT BLE Publish Interval (sec)"`
- Tooltip: `"How often to publish BLE sensor data to MQTT and refresh
  SAT room temperature state (10-300 s, default 30). The BLE scan runs
  continuously; this interval only paces publishes."`
- Same fix in `docs/api/MQTT.md:809`: change "BLE poll interval" to
  "BLE publish interval".

The inline comment in `SATtypes.h:414` is sufficient as developer doc;
the WebUI string is what the end user reads.

### 3B-H3: CHANGELOG `[Unreleased]` does not reflect the session

**Where**: `CHANGELOG.md:6-17`. The Unreleased block currently lists
TASK-432 (WiFi DHCP) and TASK-478 (MQTT publish gating) ports. None of
the eight session commits between `ace21a48` and `a61373b9` are
mentioned.

What is missing:
- TASK-466 — TT=/TC= PIC-parity remote-override + MsgID 100 state
  machine (user-visible behaviour change: TT now auto-clears, TC writes
  MsgID 100=0x01, both push a clearing MsgID 100=0 frame on TT=0/TC=0).
- TASK-487 — NimBLE port (~400 KB flash freed, no more 3-second loop()
  block, ADR-092 reference).
- TASK-488 — HA auto-discovery for BLE sensors + per-MAC topic family
  (the High finding above is a consequence of this also being missing
  here).
- TASK-489-497 — combined "code-review follow-ups" line is fine for
  these as a group.
- TASK-494 — `iBleInterval` semantics shift to publish-rate (this is
  the migration note — see Medium 3B-M3).

**Why this matters**: TT/TC behaviour change in particular is a user-
visible contract change. A 1.5.x user upgrading to 2.0.0 issuing `TT=`
via MQTT and not seeing it auto-clear would file it as a regression
unless the changelog explains the new state-machine arming.

**Recommendation**: Add a section under `[Unreleased]` covering each
of these. Two paragraphs total; the bulk of the explanatory material is
already in the backlog `final-summary` and the parity-fixture rows, so
copy-paste-and-trim is sufficient.

## Medium findings

### 3B-M1: `c4-code-otdirect.md` describes the pre-TASK-466 TT/TC mapping

**Where**: `docs/c4/c4-code-otdirect.md`
- Line 181-182: `CS=xx.x` / `TC=xx.x` listed together as "Control
  setpoint (MsgID 1, TSet)". This is doubly wrong now: TC was moved to
  MsgID 16 by TASK-443, and TASK-466 added the MsgID 100 flag synthesis
  on top.
- Line 491-497: `OTFrameOverride` documented as "7 entries: MsgID
  1,7,8,14,16,56,57". TASK-466 added MsgID 100 to `otOverrides[]`
  (`OTDirect.ino:362` in the diff), so the array now has 8 entries.
- Lines 537-544: command queue documented as a plain ring buffer with
  no mention of TASK-494 coalesce-by-MsgID semantics or the high-water
  diagnostic.
- Line 680: "Command Queue: 12-frame limit; drops commands if full
  (logs error)." — true but missing the coalesce path that prevents
  growth in the common case.

**Recommendation**: Three small edits:
1. Split the `CS=` and `TC=`/`TT=` rows in Category 1. Add a new row
   for the TT/TC state machine with a one-line pointer to the parity
   fixture.
2. Update the override-table size from 7 to 8 with MsgID 100 listed.
3. Add a paragraph under "Command Queue" describing the coalesce-by-
   MsgID behaviour, the high-water counter, and the "ADR-064 sizing
   rationale" comment block (which is already in the source from
   TASK-494).

### 3B-M2: `c4-code-sat.md` cites `BLEDevice.h` as an external dependency

**Where**: `docs/c4/c4-code-sat.md:160` lists external dependencies for
the SAT module: `BLEDevice.h (ESP32 BLE scanning)`. ADR-092 replaced
that with `<NimBLEDevice.h>` (`SATble.ino:26`).

**Recommendation**: Change to `NimBLEDevice.h (NimBLE-Arduino 2.x, ADR-092)`.
Same file, same line.

While editing: the file lists only `parseBLEAtcFormat` and
`parseBLEBTHomeFormat` under the SATble.ino key-functions section
(lines 138-142). The session added `bleFindOrAllocSlot`,
`bleMatchesConfiguredMAC`, `SATBLEScanCallbacks::onResult`,
`satBLEPublishMQTT`'s per-MAC publish loop, `satBLEUpdateState`'s
multi-slot scan, plus the `_bleSensorsMux` cross-task synchronisation.
None of this needs full prose, but the new helpers (`bleMacToCompact`
exported from MQTTstuff for SATble's use, `bleSensorPublishHaDiscovery`,
`bleSensorPublishStateTopics`) and the cross-task model deserve a
one-line entry each.

### 3B-M3: ADR-077 conformance comment in MQTTstuff.ino contradicts the function-level doc

**Where**: `src/OTGW-firmware/MQTTstuff.ino`
- Block-level comment at lines 1925-1928: "ADR-077 conformance: HA
  discovery configs are emitted via the existing chunked streaming
  primitives (beginMqttPublish + writeMqttChunk + endPublish) — same
  heap-safe two-pass shape ADR-077 requires."
- Function-level comment at lines 2001-2006 (TASK-490 fix): "Single-
  buffer publish via the streaming primitives... so one stack-local
  allocation suffices and we skip the two-pass MEASURE-then-WRITE
  dance ADR-077 prescribes for unbounded payloads."

The block-level comment was authored before TASK-490 and was written
to claim full ADR-077 chunked compliance. TASK-490 honestly corrected
the function-level comment but did not propagate the correction to
the block-level overview that introduces the helpers. The block now
overstates the conformance: a reader skimming from top to bottom sees
"same two-pass shape ADR-077 requires" first and the contradicting
"we skip the two-pass dance" only inside the function body 75 lines
later.

This is exactly the gap Phase 1B-M1 flagged ("ADR-077 not amended to
whitelist the bounded-payload exception"). The fix here is small.

**Recommendation**: Edit `MQTTstuff.ino:1925-1928` to read approximately:
```
ADR-077 conformance: HA discovery configs are emitted via the existing
streaming primitives (beginMqttPublish + writeMqttChunk + endPublish).
For BLE configs we use a single-buffer publish (bounded ~768 B per
config) rather than the two-pass MEASURE-then-WRITE shape ADR-077
prescribes for unbounded payloads — see the per-function comment on
bleSensorPublishOneDiscovery for the rationale. canPublishMQTT() and
MQTT_DISCOVERY_HEAP_MIN gate every publish so heap pressure transparently
defers via the existing tier machine.
```

This also closes Phase 1B-M1 by either pinning the bounded-payload
exception in the source (cheap) or amending ADR-077 with a
"bounded-payload single-buffer is acceptable" clause (more thorough).
Source-comment fix is the YAGNI choice.

### 3B-M4: SAT C4 component diagram does not surface the cross-task BLE model

**Where**: `docs/c4/c4-component-smart-thermostat.md:114-138` (the
mermaid C4Component diagram).

The session introduced a real architectural shift: `SATBLEScanCallbacks::onResult`
runs on the NimBLE host's FreeRTOS task (core 0 on ESP32-S3) and writes
the shared `_bleSensors[]` slot table; the loop task (core 1) reads it
in `satBLEPublishMQTT` / `satBLEUpdateState`. TASK-497 added a `portMUX`
critical-section pair to serialise. This is a structural property the
component diagram should show: the BLE node is a separate concurrent
actor, not a passive "Bluetooth Sensor" external system.

The current diagram uses `System_Ext(blesensor, ...)` and `Rel(sat,
blesensor, "Passive BLE scan", "Bluetooth LE (ESP32 only)")`. That
made sense pre-NimBLE. Post-NimBLE it elides the "BLE host task on
core 0 writes shared state under a portMUX" detail.

The same omission exists at component-level too: `c4-component.md`
and `c4-container.md` do not call out FreeRTOS task boundaries
anywhere in the firmware. This is fine as a project-wide simplification
(the cooperative loop model is the dominant truth on ESP8266 and on
most ESP32 paths), but the BLE subsystem is now the single counter-
example and should at minimum carry a note.

**Recommendation**: Two small additions:
1. In `c4-component-smart-thermostat.md`, add a one-paragraph note in
   the BLE feature description explaining "advertisement parsing runs
   on the NimBLE host FreeRTOS task; slot writes are serialised
   against loop-task reads via a portMUX critical section."
2. In the mermaid diagram, label the `Rel(sat, blesensor, ...)` line
   as "Cross-task (FreeRTOS), portMUX-serialised" or split out a
   `Component(blescan, "BLE Scan Task", "FreeRTOS / NimBLE", ...)` if
   you want full visibility. The minimal one-line label is enough.

If you do not want any cross-task surfacing in C4 (it is a deliberate
simplification), at least update `c4-code-sat.md` (per 3B-M2 above)
to reference `_bleSensorsMux` and the snapshot-under-lock pattern.

### 3B-M5: SAT REST `/api/v2/sat/status` schema in ch09 omits the `ble_*` fields

**Where**: `docs/manuals/en/ch09-api-reference.md:454-478` describes
the SAT status endpoint with an abbreviated example. The example does
not include any of the fields emitted by `satBLESendStatusJSON()`:
`ble_enable`, `ble_temp_valid`, `ble_temp`, `ble_humidity`, `ble_rssi`,
`ble_battery`, `ble_sensor_count`, `ble_mac`. Pre-session this was
already incomplete; the session did not regress it but did not fix it
either. Mentioning it here because the BLE fields are now the headline
ESP32 capability and the documented schema is what HA developers and
Node-RED users will trust.

**Recommendation**: Append the BLE fields to the response example
(under a `// ESP32 only` comment or in a separate sub-block). This is
a doc sweep item rather than session-blocking, but is in scope of the
2.0.0 release because the BLE feature pack lands now.

## Low findings

### 3B-L1: "one BLE scan per iBleInterval" wording in the MQTTstuff block comment

**Where**: `MQTTstuff.ino:1929-1930`: "Drip pacing is provided by the
caller cadence (one BLE scan per iBleInterval, typically 30s)..."

After TASK-494 the scan is continuous. The drip-pacing rationale is
still correct (the publish loop runs once per `iBleInterval`), but the
phrasing "one BLE scan per iBleInterval" describes the pre-TASK-494
shape. Trivial fix: "one publish cycle per iBleInterval" or "the
satBLEPublishMQTT cadence (every iBleInterval seconds, typically 30s,
post-TASK-494 the BLE scan itself is continuous)".

Same issue in `MQTTstuff.ino:2087-2089`: "per-scan caller cadence
(iBleInterval, typically 30s)" — "scan" should be "publish-cycle".

### 3B-L2: ADR-077 not amended to whitelist bounded-payload single-buffer exception

**Where**: `docs/adr/ADR-077-streaming-mqtt-ha-discovery-architecture.md`
(not modified in this session).

ADR-077 says "chunked streaming, no full-message buffer". The BLE
discovery helper deliberately uses a 768-byte stack buffer because
the payload is bounded. TASK-490 documented this in the source
comment. ADR-077 itself was not amended to recognise the exception.
This means the next ADR-077 reader sees only the strict rule and
would either flag the BLE helper as a violation or silently introduce
ADR-conflicting code elsewhere believing the rule is absolute.

**Recommendation** (defer-able): Add a one-paragraph "Bounded-payload
exception" section to ADR-077, or reference TASK-490 in the
"Consequences" section. This can wait for the next ADR sweep; it is
not a session blocker.

### 3B-L3: ADR-092 omits the high-water and coalesce diagnostics

**Where**: `docs/adr/ADR-092-...md` is well-structured for the four
verification gates, but its "Specific implementation decisions" list
(lines 44-65) was written for TASK-487/488 only. It does not mention
TASK-494's continuous-scan model (which fundamentally changes the
"Periodic-scan model retained" bullet at line 53-55) or the cross-task
portMUX added by TASK-497.

This is not strictly inaccurate — TASK-494 and TASK-497 landed AFTER
ADR-092 was written, and ADR-092 covers the "adopt NimBLE" decision,
not the subsequent tuning. But a reader treating ADR-092 as the
canonical record of the BLE subsystem will miss two important
behaviours. The "Periodic-scan model retained" bullet is now
factually wrong: TASK-494 replaced it with continuous-scan.

**Recommendation**: Either:
(a) Add an "Amendments" section at the bottom of ADR-092 noting that
TASK-494 superseded the periodic-scan model with continuous-scan and
TASK-497 added the cross-task portMUX. Simpler.
(b) Edit line 53-55 in place to say "Continuous-scan model: the scan
is started once at `satBLEInit()` and runs forever; the user-tunable
`iBleInterval` paces publishes, not scans." Cleaner.

ADR-092's status is `Accepted` (line 3), and project policy is that
accepted ADRs are immutable except via supersession. Amendment-section
is the conformant choice.

### 3B-L4: Hardware setup chapter mentions BLE radio but not BLE-sensor pairing

**Where**: `docs/manuals/en/ch02-hardware-setup.md:40` lists "Bluetooth
LE radio for BLE temperature sensors (Xiaomi LYWSD03MMC)" as an OTGW32
hardware feature. The chapter does not point readers to the SAT BLE
configuration (`settings.sat.bBleEnable`, `sBleMAC`, `iBleInterval`)
or the supported sensor formats (ATC/pvvx + BTHome v2).

**Recommendation** (defer-able to the next docs sweep): Add a single
sentence pointing readers to chapter 5 (SAT) BLE section, OR add a new
short BLE-pairing subsection in chapter 5. Strict YAGNI says skip it
until a user reports confusion; pragmatic says add the cross-reference
because the new per-MAC topics make BLE a more visible capability now.

### 3B-L5: `c4-component-integration-layer.md` HA discovery subsection not updated

**Where**: `docs/c4/c4-component-integration-layer.md:77-84`. Lists two
operations: `doAutoConfigure()` and `doAutoConfigureMsgid()`. The session
added `bleSensorPublishHaDiscovery()` for BLE sensors that publish
4× retained config per MAC, with one-shot per-MAC tracking via
`bDiscoveryPublished`.

**Recommendation**: Add a third bullet:
- `bleSensorPublishHaDiscovery(macCompact, macWithColons)` — emit 4
  retained discovery configs (temperature, humidity, battery, signal
  strength) for one BLE sensor MAC; one-shot per MAC with retry on
  transient publish failure (TASK-488/493, ESP32 only).

## Strengths observed

- **Inline header comments are honest about WHY**, not just WHAT.
  Examples worth highlighting:
  - `OTDirect.ino` `setRemoteOverride` (around line 1944) explains
    the f8.8 narrow-cast UB before showing the clamp — exactly the
    "fix the doc, not the identifier" / explanatory-first style the
    project prefers.
  - `OTDirect.ino` `onThermostatMsgID16` documents the sign-extension
    cast with a worked example (-5 °C raw 0xFB00 → +64256 without the
    cast). A future reader who sees the `(int16_t)` hop will not delete
    it, which is the entire point of TASK-491.
  - `SATble.ino:71-81` (TASK-497) documents the cross-task model
    explicitly: BLE host task on core 0, loop task on core 1, portMUX
    is the synchronisation. Bounded critical sections are called out.
  - `SATble.ino:303-310` for the continuous-scan model includes the
    pre-TASK-494 shape ("30-s startup blackout plus 90% off-time")
    so the maintainer understands not just what is, but what was and
    why it changed.
- **`tests/otdirect_pic_parity_fixture.md` is a model of how to
  document state-machine semantics**: 9 new rows cover every TT/TC
  scenario including reboot, replacement, queue-coalesce, and negative
  TrSet. Each row carries a citation back to `gateway.asm` where
  applicable. The "Independence from TASK-442 (CS/C2 expiry)" footnote
  is exactly the kind of cross-state-machine call-out the implementor
  thinks about and the reviewer needs to read.
- **ADR-092** discharges all four verification gates explicitly
  (lines 113-131) and pins the version (`^2.1.0`) with the future-
  major-bump-requires-new-ADR clause. This is the disciplined ADR
  shape that ADR-080 enforces.
- **Backlog hygiene is exemplary**. Each TASK has a final-summary that
  reads as a release note. Anyone running `git log` plus `backlog task
  view <id>` for the IDs in the commit messages can reconstruct the
  full chain of decisions. The CHANGELOG gap (3B-H3) is the missing
  step that turns this internal trail into a user-facing one.
- **`platformio.ini` lib_deps comment block** (commit `59b1478d`,
  lines 160-165) carries the ADR pointer, the flash-savings number,
  and the version-pin rationale. After the Phase 1B-H1 reformat
  flagged the loss of build-rationale prose elsewhere, this one new
  block is a bright spot — it shows the right pattern even where the
  rest of the file was thinned out.

## Suggested deferrals

- **3B-L2** (ADR-077 amendment for bounded-payload exception). The
  source-comment fix in 3B-M3 is enough for now; the formal ADR
  amendment can wait for the next ADR sweep, since `bleSensorPublishOneDiscovery`
  is the only current bounded-payload site. If a second site lands,
  promote this to a Medium and amend the ADR.
- **3B-L4** (BLE-sensor pairing in hardware-setup chapter). User-facing
  but no field reports yet. Defer to the next manual revision unless
  a release-blocking review pulls it forward.
- **3B-L5** (integration-layer C4 third bullet). Cosmetic; the
  function is forward-declared in `OTGW-firmware.h` and visible in
  the source, so the C4 doc gap is documentation-only.
- **3B-M5** (REST `/api/v2/sat/status` schema BLE fields in ch09). The
  BLE state JSON serialiser already emits the fields; consumers using
  the live response will see them. The doc gap is a sweep item, not a
  contract regression.
- **ADR-092 amendment for TASK-494/497** (3B-L3). Useful but not
  blocking; the ADR's primary purpose (justify the dependency
  swap) is served. Amendment can land alongside the next BLE-related
  task.

The Highs (3B-H1, 3B-H2, 3B-H3) and the four Mediums (3B-M1, 3B-M2,
3B-M3, 3B-M4) should be addressed before tagging 2.0.0 because each
either misleads the user about user-visible behaviour (H2, H3, M3) or
breaks the documentation-as-source-of-truth contract for integrators
(H1, M1, M2, M4). All seven are small edits — none requires more than
a paragraph of new text and a couple of edits to existing files.
