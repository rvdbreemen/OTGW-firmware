# ADR-111 SAT MQTT publish — on-change + jittered heartbeat

## Status

Accepted, 2026-05-26

## Context

The SAT layer publishes ~74 topics under `sat/*` (control/PID/cycle/pressure/PV/thermal/weather/ble).
Prior to this ADR each field was published unconditionally on every control cycle (default 30s),
without any change-detection or coalescing.

Concrete evidence from the telnet debug log of `otgw-1020BA16C6BC` (firmware `2.0.0-alpha.71+d95caa1`,
captured 2026-05-26):

- ~74 publishes per 30s cycle in summer conditions where virtually no SAT field mutates
  (boiler idle, room 24.9°C > target 20.0°C, PID stable negative).
- Continuous retained publishes for configuration fields (`kp`, `ki`, `kd`, `deadband`,
  `coefficient`, `manufacturer`) that by definition do not change during runtime.
- Home Assistant history table polluted with identical samples; broker DB write load
  unnecessarily high for what is, in practice, a low-rate telemetry surface.

ADR-052 (`mqtt-publish-eligibility-contract`) already specifies an eligibility contract for
OpenTherm publishes — `firstSeen ∨ valueChanged ∨ stale-refresh` — but it is OT-specific
(`mqttlastsent[256]`, indexed by OT message ID, one byte of last-value memory per slot) and is
not used by SAT publishes. A SAT-specific contract is needed because SAT fields:

- are floats with per-field decimal precision (tolerance is required, not free)
- are not indexed by an OT message ID (60+ heterogeneous named fields)
- want a longer heartbeat cadence than OT's stale-refresh, to keep broker pressure low while
  still surfacing keep-alive for non-retained topics

### Constraints

- ESP8266 dual-target build: shadow memory MUST sit in BSS (no heap allocation), within the
  remaining DRAM budget. The 2.0.0 line still compiles for ESP8266 even though the production
  hardware is ESP32 (OTGW32).
- ESP32 single-loop hot path: the BLE FreeRTOS task (`SATble.ino:615-617`) already mutex-snapshots
  its state. SAT shadows do not need to be thread-safe because they are only touched from
  `doBackgroundTasks()` (the cooperative main loop).
- Re-entrancy: helpers may indirectly trigger `feedWatchDog()` via `sendMQTTData`. Shadow
  writes MUST happen *before* the publish call, otherwise a re-entered `doBackgroundTasks`
  could double-publish the same field.
- ADR-073 (SAT MQTT topic structure) remains in force: flat keys, retained vs non-retained
  policy per topic is unchanged by this ADR.
- ADR-101 (flat per-value topics over aggregated JSON) remains in force: an aggregated
  `sat/state` JSON blob is NOT a valid alternative.

## Decision

All SAT MQTT publishes — every topic under `sat/*` including `sat/weather/*` and `sat/ble/*` —
go through a shared helper pattern that publishes on **value change OR per-topic jittered
heartbeat**.

### Sub-rule 1: Helper API (pattern-level binding)

Four typed helpers in a new header `src/OTGW-firmware/SATmqttPublish.h`:

```cpp
struct SATShadowF { float    last; uint32_t nextRepublishMs; bool valid; };
struct SATShadowI { int32_t  last; uint32_t nextRepublishMs; bool valid; };
struct SATShadowB { int8_t   last; uint32_t nextRepublishMs; };  // -1 = invalid, 0/1 = bool value
struct SATShadowS { char     last[24]; uint32_t nextRepublishMs; bool valid; };

void publishIfChangedF(const __FlashStringHelper* topic, float current,
                       SATShadowF& shadow, float tolerance, uint8_t decimals, bool retained);
void publishIfChangedI(const __FlashStringHelper* topic, int32_t current,
                       SATShadowI& shadow, bool retained);
void publishIfChangedB(const __FlashStringHelper* topic, bool current,
                       SATShadowB& shadow, bool retained);
void publishIfChangedS(const __FlashStringHelper* topic, const char* current,
                       SATShadowS& shadow, bool retained);
```

Each call site declares a file-static shadow next to the publish-call. Shadows are zero-initialised
by BSS, which means `valid == false` and `nextRepublishMs == 0` at boot — the first invocation
seeds both fields.

### Sub-rule 2: Eligibility formula (pattern-level binding)

For every helper invocation the publish decision is:

```
publish = !shadow.valid                                           // first-seen
        ∨ value_differs(current, shadow.last, tolerance)          // change
        ∨ (int32_t)(millis() - shadow.nextRepublishMs) >= 0       // heartbeat due
```

`value_differs` is type-specific:

- Float: `fabsf(current - last) >= tolerance`
- Int / bool: strict `!=`
- String: `strncmp(current, last, sizeof(shadow.last)) != 0`

On publish (any of the three triggers fires):

