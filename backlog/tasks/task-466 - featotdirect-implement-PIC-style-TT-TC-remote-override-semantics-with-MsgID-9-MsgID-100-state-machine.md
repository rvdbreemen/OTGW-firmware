---
id: TASK-466
title: >-
  feat(otdirect): implement PIC-style TT=/TC= remote-override semantics with
  MsgID 9 + MsgID 100 state machine
status: To Do
assignee: []
created_date: '2026-04-27 23:55'
updated_date: '2026-04-30 02:12'
labels:
  - otdirect
  - pic-parity
  - follow-up
dependencies:
  - TASK-443
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/OTDirecttypes.h
  - other-projects/otgw-6.6/gateway.asm
  - >-
    docs/opentherm
    specification/OpenTherm-Protocol-Specification-v4.2-message-id-reference.md
  - docs/MANUAL.md
  - tests/otdirect_pic_parity_fixture.md
priority: medium
ordinal: 9000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-443. The minimal fix in TASK-443 routed TC= to MsgID 16 (TrSet) like TT= so the safety bug (TC could lower MsgID 1 TSet) was closed, but it left both commands behaving identically: persistent override of MsgID 16 until explicit clear. PIC firmware actually distinguishes them via the OpenTherm remote-override protocol. This task implements that distinction in OTDirect.

# Background

## What PIC does (gateway.asm SerialCmd / OverrideCH region)

| Command | OT layer effect | Lifetime | Use case |
|---|---|---|---|
| TT=<v> | MsgID 16 TrSet override + MsgID 100 flags "manual override + program override allowed" | Until thermostat reports its program has overridden, or new TT=/TC= replaces, or TT=0 clears | "boost room to 20° until program changes again" |
| TC=<v> | MsgID 16 TrSet override + MsgID 100 flags "manual override + override priority" | Until explicit TC=0 or TT= replaces | "vacation: 15° regardless of program" |

Both update the same MsgID 16 frame, but **MsgID 100 (RemoteOverrideFunction) low byte bits** differ:
- bit0 = ManualChangePriority (TC sets, TT clears)
- bit1 = ProgramChangePriority (TT sets, TC clears)

The thermostat reads MsgID 100 from the boiler/gateway to decide whether to honour the manual setpoint or its own program. The boiler ignores MsgID 100 (it only cares about MsgID 16 directly), but the THERMOSTAT respects it as a UX hint.

## Auto-clear trigger for TT=

Per gateway.asm: when the thermostat sends a fresh MsgID 16 READ_DATA with a value ≠ the current TT setpoint AND the local "program just changed" flag fires, PIC clears the TT override automatically. The trigger is essentially "thermostat moved on its own, override mission accomplished".

## Why OTDirect can't currently do this

- OTDirect's `setOverride(msgId, value)` is a single-MsgID write/replace cache. No coordination across MsgIDs 16/9/100.
- No state machine for "is the override currently in effect or has it been honoured-then-superseded".
- No MsgID 100 frame interception/synthesis.

# Implementation plan

## Phase 1 — Data model (OTDirect.ino + OTDirecttypes.h)

Add to OTDirect state, near line 290 (where override tables live):

```cpp
// TASK-466: Remote override state machine for TT=/TC=.
// MsgID 16 carries the actual setpoint value; MsgID 100 carries the
// flags telling the thermostat how to treat it. PIC distinguishes:
//   TT (temporary)  -> Program override priority bit set
//   TC (constant)   -> Manual override priority bit set
enum OTRemoteOverrideMode : uint8_t {
  OT_OVERRIDE_NONE = 0,
  OT_OVERRIDE_TEMPORARY = 1,   // TT=
  OT_OVERRIDE_CONSTANT  = 2,   // TC=
};

struct OTRemoteOverrideState {
  OTRemoteOverrideMode mode;
  uint16_t f88Value;            // current override value (f8.8 of MsgID 16)
  uint32_t setAtMs;             // millis() when override was set
  uint32_t lastThermostatMs;    // millis() of last MsgID 16 READ from thermostat
  uint16_t lastThermostatVal;   // last thermostat-reported MsgID 16 value (f8.8)
  uint8_t  honoredCount;        // how many cycles thermostat honoured the override
};
static OTRemoteOverrideState otRemoteOverride = { OT_OVERRIDE_NONE, 0, 0, 0, 0xFFFF, 0 };
```

Add MsgID 100 to the override table so synthesised frames carry the correct flags:

```cpp
// In otOverrides[]:
{ 100, false, 0 },   // RemoteOverrideFunction — TT/TC flag synthesis
```

## Phase 2 — Command handlers (OTDirect.ino)

Rewrite TT= and TC= handlers to set state.mode + state.f88Value AND update MsgID 16 + MsgID 100 overrides:

