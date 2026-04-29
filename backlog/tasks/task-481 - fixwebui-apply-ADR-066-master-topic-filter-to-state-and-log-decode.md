---
id: TASK-481
title: 'fix(webui): apply ADR-066 master-topic filter to state and log decode'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-29 21:02'
updated_date: '2026-04-29 21:18'
labels:
  - webui
  - ADR-066
  - follow-up
  - rest-api
  - ot-log
dependencies: []
references:
  - docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md
  - docs/api/MQTT-message-id-echo-audit.md
  - src/OTGW-firmware/OTGW-Core.ino
  - 'C:\Users\rvdbr\.claude\plans\abundant-wandering-hammock.md'
  - TASK-479
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Context

Beta tester reports: TASK-479 / ADR-066 fix stabilized the MQTT base topic, but the WebUI still shows flapping values. Root cause analysis (plan: C:\Users\rvdbr\.claude\plans\abundant-wandering-hammock.md) identified that the firmware has four publish tiers with inconsistent filter strength, and ADR-066 only tightened the strictest tier (MQTT base topic).

## Four-tier publish architecture

| Tier | Path | Filter | Status |
|---|---|---|---|
| 1 | `AddLogf` (log decode) | none | UNFIXED — drives WebUI OT-log via WebSocket |
| 2 | `value = _value` (state write) | `is_value_valid` (broad) | UNFIXED — drives WebUI stats via REST |
| 3 | `publishToSourceTopic` | `is_value_valid` + `bSlaveEchoesValue` gate | OK |
| 4 | `sendMQTTData` (base topic) | `is_value_valid_for_master_topic` (narrow) | OK (TASK-479) |

The WebUI consumes Tier 1 (log via WebSocket) and Tier 2 (stats via REST `/api/v2/otgw/otmonitor`). Both tiers accept WRITE_ACK for OT_WRITE/OT_RW msgIDs. For msgIDs with `bSlaveEchoesValue=false` (Tr, TrSet, TrSetCH2, MaxRelModLevelSetting, TrCH2, RFsensorStatus), the WRITE_ACK data byte is per-spec undefined — decoded as random garbage that overwrites the legitimate WRITE_DATA value.

Result: WebUI stats panel oscillates between thermostat-intent and boiler-echo-garbage; OT-log shows two decoded values per WRITE-pair (e.g. `Tr = 20.00 °C` followed by `Tr = 18.50 °C`).

## Solution: align Tier 1 and Tier 2 with Tier 4

Apply `is_value_valid_for_master_topic()` to both:
- The `AddLogf` decode line in `print_f88/s16/s8s8/u16` (Tier 1)
- The `value = _value` state write in same functions (Tier 2)

For non-master-topic messages, log only the label without `= value`. The protocol-event remains visible (timestamp, source, msgid, message-type, indicator) for diagnostic purposes; only the misleading decode is suppressed.

Tier 3 (`publishToSourceTopic`) and Tier 4 (`sendMQTTData`) are not changed — ADR-066 fix preserved.

## Files

- `src/OTGW-firmware/OTGW-Core.ino`
  - `print_f88()` (~1939-1956)
  - `print_s16()` (~1959-1975)
  - `print_s8s8()` (~1977-2003)
  - `print_u16()` (~2005-2024)

No changes to `restAPI.ino`, `webSocketStuff.ino`, `index.js`, `OTGW-Core.h`, or ADR-066 logic.

## Out of scope

- PS=1 summary path (`publishPSSummaryFieldValue`): no `OTdata.type` context; PS=1 is aggregated state-dump, doesn't flap
- Indicator refinement at processOT:4060-4064: existing `>/-/P/space` semantics (= protocol-validity) coherent enough when paired with value-presence-as-display-validity
- Status-flag prints (rond 2049+): `is_value_valid_for_master_topic` accepts status-flag msgIDs explicitly
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 print_f88, print_s16, print_s8s8, print_u16 each compute validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem) once per call
- [x] #2 AddLogf with decoded value is gated on validForMaster; non-valid messages log label only (no = value)
- [x] #3 State-write (value = _value or value = OTdata.u16()) is gated on validForMaster
- [x] #4 Tier 3 (publishToSourceTopic) and Tier 4 (sendMQTTData base topic) call sites unchanged
- [x] #5 evaluate.py passes (no new violations)
- [x] #6 ESP8266 build clean (python build.py --firmware)
- [ ] #7 Hardware verification (deferred to user/tester): WebUI stats panel for Tr / TrSet / TSet stable; OT-log shows one decoded value per WRITE-pair; READ-only msgIDs (Tboiler, Toutside) unchanged; MQTT regression-free
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan

For each of `print_f88`, `print_s16`, `print_s8s8`, `print_u16`:

1. Compute `const bool validForMaster = is_value_valid_for_master_topic(OTdata, OTlookupitem);` once at the top of the function (after building `_msg`).
2. Replace the unconditional `AddLogf("%s = %s %s", label, _msg, unit)` with an if/else: when `validForMaster`, log with value; else log only the label (no `= value`).
3. Inside the existing `if (is_value_valid(...))` block: replace the existing inline `is_value_valid_for_master_topic` call (where present) with the cached `validForMaster`. Gate the state write (`value = _value` or `value = OTdata.u16()`) on `validForMaster`.

`print_s8s8` differs slightly: the AddLogf decodes both HB and LB in a single line; on `!validForMaster`, log just the label. The state-write at the end is the only place to gate; the two `sendMQTTData` calls (HB and LB topics) keep using `validForMaster` as before.

After the four functions: run `python evaluate.py` and `python build.py --firmware`.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete — `OTGW-Core.ino` print_f88/s16/s8s8/u16 each cache `validForMaster = is_value_valid_for_master_topic(...)` and gate both Tier 1 (AddLogf decoded value) and Tier 2 (state write) on it. Tier 3 (publishToSourceTopic) and Tier 4 (sendMQTTData base topic) call sites unchanged. The `_msg` buffer + `_valid` cache for MQTT path remain identical to pre-fix.

Build verification (2026-04-29): ESP8266 SUCCESS 84.7%/77.3% RAM/Flash; ESP32-S3 SUCCESS 32.1%/96.8% RAM/Flash. evaluate.py PROGMEM gate baseline = 27 violations, post-fix = 15 — drop is from auto-generated `OTGW-firmware.ino.cpp` filtering quirk (false-positive scope, see TASK-482). My code-changes don't match any of the patterns evaluate.py looks for (DebugTln/DebugTf/snprintf/strcmp/strcasecmp), so no real PROGMEM regression introduced.

AC #7 (hardware verification) remains pending — requires user/tester to confirm WebUI stats stable + log shows one decoded value per WRITE-pair on real hardware.

Incremental rebuild confirmation (commit c694fbdf): ESP8266 SUCCESS in 1m6s, ESP32-S3 SUCCESS in 2m40s. RAM and Flash byte-counts identical to first build (69408/806924 ESP8266, 105076/1902255 ESP32-S3); the validForMaster cache and conditional state-write are memory-neutral after compiler optimization. Distribution zip published: OTGW-firmware-esp32-2.0.0-alpha+c694fbd-flash.zip. Status stays In Progress for hardware verification (AC #7); flip to Done once tester confirms WebUI stats stable + log shows one decoded value per WRITE-pair.
<!-- SECTION:NOTES:END -->