```cpp
// shadow write happens BEFORE sendMQTTData to be re-entrancy safe
shadow.last = current;                       // or strlcpy for string
shadow.valid = true;
shadow.nextRepublishMs = millis() + satRandomHeartbeatMs();
sendMQTTData(topic, formattedBuf, retained);
```

On skip: shadow is left untouched.

The `(int32_t)(millis() - shadow.nextRepublishMs) >= 0` formulation is rollover-safe for any
scheduling window shorter than ~24 days, which is the standard Arduino timer idiom.

### Sub-rule 3: Heartbeat jitter (pattern-level binding)

Every publish resets the per-topic heartbeat to a uniformly random point in `[7min, 11min]`:

```cpp
static inline uint32_t satRandomHeartbeatMs() {
  // Uniform random in [7 minutes, 11 minutes] inclusive.
  constexpr uint32_t MIN_MS = 7  * 60 * 1000;  //  420 000
  constexpr uint32_t MAX_MS = 11 * 60 * 1000;  //  660 000
  uint32_t span = MAX_MS - MIN_MS;              //  240 000
#if defined(ESP32)
  return MIN_MS + (esp_random() % (span + 1));
#else
  return MIN_MS + (os_random() % (span + 1));
#endif
}
```

Boot-scatter — the very first heartbeat per topic must NOT fire all-at-once at `boot+7min`.
On the first invocation per shadow (`!shadow.valid`), the helper publishes immediately (that is
the first-seen branch) but ALSO seeds `nextRepublishMs = millis() + satRandomBootScatterMs()`
*for the next heartbeat*, where:

```cpp
static inline uint32_t satRandomBootScatterMs() {
  // Uniform random in [0, 11 minutes] — first heartbeat wave spread over 11 minutes.
  constexpr uint32_t MAX_MS = 11 * 60 * 1000;
#if defined(ESP32)
  return esp_random() % (MAX_MS + 1);
#else
  return os_random() % (MAX_MS + 1);
#endif
}
```

After the second publish (and forever after) the normal `satRandomHeartbeatMs()` is used.
This single boot-scatter step spreads the first heartbeat wave across the full 11-minute
window without any global coordination state — each topic schedules itself.

Modulo bias: `span = 240000` is much smaller than `UINT32_MAX`, so the bias is well below
1 ppm and is acceptable.

### Sub-rule 4: Tolerance table (guideline-level)

Tolerance per float field is **explicit at the call site**, not derived from format decimals.
Recommended constants live next to the helpers in `SATmqttPublish.h`:

```cpp
constexpr float SAT_EPS_TEMP        = 0.05f;     // °C  — setpoint, target, room_temp, outside_temp
constexpr float SAT_EPS_TEMP_COARSE = 0.1f;      // °C  — heating_curve, thermal_drop_rate display
constexpr float SAT_EPS_PID_OUTPUT  = 0.5f;      // °C  — pid_output (derivative jitter)
constexpr float SAT_EPS_PID_TERM    = 0.1f;      // °C  — pid_p, pid_i, pid_d
constexpr float SAT_EPS_ERROR       = 0.05f;     // °C
constexpr float SAT_EPS_KP          = 0.01f;
constexpr float SAT_EPS_KI          = 0.00001f;
constexpr float SAT_EPS_KD          = 0.1f;
constexpr float SAT_EPS_DERIVATIVE  = 0.0005f;
constexpr float SAT_EPS_FRACTION    = 0.005f;    // duty_ratio, overshoot_fraction, underheat_fraction
constexpr float SAT_EPS_PRESSURE    = 0.02f;     // bar
constexpr float SAT_EPS_POWER       = 0.05f;     // kW
constexpr float SAT_EPS_ENERGY      = 0.005f;    // kWh (3 decimals)
constexpr float SAT_EPS_PV_W        = 25.0f;     // W   — not hyper-precise
constexpr float SAT_EPS_DURATION    = 1.0f;      // seconds
```

These values are tunable from field observation. They are guideline-level: an off-by-one in
either direction is not an ADR violation as long as the helper pattern is used.

### Sub-rule 5: MQTT reconnect behaviour (pattern-level binding)

On MQTT reconnect the SAT shadows are **NOT** reset.

- Retained topics survive on the broker — HA sees the last value immediately on reconnect.
- Non-retained topics stay silent until their next scheduled heartbeat (≤ 11 minutes).

Rationale: minimal extra traffic, the heartbeat covers it within the heartbeat window, and the
4-minute jitter band guarantees the post-reconnect re-publish wave is itself spread out rather
than colliding into a single burst.

