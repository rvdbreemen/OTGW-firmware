# Phase 3A: Test Coverage & Quality Review

## Summary

The session work landed roughly 700 lines of new firmware C++ in three hot-path
files (`SATble.ino` NimBLE port, `OTDirect.ino` TT/TC state machine + queue
coalesce, `MQTTstuff.ino` BLE HA-discovery helpers) plus a reformulated
`OTDirecttypes.h`. The only test artefact added in the diff window
`ace21a48..a61373b9` is **45 lines of fixture-table prose in
`tests/otdirect_pic_parity_fixture.md`** — no executable test was written.
`evaluate.py` and `tests/test_evaluate.py` are flagged in `00-scope.md` but
the actual evaluate.py changes (TASK-482) live in commit `ace21a48` itself,
which is the *exclusive* lower bound of the range; the diff window contains
zero changes to those files. The scope manifest is incorrect on this point
(this matches finding 1B-M4).

The test posture for this session is therefore: a curated documentation
fixture for OTDirect, and nothing else. Given the embedded constraints the
test pyramid is necessarily skewed — but the project already has a working
host-side C++ harness pattern (`tests/test_dallas_address.cpp`, with
`tests/README.md` describing how to add new host tests) and `setRemoteOverride`,
`onThermostatMsgID16`, `parseBLEAtcFormat`, `parseBLEBTHomeFormat`,
`bleMacToCompact`, and `otCmdEnqueue` are all pure-logic functions trivially
liftable into that same pattern. The session shipped behaviour that has
explicit numerical contracts (f8.8 sign-extend, BTHome v2 flag bits, MAC
format validation, ring-buffer coalesce semantics, range clamp boundaries)
and none of those contracts are pinned by an automated check. Several of
them landed specifically as Highs in Phases 1+2 (TASK-491 sign-extend,
TASK-495 -40..127 clamp, TASK-494 coalesce + high-water) and a regression
in any of them would be silent until hardware soak. That is exactly the
gap the host-side seam was added to close — for Dallas address parsing,
in 2024.

## Critical findings

(none)

## High findings

### 3A-H1 — TASK-466/491/495 OTDirect override math has zero automated tests

**Severity**: High.

The session added a state machine with explicit numerical contracts:

- `setRemoteOverride()` clamp boundary at `-40.0f` and `+127.0f` (TASK-495,
  fixes 2A-H1 UB).
- `setRemoteOverride()` f8.8 cast `(uint16_t)((int16_t)(celsius * 256.0f))`
  for the boundary values: `-40.0` -> `0xD800`, `+127.0` -> `0x7F00`,
  `+0.0039` -> `0x0001`.
- `onThermostatMsgID16()` sign-extend through `(int16_t)` (TASK-491);
  the explicit fixture row (`0xFB00 = -5.0 °C`) gives the exact bytes
  but no code consumes them.
- `OT_OVERRIDE_HONOR_DELTA_F88 = 0x0040` (0.25 °C), `OT_OVERRIDE_RELEASE_DELTA_F88 = 0x0080` (0.50 °C),
  `OT_OVERRIDE_HONOR_THRESHOLD = 3` cycles. The honour/release transitions
  are the exact lines of code where TASK-491 found a real bug.

A future refactor that drops the `(int16_t)` hop, or that changes the
clamp to `-127..127` (matching the wrong half of the f8.8 range), or that
flips the strict-less-than comparison in the honour path would compile
clean, ship, and only surface in field reports about thermostats whose
program never resumes.

**Recommendation**: Add a host-compilable C++ test
`tests/test_otdirect_override.cpp` modelled on `tests/test_dallas_address.cpp`,
covering:

1. `f88FromCelsius()` (extract the cast/clamp into a pure helper, OR copy
   the clamp + cast lines into the test): assert exact bytes for `+20.0`,
   `+0.0`, `-5.0`, `-40.0` (clamp low), `+127.0` (clamp high), `+200.0`
   (must clamp to `0x7F00`), `-200.0` (must clamp to `0xD800`).
2. `onThermostatMsgID16()` honour-counting and release transitions (drive
   it with a synthetic f88 stream; assert state-machine transitions).
   Requires lifting the file-static `otRemoteOverride` and
   `OT_OVERRIDE_*` constants behind a thin host-shim header, OR a more
   surgical extraction into a free function `applyHonourCycle(state, f88)
   -> NewState` that the test imports.
