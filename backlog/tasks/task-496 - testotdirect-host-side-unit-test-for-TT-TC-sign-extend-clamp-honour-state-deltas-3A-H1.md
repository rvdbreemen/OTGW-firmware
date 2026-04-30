---
id: TASK-496
title: >-
  test(otdirect): host-side unit test for TT/TC sign-extend, clamp, honour-state
  deltas (3A-H1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-30 18:13'
labels:
  - tests
  - otdirect
  - code-review
  - follow-up
dependencies:
  - TASK-466
  - TASK-491
  - TASK-495
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Origin

Phase 3A finding 3A-H1 from the comprehensive review of session
`ace21a48..6a01ae5d`: 700+ lines of new firmware C++, zero new
executable tests. The numerical-contract fixes (TASK-491 sign-extend,
TASK-495 clamp, TASK-466 honour/release deltas, f8.8 cast math) are
not pinned by an automated check. Existing host-compilable C++ test
template at `tests/test_dallas_address.cpp` + `tests/README.md` is
the intended pattern.

## Scope

`tests/test_otdirect_override.cpp` covering pure-logic helpers from
`src/OTGW-firmware/OTDirect.ino`:

- f8.8 cast math: `(uint16_t)((int16_t)(celsius * 256.0f))` round-trip.
- Sign-extend: TASK-491's `(int16_t)f88` cast preserves sign for raw
  inputs `0xFB00` (-5.0 °C), `0x8000` (-128.0 °C), `0x7FFF` (~127.99 °C).
- Range clamp: TASK-495's `-40..127` boundary clamps. Verify
  `celsius = -50` clamps to `-40`; `celsius = 200` clamps to `127`;
  `celsius = 22.5` is unchanged.
- Honour-state deltas: replicate the `OT_OVERRIDE_HONOR_DELTA_F88`
  (0x40) and `OT_OVERRIDE_RELEASE_DELTA_F88` (0x80) thresholds.
  Verify a thermostat-echo within 0x40 increments `honoredCount`,
  and a divergence above 0x80 (with mode==TEMPORARY and count >= 3)
  triggers auto-clear.

## Implementation plan

- Lift only the pure-logic helpers into the test file as static
  inline functions, mirroring the `test_dallas_address.cpp` shape.
  Do NOT include `Arduino.h` or PlatformIO headers.
- Use the existing `tests/README.md`-documented build path
  (typically `g++ -std=c++17 tests/test_X.cpp && ./a.out`).
- ~120 lines of C++, 6 assertions minimum:
  1. f8.8 round-trip identity for 22.5 °C.
  2. Negative f8.8 sign-extension correct (input 0xFB00 reads as -5.0).
  3. Clamp `-50` → `-40`.
  4. Clamp `200` → `127`.
  5. Honour-count increments when delta < 0x40.
  6. Auto-clear fires when (count >= 3) AND (delta > 0x80) AND mode == TEMPORARY.

## Out of scope

- Re-implementing the full `OTDirect.ino` state machine in the test.
  Lift only the pure-logic shape.
- Wiring this test into a CI gate (separate concern; if the project
  adds a CI test-runner for `tests/`, the new file plugs in
  automatically).

## Validation

- `g++ -std=c++17 -Wall -Wextra tests/test_otdirect_override.cpp -o /tmp/test_otdirect_override && /tmp/test_otdirect_override`
  exits 0; all assertions pass.
- Optionally: a deliberately mutated copy (e.g., remove the `(int16_t)`
  cast) FAILS the sign-extend assertion — proves the test discriminates.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 `tests/test_otdirect_override.cpp` exists, follows the `tests/test_dallas_address.cpp` pattern, no `Arduino.h` or PlatformIO includes
- [ ] #2 Six assertions covering: f8.8 round-trip, sign-extension, two clamp boundaries, honour-count increment, auto-clear trigger
- [ ] #3 `g++ -std=c++17 tests/test_otdirect_override.cpp -o ...` compiles clean
- [ ] #4 Compiled binary exits 0; all assertions pass
- [ ] #5 Sanity check: removing the `(int16_t)` cast in a copy of the test code FAILS the sign-extension assertion (proves the test discriminates)
<!-- AC:END -->