This is an explicit deviation from ADR-052 (which DOES reset OT publish-state on reconnect),
justified by SAT having a much longer heartbeat (7-11 min vs OT's much tighter stale-refresh)
and a much higher topic count (74 vs ~17 status sub-topics).

### Sub-rule 6: Scope boundary

Helpers are used by all SAT telemetry publishers:

- `SATcontrol.ino` — `satPublishMQTT()` (all ~50 PID/cycle/pressure/PV/thermal topics)
- `SATpressure.ino` — `satPressureHealthPublish()` (pressure-health topics)
- `SATble.ino` — `satBLEPublishStateTopics()` (BLE sensor topics, ESP32 only)
- `SATweather.ino` — `weatherPublishMQTT()` (13 weather topics)

**Out of scope** (unchanged by this ADR):

- Command topics (`set/<nodeId>/sat/*`) — input, no telemetry contract applies.
- HA discovery configs (`homeassistant/.../config`) — own lifecycle, see ADR-077.
- `sat/target` via `satHandleTargetTemp` (REST-write echo) — already event-driven; this ADR
  does not change that path. Only the periodic `satPublishMQTT()` paths are migrated.
- Ad-hoc debug/error publishes (one-shot per event) — no heartbeat is meaningful.

## Alternatives Considered

### Alternative 1: Generic hash-table shadow (topic → last value)

A global `std::map<String, String>` (or linked list of shadow entries) keyed by topic.

Rejected. Heap allocations fragment ESP8266 RAM (ADR-004 / ADR-053). Topic-string lookups cost
CPU per publish. Worst of all, there is no typed comparison: everything would compare on the
formatted output string, so PID-derivative numeric jitter would still cause publishes because
the formatted string changes by one digit. The typed helpers in sub-rule 1 fix this at the
type-level.

### Alternative 2: Single `sat/state` JSON blob

Aggregate all SAT fields into one JSON payload, publish on change-of-anything.

Rejected. Violates ADR-101 (flat per-value topics) and ADR-073 (SAT topic structure). Breaks
HA auto-discovery which subscribes one sensor per topic. Also kills granularity: any field
changing forces re-publish of the whole document. We would trade publish-count for
publish-byte-count, with no clear win.

### Alternative 3: Reuse ADR-052 / `mqttlastsent[]` for SAT

Extend the OT eligibility contract — and its 256-byte shadow array — to SAT.

Rejected. `mqttlastsent[]` is indexed by OT message ID (0-255). SAT has no comparable discrete
ID space; its 60+ heterogeneous fields would need a parallel name-to-index mapping. The 1-byte
last-value granularity is far too coarse for floats. SAT deserves its own contract that knows
about float tolerances natively.

### Alternative 4: Fixed heartbeat (no jitter, e.g. exactly 10 min)

Every topic refreshes on a 10-minute deadline, no random component.

Rejected. Thundering herd: all 74 topics in one `doBackgroundTasks` tick would burst ~30 ms of
publishes (we already know from ADR-088 that msgid 0 fan-out of 17 publishes takes ~20 ms and
can push the heap into the WARNING tier). A 4-minute jitter window spreads that load across
hundreds of ticks and keeps the average publish rate below 1/sec.

## Consequences

### Positive

- Broker load is expected to drop ~85-95% in steady state (from ~74/cycle to 0-10/cycle).
- HA history table contains only meaningful samples (value changes + 7-11 min keep-alive).
- Publish code becomes typed and uniform: every publish flows through the same four helpers,
  trivial to audit, easy to add new topics correctly.
- Per-field float tolerance is explicit and tunable; PID derivative jitter no longer causes
  false-positive publishes.
- Memory cost is predictable and BSS-allocated (see Memory budget below).

### Trade-offs

- One-time refactor of ~74 call sites in 4 files. Mechanical work, low semantic risk per site.
- Every new SAT topic must carry a shadow struct — one extra line at the declaration, no extra
  line at the call site (the helper takes the shadow by reference).
- Non-retained topics stay silent for up to 11 minutes after an MQTT reconnect (the heartbeat
  window). Deliberate trade — HA will show the last-known value or "unavailable" depending on
  the entity's `availability_topic` configuration. Retained topics (the majority of SAT
  settings-reflected state) recover immediately from broker memory.

### Risks and mitigations

- *Risk*: shadow-state drift during refactor — one call site forgotten, keeps publishing
  unconditionally.
  *Mitigation*: `evaluate.py` CI gate `check_sat_publishes_use_helpers` (pattern-level binding,
  added with this ADR) scans all `*.ino`/`*.cpp` for `sendMQTTData(F("sat/...` and
  `sendMQTTData(PSTR("sat/...` outside a whitelist of `SATmqttPublish.cpp`. Any match fails
  the build.

- *Risk*: tolerance too coarse for a field — a meaningful change is missed.
  *Mitigation*: tolerance table is guideline-level (sub-rule 4) and per-field tunable.
  Beta-tester feedback in Discord `#dev-sat-mqtt` is the primary signal for re-tuning.