```cpp
else if (cmd0 == 'T' && cmd1 == 'T') {
  float temp = atof(value);
  if (temp == 0.0f) {
    clearRemoteOverride();   // helper
  } else {
    setRemoteOverride(OT_OVERRIDE_TEMPORARY, temp);
  }
  dtostrf(temp, 1, 2, rspBuf);
  synthesizeResponse(buf, rspBuf);
}
else if (cmd0 == 'T' && cmd1 == 'C') {
  float temp = atof(value);
  if (temp == 0.0f) {
    clearRemoteOverride();
  } else {
    setRemoteOverride(OT_OVERRIDE_CONSTANT, temp);
  }
  dtostrf(temp, 1, 2, rspBuf);
  synthesizeResponse(buf, rspBuf);
}
```

Helpers:

```cpp
static void setRemoteOverride(OTRemoteOverrideMode mode, float celsius) {
  uint16_t f88 = (uint16_t)((int16_t)(celsius * 256.0f));
  otRemoteOverride.mode = mode;
  otRemoteOverride.f88Value = f88;
  otRemoteOverride.setAtMs = millis();
  otRemoteOverride.honoredCount = 0;

  // MsgID 16: replace thermostat's TrSet on outbound
  setOverride(16, f88);

  // MsgID 100: low byte = priority bits (no high byte semantics defined).
  //   bit0 ManualChangePriority  (TC sets)
  //   bit1 ProgramChangePriority (TT sets)
  uint8_t flags = 0;
  if (mode == OT_OVERRIDE_TEMPORARY) flags = 0x02;
  else if (mode == OT_OVERRIDE_CONSTANT) flags = 0x01;
  setOverride(100, (uint16_t)flags);

  enqueueWriteCommand(16, f88, mode == OT_OVERRIDE_TEMPORARY ? "TT" : "TC");
  enqueueWriteCommand(100, (uint16_t)flags, "OVR-flags");
}

static void clearRemoteOverride() {
  otRemoteOverride.mode = OT_OVERRIDE_NONE;
  otRemoteOverride.f88Value = 0;
  otRemoteOverride.honoredCount = 0;
  clearWriteOverride(16);
  clearWriteOverride(100);
  // Push a clearing MsgID 100 = 0 frame so the thermostat's display flips back
  enqueueWriteCommand(100, 0, "OVR-clear");
}
```

## Phase 3 — Auto-clear logic for TT (loopOTDirect 1Hz timer)

Detect "thermostat-honoured-then-program-took-over" pattern. Two checks:

1. **Honour-detection**: when we observe a thermostat MsgID 16 READ_DATA whose value ≈ otRemoteOverride.f88Value (within 0.25°C), increment honoredCount. After honoredCount >= 3 the override is considered "engaged".

2. **Auto-clear-trigger**: AFTER engagement, if a thermostat MsgID 16 READ_DATA arrives with a value that differs from f88Value by more than 0.5°C AND the previous-thermostat-val also differed (i.e. thermostat is actively driving its own program again), and mode == TEMPORARY, clear the override.

Hook into the MsgID 16 inbound frame path (search for where thermostat MsgID 16 frames are processed in OTDirect; likely in handleSlaveRequest or onMasterFrame). Update otRemoteOverride.lastThermostatMs + lastThermostatVal there.

```cpp
// In the MsgID 16 inbound observer:
static void onThermostatMsgID16(uint16_t f88) {
  uint32_t now = millis();
  otRemoteOverride.lastThermostatMs = now;

  if (otRemoteOverride.mode != OT_OVERRIDE_TEMPORARY) {
    otRemoteOverride.lastThermostatVal = f88;
    return;
  }

  int16_t delta = (int16_t)f88 - (int16_t)otRemoteOverride.f88Value;
  if (delta < 0) delta = -delta;

  if (delta < (int16_t)(0.25f * 256.0f)) {
    // Thermostat reports our value -> engaged
    if (otRemoteOverride.honoredCount < 255) otRemoteOverride.honoredCount++;
  } else if (otRemoteOverride.honoredCount >= 3 && delta > (int16_t)(0.5f * 256.0f)) {
    // Thermostat moved on its own after honouring us -> TT auto-clear
    OTDDebugTln(F("OTD: TT auto-clear (thermostat program resumed)"));
    clearRemoteOverride();
  }
  otRemoteOverride.lastThermostatVal = f88;
}
```

## Phase 4 — Persistence + reporting

- `PR=O` (override) reporting: extend the existing PR=O handler so it returns the override mode as TT/TC/none plus value.
- MQTT autoconfig: state.otd.eOverrideMode (new enum field) so HA shows "Manual override: temporary @ 20.5°C".
- Settings: NOT persisted across reboot — overrides are runtime state, matches PIC behaviour where heartbeat resets clear them.

## Phase 5 — TASK-442 interaction

CS/C2 expiry from TASK-442 stays untouched (different MsgIDs, different commands). The new TT/TC state machine is independent. Make sure clearRemoteOverride() does not touch otCSLastCommandMs / otC2LastCommandMs.

## Phase 6 — Tests

