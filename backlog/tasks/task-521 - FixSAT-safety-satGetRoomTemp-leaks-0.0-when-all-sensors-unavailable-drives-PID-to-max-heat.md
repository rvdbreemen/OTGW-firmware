---
id: TASK-521
title: >-
  Fix(SAT safety): satGetRoomTemp() leaks 0.0 when all sensors unavailable,
  drives PID to max heat
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-02 21:14'
updated_date: '2026-05-02 21:30'
labels:
  - bug
  - safety
  - sat
  - esp32
dependencies: []
references:
  - 'Discord #dev-sat-mqtt, sergeantd, 2026-05-02 telnet log paste'
  - 'src/OTGW-firmware/SATcontrol.ino:888-947 (satGetRoomTemp)'
  - 'src/OTGW-firmware/SATcontrol.ino:39-42 (staleness constants)'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed in production log on `OTGW-firmware-esp32-2.0.0-alpha+168bd9e` (sergeantd, 2026-05-02 ~20:33).

Sequence:
1. BLE sensor `A4:C1:38:92:4F:AC` works for ~3 min: SAT loop reads `room=21.7` correctly, computes PID, sends sane CS values.
2. At 20:33:54 BLE goes stale (`SAT_STALE_TEMP_BLE_MS = 300000UL = 5 min` elapsed without an update from the sensor) — `satGetRoomTemp()` falls through to the next source.
3. Sergeantd's config: `bUseExternalTemp=false`, no MultiArea configured, no thermostat (OTDirect/OTGW32 master mode → no MsgID 24 inbound). The fallback chain in `satGetRoomTemp()` (SATcontrol.ino:888-947) ends with `return otRoom;` where `otRoom = OTcurrentSystemState.Tr` is **initialised to 0.0f at boot** and never updated.
4. PID receives `room=0.0 target=21.0 → error=-21.0`. Output ramps to the absolute ceiling: `pid=62.0 final=62.0` (only `SAT_GLOBAL_MAX_SETPOINT=65.0f` clamps it).
5. SAT enqueues `CS=62.0` to OTDirect every 30 s for the rest of the log.

If the boiler had been online during that period the firmware would have driven flow setpoint to ~62 °C continuously based on a fictitious 0 °C room reading. This is a single-point-of-failure safety hole: a missing sensor masquerades as a freezing house.

Root cause: `satGetRoomTemp()` returns a `float` with no in-band invalid-sentinel. Every fallback step succeeds with whatever value is currently in `OTcurrentSystemState.Tr`, including the boot-time zero. There is `SAT_MAX_SKIP_COUNT = 10` (SATcontrol.ino:42) but it appears not to be checked against an "all sources stale" condition — at minimum the `otRoom` zero path is not flagged as invalid.

Related code:
- `SATcontrol.ino:888-947` — `satGetRoomTemp()` fallback chain
- `SATcontrol.ino:39-42` — staleness constants and skip-count
- Reference for log: see Discord #dev-sat-mqtt 2026-05-02 around 20:33 (sergeantd's paste)

Note: this is a behaviour bug separate from any specific BLE-sensor reliability work. Even if the BLE sensor is bulletproof, the 0.0-fallback hazard remains for any user who runs SAT without an MQTT external temp and without a thermostat (legitimate OTDirect+SAT config).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: SAT enabled on OTGW32, BLE primary, no MQTT external, no MsgID 24 inbound — confirm satGetRoomTemp() returns 0.0 when BLE goes stale
- [ ] #2 satGetRoomTemp() distinguishes 'no valid source' from 'genuinely 0 °C' — either via NaN sentinel, an out-param validity flag, or a separate satIsRoomTempValid() probe
- [ ] #3 SAT control loop on invalid input: skip PID update, hold last-good for at most SAT_MAX_SKIP_COUNT cycles, then drop to a documented safe state (CH=0 / final=0 / fall back to OT-bus heating curve)
- [ ] #4 After SAT_MAX_SKIP_COUNT consecutive invalid-input cycles, log a clear DebugTln explaining why SAT disengaged so the user is not left guessing
- [ ] #5 Integration test or manual check: simulate the all-sources-fail condition and verify final stays at safe value (not 62 °C)
- [ ] #6 OTcurrentSystemState.Tr boot-default 0.0f either replaced with NAN or paired with a 'has ever been updated' flag, so a never-received Tr cannot masquerade as a real reading
- [ ] #7 Compiles clean on ESP8266 and ESP32
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Implementation plan

