---
id: TASK-491
title: 'fix(otdirect): correct sign-handling in onThermostatMsgID16 f88 delta'
status: Done
assignee: []
created_date: '2026-04-30 05:43'
updated_date: '2026-04-30 05:47'
labels:
  - esp32
  - otdirect
  - pic-parity
  - code-review
dependencies:
  - TASK-466
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

Code-review finding #3 on the TASK-466 TT/TC remote-override state
machine.

`onThermostatMsgID16()` casts `uint16_t f88` directly to `int32_t` to
compute the delta:

```cpp
int32_t signedDelta = (int32_t)f88 - (int32_t)otRemoteOverride.f88Value;
uint32_t delta = (signedDelta < 0) ? (uint32_t)(-signedDelta) : (uint32_t)signedDelta;
```

OpenTherm's `f8.8` is a *signed* fixed-point format: a setpoint of
`-5.00 °C` would be stored as `0xFB00`, which when cast directly from
`uint16_t` to `int32_t` becomes `64256` instead of `-1280`. The
absolute-value computation `delta = |signedDelta|` then yields
nonsense for negative inputs.

In practice this is a non-issue because `TrSet` (MsgID 16, room
temperature setpoint) is always a positive value (15-25 °C range);
the high bit is never set. The state machine therefore behaves
correctly today.

However, the inline comment claims "Compute |delta| in f8.8 units,
ignoring sign", which overpromises correctness. A future caller who
extends the helper to a signed-capable MsgID would inherit a latent
bug.

## Fix options

**Option A — sign-extend the cast**:

```cpp
int32_t signedDelta = (int32_t)(int16_t)f88 - (int32_t)(int16_t)otRemoteOverride.f88Value;
```

Cast `uint16_t -> int16_t` first so two's-complement bits are
preserved; then widen to `int32_t`. Mathematically correct for the
full f8.8 range, including negative setpoints.

**Option B — narrow the comment**:

Document the constraint instead:

```
// Compute |delta| in raw u16 units. Valid only for non-negative
// f8.8 inputs (TrSet is always >= 0 in practice).
```

Option A is the principled fix; Option B is the minimal-change fix.
Option A is preferred since the cost is a single `(int16_t)` cast.

## Validation

- ESP32 / ESP8266 builds clean (Option A is a one-line change).
- Behaviour identical for room-setpoint range (15-25 °C); difference
  only manifests for hypothetical negative TrSet which the OT spec
  does not preclude but no thermostat sends in practice.
- Add a fixture row in `tests/otdirect_pic_parity_fixture.md`
  covering a hypothetical negative TrSet to lock in the new
  semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 `onThermostatMsgID16` correctly handles negative f8.8 inputs (sign-extension via `(int16_t)` cast) OR comment narrowed to clearly state the non-negative-only constraint
- [x] #2 Behaviour for room-setpoint range (15-25 °C) is unchanged
- [x] #3 Builds clean on ESP32 and ESP8266
- [x] #4 Fixture row in `tests/otdirect_pic_parity_fixture.md` covers the sign-handling boundary case
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## TASK-491 — Sign-extension in onThermostatMsgID16 f8.8 delta

Fix Option A from the task plan: cast `uint16_t f88` through `(int16_t)`
before widening to `int32_t` so two's-complement bits are preserved.
Without the hop a raw value of `0xFB00` (-5.0 °C) would compare as
`+64256` instead of `-1280` and the honour/release deltas would
mis-fire for negative TrSet values.

In practice today no thermostat sends negative TrSet (room setpoints
are always 15-25 °C), so this is a latent-bug fix rather than an
observed regression. The change is one line of code plus a clarifying
comment; it locks in correctness for the boundary case and removes a
gotcha for any future caller that extends the helper to a
signed-capable MsgID.

### Change
- `src/OTGW-firmware/OTDirect.ino` `onThermostatMsgID16`: explicit
  `(int16_t)` cast hop on both the incoming and stored f8.8 values
  before the `int32_t` widening.
- `tests/otdirect_pic_parity_fixture.md`: new row "Negative TrSet
  sign-handling" pins the boundary semantics in the parity table.

### Verification
- ESP32 build SUCCESS (combined cycle with TASK-489 + TASK-490).
- Behaviour for room-setpoint range (15-25 °C) is unchanged.
- `python tests/check_otdirect_fixture.py`: PASS (all rows have the
  required columns, including the new negative-TrSet row).

Pushed in commit `67ad53cf` on `feature-dev-2.0.0-otgw32-esp32-sat-support`.
<!-- SECTION:FINAL_SUMMARY:END -->