Extend tests/otdirect_pic_parity_fixture.md (TASK-444) with:
- TT=20 -> MsgID 16 = 0x1400, MsgID 100 = 0x0002
- TC=15 -> MsgID 16 = 0x0F00, MsgID 100 = 0x0001
- TT=0 / TC=0 -> MsgID 100 = 0x0000, MsgID 16 override cleared
- TT auto-clear after 3 honour-cycles + thermostat program shift
- TC persists across the same scenario

# Edge cases to handle

1. TT then TC (or vice versa) — second command replaces the first cleanly; honoredCount resets.
2. Reboot during active TT — override is lost on power cycle (matches PIC).
3. SAT control loop running with CS= concurrently — independent (different MsgID); document this so users know SAT + TT/TC overlay is fine.
4. Thermostat that ignores MsgID 100 — no harm, MsgID 16 still drives behaviour, auto-clear still works because trigger is on thermostat's own MsgID 16 movement.
5. Honour-cycle buffer too tight (0.25°C) — make it a const for tuning. Document the choice.

# What we explicitly do NOT do

- Do not implement MsgID 9 (TrOverride) write/synthesis — too far down the rabbit hole; the boiler already gets the right TrSet via MsgID 16.
- Do not persist override across reboot — matches PIC, simpler.
- Do not add a `mode=temporary|constant` REST endpoint — TT= / TC= command routes already work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTRemoteOverrideState struct + OT_OVERRIDE_NONE/TEMPORARY/CONSTANT enum added in OTDirect.ino (or OTDirecttypes.h) with file-static instance otRemoteOverride.
- [x] #2 TT= and TC= command handlers route through setRemoteOverride() helper that sets MsgID 16 (TrSet) + MsgID 100 (RemoteOverrideFunction flags 0x02 for TT, 0x01 for TC).
- [x] #3 TT=0 and TC=0 route through clearRemoteOverride() which clears MsgID 16 and MsgID 100 overrides and emits a clearing WRITE_DATA for MsgID 100 = 0.
- [x] #4 Inbound MsgID 16 from thermostat is observed (hook into existing OT processing path) and updates otRemoteOverride.lastThermostatVal + honoredCount.
- [x] #5 Auto-clear trigger: when mode == TEMPORARY and honoredCount >= 3 and the next thermostat MsgID 16 differs by > 0.5C from the override value, the override is cleared automatically with an OTDDebug log line.
- [x] #6 TC behaviour: persists indefinitely across thermostat MsgID 16 transitions; only TC=0 (or TT= replacing it) clears.
- [x] #7 PR=O reporting includes override mode (none / TT / TC) and value.
- [x] #8 TASK-444 fixture extended with TT/TC frame-shape rows: TT=20 -> 0x1400 + 0x0002, TC=15 -> 0x0F00 + 0x0001, clearing -> 0x0000 + 0x0000.
- [x] #9 TASK-442 CS/C2 expiry timestamps are not touched by the new clearRemoteOverride() and CS/C2 behaviour remains independent.
- [x] #10 PIC firmware sources are not modified.
- [x] #11 Build clean on ESP32 and ESP8266, 0 warnings 0 errors.
- [ ] #12 Hardware verification scenario documented: drag thermostat program through a setpoint change while TT is active -> auto-clears within 3-5 thermostat cycles; same scenario with TC -> stays.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-30 00:50 main session takeover from agent: Agent completed Phases 1-5 in source. Fixture markdown column-count errors (rows 4-5 in Table 9, `\|...\|` escapes confused the validator) fixed by replacing with `abs(...)` notation. `python tests/check_otdirect_fixture.py` now passes clean.

2026-04-30 00:51 evaluate.py --quick: 67 checks, 57 pass, 2 warn, 1 fail (pre-existing PROGMEM 14-violations baseline; TASK-482 just shaved one off from 15). Direct grep of OTDirect.ino for hot-path `String`, `DebugTln("..."`, `DebugTf("..."`, `snprintf(` (without `_P`) returns zero matches. My changes (and the agent's) add zero new violations.

2026-04-30 00:51 builds in progress (python build.py --firmware in background). AC #11 will flip when both ESP32 + ESP8266 succeed.

Open question 1 from agent (msgType==1 filter on MsgID 16 inbound observer): CONFIRMED CORRECT. Per OT v4.2 spec, MsgID 16 (TrSet) is master-write-only. Master = thermostat. So WRITE_DATA on MsgID 16 is the only valid direction. Filter stays as-is.

Open question 2 from agent (MsgID 100 clear via direct otCmdEnqueue, not via enqueueWriteCommand): CONFIRMED CORRECT. The clear signal should be one-shot, NOT persistently rebroadcast every periodic cycle. enqueueWriteCommand would update the cache and keep emitting forever; otCmdEnqueue is single-shot. Design is right.

2026-04-30 02:13 BUILDS GREEN: ESP32 SUCCESS (Flash 95.8%, RAM 31.7%, 19m25s), ESP8266 SUCCESS (Flash 77.3%, RAM 84.7%, 2m41s). AC #11 satisfied. Open: AC #12 hardware verification by owner.
<!-- SECTION:NOTES:END -->