Branch: `feature-dev-2.0.0-otgw32-esp32-sat-support` (running in parallel with TASK-516).

### Parallel-safety scope adjustment

To run in parallel with TASK-516 (which reads `OTGW-Core.h`/`OTGW-Core.ino` for `SlaveConfigMemberIDcode`), this task is restricted to **SATcontrol.ino only**. Do NOT touch:
- `OTGW-Core.h` / `OTGW-Core.ino` — TASK-516's agent reads these
- Anywhere else outside SATcontrol.ino

AC #6 (replace `OTcurrentSystemState.Tr = 0.0f` boot-default with NAN or add a "has ever been updated" flag in the struct) is **deferred to a follow-up task** to keep parallel execution clean. We solve the safety problem locally in SATcontrol.ino instead.

### How the local fix works

In `satGetRoomTemp()` (SATcontrol.ino:888-947), when the fallback chain reaches `OTcurrentSystemState.Tr`:
- Track a static-local "first non-zero Tr ever observed" flag in SATcontrol.ino.
- If `bUseExternalTemp` is off, no MultiArea, BLE not valid, **and** Tr has never been observed as non-zero, return `NAN` (or signal invalid via an out-param).
- The SAT control loop checks for NAN, skips PID update, increments skip counter, holds last-known-good for ≤ `SAT_MAX_SKIP_COUNT=10` cycles, then drops to a documented safe state (`CH=0` + `final=0` to clamp boiler).

### File ownership for this task
- `src/OTGW-firmware/SATcontrol.ino` — only file changed.

### Files explicitly NOT touched
- `OTGW-Core.h`, `OTGW-Core.ino`, `OTGW-firmware.h`, `settingStuff.ino`, `restAPI.ino`, `MQTTstuff.ino`, `data/*` — owned by TASK-516 or unrelated.

### Phases
1. **Add "Tr ever valid" tracking** as a static-local in `satGetRoomTemp()`.
2. **Modify fallback chain** to return NAN when Tr would be the first-ever value of 0.0.
3. **Modify SAT control loop** (`satControlLoop` around SATcontrol.ino:3489) to detect NAN, skip PID, increment skip counter, hold last-good or fall to safe state after `SAT_MAX_SKIP_COUNT`.
4. **Add a clear DebugTln** when SAT disengages so the user understands.
5. **Manual test plan**: simulate the all-sources-fail condition by disabling BLE + external temp + thermostat, confirm `final=0` (or last-good) instead of `final=62`.
6. **Build verify**: `python build.py --firmware` for ESP8266.
7. **Commit + push**: `fix(sat): guard against ghost room=0 driving PID to max heat (TASK-521)`.

### AC coverage
- AC #1 ✅ Reproduce condition documented above
- AC #2 ✅ NAN sentinel from `satGetRoomTemp()`
- AC #3 ✅ Skip PID, hold last-good, drop to safe state after SAT_MAX_SKIP_COUNT
- AC #4 ✅ Explicit DebugTln when disengaging
- AC #5 ✅ Simulate the fail-state, confirm `final` stays safe
- AC #6 ⏸️ Deferred to follow-up task (avoid OTGW-Core.h conflict with TASK-516)
- AC #7 ✅ ESP8266 + ESP32 clean compile

### Architecture rules
- ADR-004: no String class
- All literals via F() / PSTR()
- Debug via SATDebugTln/SATDebugTf
- No new state fields (use static-local in satGetRoomTemp to avoid struct-conflict with 516)
<!-- SECTION:PLAN:END -->