3. Sign extension regression: `f88 = 0xFB00`, `override = 0x0F00`
   -> delta exactly `0x1400` (5120, i.e. 20.0 °C), not `0xEC00`
   (60160 if interpreted unsigned). One assertion catches the 2026-Q1 bug.

The Dallas template already shows how to stub `PROGMEM`, `pgm_read_byte`,
`millis()`. The override state machine has no real `millis()` dependency
in the honour math itself — only the timestamp field — so it is
host-testable today with one `static uint32_t fakeMillis = 0;` stub.

### 3A-H2 — TASK-487 BLE parsers are not unit-tested despite known security risk

**Severity**: High.

`parseBLEAtcFormat()` and `parseBLEBTHomeFormat()` (SATble.ino:94-171) are:

- The attack surface for finding 2A-M2 (default-allow MAC filter) and
  2A-M3 (BTHome unknown-object truncation) — both Mediums, both still open.
- Pure functions over `const uint8_t*` byte arrays.
- Fully deterministic, no timing, no I/O.
- The exact shape of code that the project's `tests/README.md`
  recommends host testing for (last sentence: "Do not add anything that
  requires millis(), delay(), Serial, network stacks, or actual
  peripherals.").

There is no excuse — embedded constraint, build-budget, RTOS quirk —
for these to lack automated tests. Real-world BLE radios in range will
produce malformed payloads regularly (BTHome encryption-flag bit, partial
fragments under 13 bytes, unknown object-IDs the parser cannot length-skip,
maliciously crafted neighbour-broadcaster spoofs). The parsers' only
defence today is that the field has not yet seen a sensor whose
`object_id 0x00` byte appears before the temperature object.

**Recommendation**: Add `tests/test_ble_parsers.cpp` with at minimum the
cases below. Each row is one assertion; each is implementable in <5 lines
of test code. The functions themselves are 16 and 51 lines respectively;
the test will cover them at near-100% branch coverage.

| Case | Expected outcome |
|---|---|
| ATC: 13-byte valid (`temp_raw=2050` -> 20.50 °C) | parsed=true, temp=20.50 |
| ATC: 12-byte truncated | parsed=false |
| ATC: temp_raw=`0xF830` (-20.0 °C) | parsed=true, temp=-20.0 |
| ATC: temp_raw=`0xC8E0` (-141.6 °C, out of range) | parsed=false (sanity) |
| ATC: hum_raw=`0x4E20` (200.0 %, out of range) | parsed=false |
| BTHome: 3-byte valid temp-only | parsed=true |
| BTHome: flags=0x00 (no version bit) | parsed=false (security regression check) |
| BTHome: flags=0x41 (version + encrypted) | parsed=false (security: 2A-M3 boundary) |
| BTHome: unknown object 0xFF after valid temp | returns gotTemp=true (truncation behaviour locked in) |
| BTHome: unknown object 0xFF before temp | parsed=false (current bug per 2A-M3 — pin the behaviour, fix or document) |
| BTHome: temp at len boundary (`pos + 2 > len`) | returns gotTemp=false |

The 2A-M3 "unknown object before temperature drops the reading silently"
is captured directly in the fixture above. Whether the team chooses to
fix it (skip-byte policy) or document it as accepted-truncation, the
test pins the choice; today it is undocumented and untested.

## Medium findings

### 3A-M1 — TASK-494 `otCmdEnqueue` coalesce semantics has zero automated tests

`otCmdEnqueue()` (OTDirect.ino, 30 lines) is the new fan-in convergence
point with intentional position-preserving coalesce-by-MsgID semantics.
Contract claims (per fixture row + commit message):

- Two enqueues of the same MsgID before drain -> one slot, latest data.
- Two enqueues of different MsgIDs -> two slots, original order preserved.
- High-water counter increments only on actual depth growth, not on
  coalesce hits.
- Wraparound semantics across `OT_CMD_QUEUE_SIZE = 12` — the `i = (i + 1)
  % OT_CMD_QUEUE_SIZE` walk must terminate under all head/tail
  positions.

This is a 12-entry ring buffer of `uint32_t`; testing it on host has
*zero* embedded dependencies. A regression in the coalesce loop
(off-by-one on the modulo, wrong-direction walk, missing the "tail ==
head when full" case) would silently drop frames in the field — the
exact failure mode TASK-494 was created to prevent.

**Recommendation**: `tests/test_otdirect_queue.cpp`. Lift the queue
state (`otCmdQueue`, `otCmdHead`, `otCmdTail`,
`otCmdQueueHighWater`) and the four functions (`otCmdEnqueue`,
`otCmdQueueEmpty`, `otCmdQueueFull`, `otCmdQueueDepth`) into a thin
`otdirect_queue_host.h` shim included by both the firmware and the test.
Cases:

1. Empty queue: enqueue 5 distinct MsgIDs -> depth=5, no coalesce.
2. Coalesce: enqueue MsgID 16=0x1400, then MsgID 16=0x1500 -> depth=1,
   slot 0 holds 0x...1500.
3. Position: enqueue 1, 16, 8, then 16 again -> depth=3, slot 1 still
   second in iteration order, MsgID-16 data is the second value.
4. Full: enqueue 12 distinct -> 12th returns true (queue full at 11
   per `((head+1) % size) == tail` semantics: actual capacity is 11);
   13th returns false. Pin the off-by-one explicitly so a future
   "fix" doesn't silently bump capacity.
5. Wrap: drain 6, then enqueue 6 more -> high-water reflects peak, not
   current.
6. Coalesce after wrap: ensure the modulo walk traverses past the
   wrap point.

The fixture row "hardware: rapid TT=20 then TT=21 ... High-water-mark
counter remains low" is a hardware soak instruction — fine on its
own but it cannot detect the modulo bug, the off-by-one, or the
wrap-traversal bug. Host test catches all three.

### 3A-M2 — `bleMacToCompact()` boundary parsing has zero tests despite explicit format contract

`bleMacToCompact()` (MQTTstuff.ino:1928-1949) is a 22-line pure function
with explicit error contracts:

- Exactly 17 chars input or empty output.
- `outSize < 13` -> empty output.
- Non-hex digit -> empty output.
- Wrong colon position -> empty output.
- All hex digits lowercased.

This is the most testable function the session shipped, and the function
that downstream code depends on for a security-relevant property: the
HA discovery topic is built from the MAC, and a malformed MAC slipping
through would land in `<haPrefix>/sensor/<uniqueId>_ble_<bad_mac>_temp/config`
which the broker then accepts as a retained topic. An adversarial
`AA:BB::CC:DD:EE:FF` (extra colon) needs to fail closed.

**Recommendation**: 8 cases, ~40 lines of test:

| Input | outSize | Expected output |
|---|---|---|
| `"AA:BB:CC:DD:EE:FF"` | 13 | `"aabbccddeeff"` |
| `"aa:bb:cc:dd:ee:ff"` | 13 | `"aabbccddeeff"` |
| `"AA:BB:CC:DD:EE:F"` (16 chars) | 13 | `""` |
| `"AA:BB:CC:DD:EE:FFG"` (18 chars) | 13 | `""` |
| `"AA-BB-CC-DD-EE-FF"` (wrong sep) | 13 | `""` |
| `"AA:BB:CC:DD:EE:GG"` (non-hex) | 13 | `""` |
| `"AA:BB:CC:DD:EE:FF"` | 12 (too small) | `""` |
| `nullptr` | 13 | `""` |

### 3A-M3 — Cross-task BLE concurrency has no automated regression catch

The portMUX_TYPE snapshot pattern (TASK-497) is correct — three
independent reviewers landed on the same recommendation, code looks
right. But: a future refactor could drop the `portENTER_CRITICAL` /
`portEXIT_CRITICAL` pair around the slot-update block (lines 262-280
in SATble.ino), or could extend the critical section to include the
`SATBLEDebugTf()` call (which would block the BLE radio for milliseconds),
or could read `_bleSensors[i]` directly outside the lock. None of these
would fail to compile, none would fail `evaluate.py`, none are testable
without spinning up FreeRTOS.

**Recommendation**: This is genuinely hardware-only. The honest mitigation
is a **static check in `evaluate.py`** that any access to
`_bleSensors[` from outside the scan callback must be preceded within
~4 lines by `portENTER_CRITICAL(&_bleSensorsMux)`. This is a regex check;
it would be brittle but it would catch the most common refactor mistake.
Document the contract in a code comment is fine; it is what TASK-497 did.
Cost-benefit: I'd add the evaluate.py check only if the file has more
than two future cross-task readers; today there are two
(`satBLEUpdateState` and `satBLEPublishMQTT`), under the recurrence bar
ADR-080 sets for binding pattern-level enforcement. **Document this
explicitly in ADR-090 as a guideline-level pattern** rather than adding
a half-baked regex. That is a documentation task, not a test task.

### 3A-M4 — `00-scope.md` claim about evaluate.py changes is wrong

`00-scope.md` lists "evaluate.py — PROGMEM gate macro-continuation
tracking and `*.ino.cpp` exclusion" and "tests/test_evaluate.py — new
tests for the gate" as files in review. `git diff ace21a48..a61373b9 --
evaluate.py tests/test_evaluate.py` returns zero changes. Those changes
live in commit `ace21a48` itself, which is the *exclusive* lower bound
of the range. This already shows up as 1B-M4 (architecture phase) but
it directly affects the testing review's framing: a test reviewer
following the scope blindly would write recommendations for code that
isn't in the diff window. **Action**: clarify in scope manifest convention
whether ranges are inclusive or exclusive on the lower bound; both phase
1B and now phase 3 hit this.

### 3A-M5 — TASK-493 bool-return change has no test

`bleSensorPublishHaDiscovery()` was changed from `void` to `bool` so
`satBLEPublishMQTT` can gate `bDiscoveryPublished` on success. The
behaviour matters: if the helper returns true on partial success
(e.g. temp+rh published, bat failed, rssi failed), the caller flips
the flag and the rssi config is permanently missing from HA. Today
the helper returns false on the first sub-failure and the caller does
NOT flip the flag — correct, retry-on-next-cycle. But the contract is
in code, not in a test. A future refactor that adds "best-effort"
semantics would silently break HA discovery for partially-failing
sensors.

**Recommendation**: Annotate the function with a
`// CONTRACT: returns true iff ALL FOUR sub-publishes succeeded` comment
right at the signature. (Not a test per se — the function does network
I/O and is not host-testable without a mock MQTT client, which is heavier
machinery than the project warrants.) This is documentation hardening.

## Low findings

### 3A-L1 — Fixture validator naming is fine but undertested

`tests/check_otdirect_fixture.py` is a static format check; it has no
unit tests of its own. The new TASK-466 row added 11 fixture rows and
the validator passed silently. A regression in the validator (e.g.
removing the `is_command_table` 7-column gate) would not be caught.
This is meta-testing and is genuinely low priority — the validator is
~150 lines and rarely changes.

### 3A-L2 — Hardware-soak ACs are not lifted into host simulation where possible

The TASK-466 fixture says "hardware: send TT=20, observe `honoredCount`
increment as thermostat echoes 20.0 C". This is a real hardware test,
but the *honour count increment* itself is pure state-machine logic
already covered by 3A-H1 above. The fixture conflates the
unit-testable bit (count increments deterministically) with the
hardware-only bit (real thermostat echoes the value). Splitting these
in the fixture would make it clearer which rows can be lifted out of
hardware soak.

### 3A-L3 — No test for `clearRemoteOverride()` one-shot frame

The "one-shot WRITE_DATA MsgID 100 = 0x0000" emit in `clearRemoteOverride()`
is a UX detail mentioned only in code comments and the fixture. If a
future refactor moves the `otCmdEnqueue(clearFrame)` call inside an `if`
that no longer fires, the thermostat UI never flips back. Coverable as
part of 3A-H1 with one assertion: "after `clearRemoteOverride`,
queue-spy sees a frame with MsgID=100 and data=0x0000."

### 3A-L4 — `setRemoteOverride()` does not check `enqueueWriteCommand` return value

If the OT command queue is full (12 entries, depth-11 capacity), the
state machine sets `mode=TEMPORARY` and `f88Value=0x1400` regardless
of whether the frame actually made it onto the bus. This is the
1A-M1 finding from Phase 1A, still open. Per the AC-honour contract,
`onThermostatMsgID16` will then fire `honoredCount++` based on a state
the boiler never received. Marginal in practice (queue rarely full in
normal operation), but it survives the TASK-494 coalesce + high-water
addition because coalesce reduces, not eliminates, full conditions.

## Strengths observed

- **Fixture parity table is genuinely first-class artefact**:
  `tests/otdirect_pic_parity_fixture.md` is a curated, source-cited
  parity table with `gateway.asm` line-number citations. The TASK-466
  additions hold the bar — every new row cites either `gateway.asm`,
  OT v4.2 spec, or a specific TASK-ID. The negative-TrSet sign-extend
  row (TASK-491) names the exact failing byte (`0xFB00`) and the exact
  arithmetic that would mis-fire. As fixtures-as-documentation goes,
  this is good.
- **Static format validator runs in CI**:
  `tests/check_otdirect_fixture.py` is a format gate with `gh workflows`
  visibility. It catches table-column drift without compiling firmware.
- **`evaluate.py` covers the structural patterns it can**: TASK-482's
  PROGMEM-gate fix and the existing `is_hot_path_file`,
  `scan_string_usages_detailed`, `check_design_system_drift`,
  `otdirect_25238_bridge_regressions` checks are real regression catches.
  The session's *existing* hot-path additions (SATble, MQTTstuff) inherit
  these gates automatically — that is real testing leverage.
- **`tests/test_dallas_address.cpp` template is the right shape**: when
  the team has chosen to host-test, they have done it well. The pattern
  is reusable. The session simply did not invoke it for the new
  pure-logic functions (parsers, queue, override math).
- **Hardware-soak ACs are explicit and audit-trail-ready**: the TASK-466
  rows distinguish "host-testable now" from "hardware: send TT=20,
  observe X" — that distinction is correct for state-machine timing
  on a real thermostat.

## Concrete additions to consider

In priority order, highest leverage first:

1. **`tests/test_otdirect_override.cpp`** (3A-H1): clamp boundary +
   sign-extend regression + honour state machine. Lifts directly from
   `test_dallas_address.cpp` template. Estimate: 120 lines C++,
   captures 3 of the 4 Highs that landed this session in their actual
   numerical contract form. Highest leverage.
2. **`tests/test_ble_parsers.cpp`** (3A-H2): ATC + BTHome v2 parsers
   under deterministic byte-array inputs. Pins the BTHome encrypted-flag
   rejection (security 2A-M3 boundary) and the unknown-object truncation
   behaviour. Estimate: 200 lines C++, ~12 cases.
3. **`tests/test_otdirect_queue.cpp`** (3A-M1): coalesce-by-MsgID, full,
   wrap, high-water. Lift queue functions behind a thin `_host.h`
   shim. Estimate: 150 lines C++, ~6 cases. Catches modulo-walk and
   off-by-one regressions that hardware soak cannot.
4. **`tests/test_ble_mac.cpp`** (3A-M2): `bleMacToCompact()` 8 cases
   in ~40 lines. Smallest, easiest, most defensive. Pin the security
   contract that malformed MACs cannot leak into MQTT topic names.
5. **Hardware-test plan document** (3A-L2): split the TASK-466 fixture
   "hardware:" rows into rows-that-can-be-host-tested vs
   rows-that-genuinely-require-a-thermostat-echo. The team already
   knows the difference; the fixture should make it explicit so the
   next reviewer doesn't see "hardware:" everywhere and assume nothing
   is automatable.
6. **Document ADR-090 cross-task pattern as guideline-level** (3A-M3):
   not a test addition; explicit acknowledgement that the BLE portMUX
   snapshot pattern is the second instance of "shared scratch state
   accessed from re-entrant context". Two instances is below the
   ADR-080 binding bar; document it as guideline-level so the third
   instance (when it lands) has a name to refer to.
7. **Scope manifest convention clarification** (3A-M4): inclusive vs
   exclusive lower bound of the diff range. Both Phase 1B and Phase 3A
   hit the same evaluate.py false-positive. Document the convention
   in `.full-review/00-scope.md` template.

The first four items together are roughly 510 lines of C++, all
host-compilable with `g++ -std=c++17`, no Arduino dependencies, no
mocks beyond a `static uint32_t fakeMillis = 0;` stub. They cover the
entire numerical-contract surface of TASK-466 / TASK-491 / TASK-494 /
TASK-495 and the security-relevant byte-parsing surface of TASK-487.
The session shipped four Highs' worth of fixes and zero new automated
tests; closing that gap would put this session's net test coverage on
par with its net code coverage.
