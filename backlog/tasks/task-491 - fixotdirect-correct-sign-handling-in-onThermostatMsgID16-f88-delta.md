---
id: TASK-491
title: 'fix(otdirect): correct sign-handling in onThermostatMsgID16 f88 delta'
status: To Do
assignee: []
created_date: '2026-04-30 05:43'
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
- [ ] #1 `onThermostatMsgID16` correctly handles negative f8.8 inputs (sign-extension via `(int16_t)` cast) OR comment narrowed to clearly state the non-negative-only constraint
- [ ] #2 Behaviour for room-setpoint range (15-25 °C) is unchanged
- [ ] #3 Builds clean on ESP32 and ESP8266
- [ ] #4 Fixture row in `tests/otdirect_pic_parity_fixture.md` covers the sign-handling boundary case
<!-- AC:END -->