- *Risk*: modulo bias in the jitter formula skews the random distribution.
  *Mitigation*: `span = 240000` vs `UINT32_MAX = 4.29e9` puts the bias well below 1 ppm.
  Acceptable.

- *Risk*: `millis()` rollover (49.7 days) triggers a false heartbeat.
  *Mitigation*: the comparison `(int32_t)(millis() - shadow.nextRepublishMs) >= 0` is the
  standard Arduino rollover-safe pattern. Correct for any scheduling window under ~24 days,
  which 11 minutes obviously is.

- *Risk*: 24-byte string shadow too small for some enum names.
  Audit of currently published strings: `flame_status` worst case is `"INSUFFICIENT_DATA"`
  (17 chars + null = 18 bytes — fits); `boiler_status` worst case is around 19 chars + null
  (fits); `cycle_class`, `cycle_phase`, `curve_recommendation` all single short words.
  *Mitigation*: a `static_assert(sizeof(SATShadowS::last) >= 20, ...)` in the helpers, and a
  PR-review audit table. If a future string is added that exceeds 23 chars, bump to 32 in a
  follow-up patch ADR rather than enlarging silently.

## Related Decisions

- **ADR-006** (MQTT integration pattern) — the underlying MQTT layer SAT publishes ride on.
- **ADR-052** (MQTT publish eligibility contract) — analogous logic for OT; deliberately
  NOT reused by SAT for the reasons in Alternative 3.
- **ADR-073** (SAT MQTT topic structure) — flat keys and per-topic retained/non-retained policy
  remain in force; this ADR is orthogonal.
- **ADR-077** (Streaming MQTT HA discovery) — HA discovery has its own lifecycle and is
  explicitly out of scope here.
- **ADR-088** (MQTT status burst windowing) — SAT publishes avoid synchronous bursts by
  construction (jitter), so they do not need to participate in burst windowing.
- **ADR-089** (Heap-tier machine contract) — `canPublishMQTT()` remains the consumer-side
  gate; helpers do not duplicate the check, they ride the existing `sendMQTTData` path which
  already honours it.
- **ADR-090** (Re-entrancy guard pattern) — shadows are file-static, single-threaded access,
  no lock required.
- **ADR-101** (Flat per-value topics over aggregated JSON) — confirmed; rules out the
  `sat/state` blob alternative.

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "sendMQTTData\\(\\s*(F|PSTR)\\(\\s*\"sat/",
      "path_glob": "src/OTGW-firmware/**/*.{ino,cpp}",
      "message": "ADR-111: SAT publishes must flow through publishIfChangedF/I/B/S helpers in SATmqttPublish.h. Direct sendMQTTData on sat/* topics is forbidden outside SATmqttPublish.cpp."
    }
  ],
  "require_pattern": [
    {
      "pattern": "publishIfChanged[FIBS]\\s*\\(",
      "path_glob": "src/OTGW-firmware/SATcontrol.ino",
      "message": "ADR-111: SATcontrol.ino must use the publishIfChanged* helpers for SAT publishes."
    }
  ],
  "llm_judge": true
}
```

The `forbid_pattern` declarative gate catches the main rule. `llm_judge: true` is added because
edge cases — a legitimate exception with an inline rationale comment, or a new SAT topic added
outside the existing publishers — need Sonnet's judgement at commit time. The whitelist for
`SATmqttPublish.cpp` is enforced via the in-session judge logic, not the regex (the regex
matches but the judge clears the violation when the file is the helper implementation itself).

## References

- Source files under refactor (current state on `feature-dev-2.0.0-otgw32-esp32-sat-support`):
  - `src/OTGW-firmware/SATcontrol.ino:1986-2279` — `satPublishMQTT()` with ~50 raw publishes
  - `src/OTGW-firmware/SATpressure.ino:62-72` — `satPressureHealthPublish()` (2 topics)
  - `src/OTGW-firmware/SATble.ino:570-650` — `satBLEPublishStateTopics()` (6 topics)
  - `src/OTGW-firmware/SATweather.ino:677-724` — `weatherPublishMQTT()` (13 topics)
- Live evidence: telnet debug log of `otgw-1020BA16C6BC`, firmware `2.0.0-alpha.71+d95caa1`,
  captured 2026-05-26.
- Helpers to be introduced in `src/OTGW-firmware/SATmqttPublish.h` + `.cpp`.
- CI gate to land in `evaluate.py` as `check_sat_publishes_use_helpers` alongside this ADR's
  acceptance.
- Backlog: TASK-722 (`feat-2.0.0: SAT MQTT publish on-change + jittered 7-11min heartbeat`).
- Memory budget (approximately 1016 B BSS total, comfortably under the 1.5 KB cap): ~50 floats
  × 12 B + ~12 ints × 12 B + ~10 bools × 8 B + ~6 strings × 32 B.
