# Phase 1A: Code Quality Review

Range reviewed: `ace21a48..fa5ef3c5` (8 commits) on `feature-dev-2.0.0-otgw32-esp32-sat-support`.

## Summary

Overall code health for the in-scope changes is good. The session shows
sustained discipline: PROGMEM is observed, no `String` slips into the OTDirect
or MQTTstuff hot paths, the new TT/TC state machine is small and testable,
the NimBLE port deletes more code than it adds, and TASK-489-491-492 are the
sort of self-initiated review follow-ups that tighten correctness rather than
add scope. The new `evaluate.py` helper `collect_firmware_source_files` is a
clean DRY consolidation of eight near-identical glob blocks.

The findings below are mostly Medium / Low. The only finding I would call
out for fixing before the next release is the discovery-publish flag being
set unconditionally even when the publish fails, which can permanently
suppress HA discovery for a sensor until reboot or slot recycling. The
remaining items are smaller correctness corners (queue-failure semantics in
`setRemoteOverride`, single-byte sentinel `0xFFFF`, macro-continuation
heuristic) and naming / cohesion nits.

## Critical findings

(None.)

## High findings

### H1. `bDiscoveryPublished = true` is set even when discovery publish failed

- **File**: `src/OTGW-firmware/SATble.ino:439-442`
- **Severity**: High
- **Description**: The caller pattern is

  ```cpp
  if (!_bleSensors[i].bDiscoveryPublished) {
    bleSensorPublishHaDiscovery(macCompact, _bleSensors[i].sMacAddress);
    _bleSensors[i].bDiscoveryPublished = true;
  }
  ```

  `bleSensorPublishHaDiscovery` is `void`. Inside it, every gating step
  (`!settings.mqtt.bEnable`, `!MQTTclient.connected()`, broker IP invalid,
  `!canPublishMQTT()`, `platformFreeHeap() < MQTT_DISCOVERY_HEAP_MIN`,
  `beginMqttPublish` failure, `endPublish` failure, `snprintf_P` truncation)
  silently returns. The caller has no signal and unconditionally flips the
  flag. From that point onward the slot is treated as "already discovered"
  and HA discovery is never retransmitted for that MAC until either:
  - the device reboots (flag is RAM-only), or
  - the slot is recycled to a different MAC (the new TASK-489 reset hook).

  The two realistic failure paths in production are: discovery fires during
  a low-heap window (very plausible: BLE scan + autoconfigure burst overlap
  is exactly the heap pressure the tier machine guards against), or MQTT
  isn't connected yet during the first scan after boot (`state.mqtt.bConnected`
  briefly false). In both cases HA never sees that sensor.

- **Fix recommendation**: Make `bleSensorPublishHaDiscovery` return `bool`
  (true only when all four sub-publishes succeeded), and gate the flag flip
  on success:

  ```cpp
  // SATble.ino
  if (!_bleSensors[i].bDiscoveryPublished) {
    if (bleSensorPublishHaDiscovery(macCompact, _bleSensors[i].sMacAddress)) {
      _bleSensors[i].bDiscoveryPublished = true;
    }
  }
  ```

  And update the forward declaration in `OTGW-firmware.h:256` to match.
  The four `bleSensorPublishOneDiscovery` calls inside already return
  `bool`; they currently `return` from the outer void on failure, so the
  threading is straightforward.

  Same issue applies to `bleSensorPublishStateTopics` only in a milder
  form (per-scan retransmits absorb single failures), so that one can stay
  `void`.

## Medium findings

### M1. `setRemoteOverride` enqueues two frames and silently keeps state on partial failure

- **File**: `src/OTGW-firmware/OTDirect.ino:1888-1905`
- **Severity**: Medium
- **Description**: `setRemoteOverride` writes the state struct first, then
  calls `enqueueWriteCommand(16, ...)` and `enqueueWriteCommand(100, ...)`.
  Both calls go through `otCmdEnqueue` (line 1808) which can fail with
  `queue full`. On failure `enqueueWriteCommand` logs and returns; it does
  not roll back. Result: `otRemoteOverride.mode == TEMPORARY` and
  `state.otd.eOverrideMode == TEMPORARY` are claimed, but no MsgID 16 or
  no MsgID 100 frame is in flight. The thermostat sees no override and
  `PR=O` falsely reports `O=T<value>`. The honour-detection state machine
  then waits for a thermostat echo that will never come.

  In practice this is rare (the OT command queue is typically empty when
  TT= arrives over Telnet/MQTT), but the failure mode is silent and the
  resulting "ghost override" survives until the next TT=/TC= command or
  reboot.

- **Fix recommendation**: Two pragmatic options:
  1. **Best-effort with diagnostic** (closest to current PIC behaviour):
     leave the state set, but log at `DebugTln` (not just OTDDebug) when
     queue-full happens during `setRemoteOverride` so the symptom is
     visible in the standard log, not behind a debug toggle.
  2. **Atomic enqueue**: change `enqueueWriteCommand` to return `bool`,
     attempt the MsgID 16 enqueue first; on failure leave state at NONE
     and skip MsgID 100. On MsgID 16 success but MsgID 100 failure, the
     boiler-side override still works (boiler ignores MsgID 100), so
     accept the partial success but log it.

     ```cpp
     static bool enqueueWriteCommand(uint8_t msgId, uint16_t dataValue,
                                     const char* label);
     // ...
     if (!enqueueWriteCommand(16, f88, ...)) {
       // do not commit state; let caller observe failure
       return;
     }
     // commit state, then try MsgID 100 (best-effort)
     ```

     This is more invasive but it removes the ghost-state class entirely.

### M2. `lastThermostatVal = 0xFFFF` is a sentinel that collides with a valid f8.8 value

- **File**: `src/OTGW-firmware/OTDirect.ino:1895, 1911, 2031` and `OTDirecttypes.h:73`
- **Severity**: Medium
- **Description**: `OTRemoteOverrideState::lastThermostatVal` is `uint16_t`
  and `0xFFFF` is used as the "never observed" sentinel. As an f8.8 value,
  `0xFFFF` decodes to -1/256 = -0.00390625 °C, a perfectly legal (if odd)
  reading. The current code only writes `lastThermostatVal` and never reads
  it for control flow (the honour delta is computed against
  `f88Value`, not `lastThermostatVal`), so today this is harmless. But the
  field is documented as a sentinel-bearing comparand and that contract
  will silently break the first time someone wires honour detection to
  `lastThermostatVal`.

- **Fix recommendation**: Either:
  - Change the type to `int32_t` and use `-1` as the sentinel, leaving the
    full f8.8 range valid, or
  - Add a separate `bool bLastThermostatValValid` companion field and drop
    the sentinel entirely.

  The latter is more in line with ADR-051 Hungarian-prefix conventions
  already in use elsewhere in `OTDirectSection`.

### M3. `prev_was_continuation` doesn't survive blank-line interruptions correctly

- **File**: `evaluate.py:1770-1787` (PROGMEM gate macro continuation tracking)
- **Severity**: Medium
- **Description**: The new logic correctly skips the body of a multi-line
  `#define`. But consider a malformed continuation like:

  ```
  #define FOO(x) \
      do { something(x); } while (0) \
  // trailing comment, no backslash
  ```

  Line 1: `stripped` starts with `#define`, sets `prev_was_continuation = true`.
  Line 2: continuation skipped, ends with `\`, keeps chain alive.
  Line 3: `stripped` starts with `//`. Per the current code, we fall into
  the comment-skip branch which **resets** `prev_was_continuation` to
  `stripped.endswith('\\')` (false), correctly ending the chain. OK there.

  But the more realistic case:

  ```
  #define FOO(x) \
      do { something(x); } while (0)

  void normalCode() { snprintf("not in F()", ...); }
  ```

  Line 1: `prev_was_continuation = True`
  Line 2: skipped, no trailing `\`, sets `prev_was_continuation = False`. OK.
  Line 3 (blank): `stripped == ''`, doesn't match the comment/define branch,
  falls through to `prev_was_continuation = False` then pattern match.
  None of the patterns match an empty line, so OK.
  Line 4: detected. OK.

  So I don't think this misclassifies in practice. But the heuristic is
  fragile in two ways:
  1. It uses `stripped.strip()` which trims trailing whitespace; a literal
     `\` followed by a space at end-of-line would be classified as
     continuation, which matches what GCC warns about but doesn't actually
     continue the macro. False-negative for the gate; harmless either way.
  2. It does not handle `/* multi-line block comments */` that contain a
     `#define` mention. Out of scope of this change but worth a note.

- **Fix recommendation**: Optional — add a single-line comment near the
  loop documenting the heuristic limits, and a 2-line test in
  `tests/test_evaluate.py` that asserts a `#define FOO \\nbody\n` macro
  body is *not* counted as a violation. The test_evaluate file diff in
  the commit range does not show any new tests; the commit message
  claims "tests for the gate" but the only test diff observable in
  `git diff -- tests/` is the OTDirect parity fixture. Worth confirming
  whether the test landed.

### M4. `bleSensorPublishOneDiscovery`: payload length is unsigned-cast after a signed-int check

- **File**: `src/OTGW-firmware/MQTTstuff.ino:2037-2068`
- **Severity**: Medium
- **Description**: `int n = snprintf_P(...)`; the check is

  ```cpp
  if (n <= 0 || (size_t)n >= sizeof(payload)) { ... return false; }
  ```

  Then `(size_t)n` is passed into `beginMqttPublish` and `writeMqttChunk`.
  The `n <= 0` short-circuit handles the GLIBC error case (`n < 0` for
  encoding error), and the `(size_t)n >= sizeof(payload)` handles
  truncation. This is correct as written, but the cast pattern is exactly
  the kind of thing that bites later if someone "simplifies" by removing
  the `n <= 0` guard. Worth a comment, or use a `static_cast<int>` /
  `static_cast<size_t>` pair to make the intent explicit.

- **Fix recommendation**: Low priority. Either add `// guard: n>0 before
  cast to size_t` or restructure to:

  ```cpp
  if (n < 0) { /* encoding error */ return false; }
  size_t plen = (size_t)n;
  if (plen == 0 || plen >= sizeof(payload)) { /* truncated */ return false; }
  ```

### M5. The state-topic format string and the discovery `stat_t` format string are duplicated in two places

- **File**:
  `src/OTGW-firmware/MQTTstuff.ino:1973` (`PSTR("sat/ble/%s/%s")`) and
  `src/OTGW-firmware/MQTTstuff.ino:2031-2033` (`PSTR("%s/value/%s/sat/ble/%s/%s")`)
- **Severity**: Medium
- **Description**: `bleSensorPublishStateTopics` builds topics like
  `sat/ble/<mac>/<kind>` (relative — `sendMQTTData` prepends
  `MQTTPubNamespace`).
  `bleSensorPublishOneDiscovery` builds the corresponding `stat_t`
  payload using a different absolute form:
  `<topTopic>/value/<uniqueId>/sat/ble/<mac>/<kind>`.

  The two have to agree (HA subscribes to the path inside `stat_t` and
  it must match what we publish). They are derived independently from
  format strings, with no shared constant or helper. If anyone later
  changes the namespace shape (e.g. `sat/ble/v2/...`) they will almost
  certainly fix the publish side and miss the discovery side, breaking
  every existing HA dashboard silently.

  Note: this same anti-pattern already exists in the OT auto-config code
  (each sensor's discovery stat_t is built independently of the publish
  topic), so this is not a new project-wide problem — but the new code
  reproduces it.

- **Fix recommendation**: Define a single `PSTR("sat/ble/%s/%s")` helper
  or constant and use it in both places. The compact form means the
  publisher uses the relative version (gets `MQTTPubNamespace/` prepended
  in `sendMQTTData`) and the discovery composes
  `PSTR("%s/value/%s/" SAT_BLE_TOPIC_FMT)`. Trivial macro composition,
  no new headers needed.

## Low findings

### L1. Magic literal labels passed to `enqueueWriteCommand` are not consistent across project

- **File**: `src/OTGW-firmware/OTDirect.ino:1900, 1904`
- **Severity**: Low
- **Description**: `enqueueWriteCommand(16, f88, mode == OT_OVERRIDE_TEMPORARY ? "TT" : "TC")`
  and `enqueueWriteCommand(100, flags, "OVR-flags")`. The label is used
  only by `DebugTf`. The rest of the function uses 2-letter PIC labels
  (`CS`, `C2`, `CC`, `SW`, `SH`, `MM`, `OT`, `VS`); `OVR-flags` is the
  outlier. Not wrong, but inconsistent.
- **Fix recommendation**: Consider `OF` (override-flags) or `O1` to
  match the 2-char PIC style. Cosmetic.

### L2. `clearRemoteOverride` calls `clearWriteOverride(100)` then immediately enqueues a fresh MsgID 100 = 0 frame

- **File**: `src/OTGW-firmware/OTDirect.ino:1915-1926`
- **Severity**: Low
- **Description**: The dance is:
  1. `clearWriteOverride(16)` — turn off the periodic-rewrite for MsgID 16.
  2. `clearWriteOverride(100)` — turn off the periodic-rewrite for MsgID 100.
  3. Build a one-shot `WRITE_DATA MsgID 100 = 0` and enqueue it directly.

  Step 2 followed by step 3 reads as "I need MsgID 100 cleared but also
  one final MsgID 100 = 0 sent". The comment explains why (UI consistency
  on the thermostat) but the code shape doesn't reflect that. A reader
  may wonder if step 2 is redundant or if step 3 is wrong.

- **Fix recommendation**: The comment on lines 1917-1918 already explains
  the intent. Could be tightened to "Step 2 stops the override; step 3
  injects one more frame so the thermostat UI hops out of override mode
  immediately rather than at the next periodic rewrite that would never
  come". Or split into a tiny helper `enqueueOneShotWrite(msgId, data)`.

### L3. ADR-092 verification gates section claims "Bluedroid blocking-call behaviour documented at the pre-change SATble.ino line" but the code is gone after the diff

- **File**: `docs/adr/ADR-092-...md:118-121`
- **Severity**: Low
- **Description**: The Evidence gate cites
  `_pBLEScan->start(BLE_SCAN_DURATION_SEC, false)` as the synchronous
  blocking call. Post-merge, that line no longer exists in `SATble.ino`.
  Anyone reading ADR-092 in three months and trying to verify the claim
  will need git archaeology.
- **Fix recommendation**: Anchor by SHA: cite
  `SATble.ino:317@c34d338d` (or the parent commit `ace21a48^`) and add
  the file URL with that ref. Tiny doc-quality nit.

### L4. NimBLE callback receives a `const NimBLEAdvertisedDevice*` and never null-checks it

- **File**: `src/OTGW-firmware/SATble.ino:194`
- **Severity**: Low
- **Description**: `void onResult(const NimBLEAdvertisedDevice* dev) override`.
  All subsequent code dereferences `dev` unconditionally
  (`dev->getAddress().toString().c_str()`, `dev->getRSSI()`, etc.).
  The NimBLE 2.x API contract says the pointer is non-null when the
  callback fires, so this is fine; mentioning for completeness only.
- **Fix recommendation**: None. (Could `assert(dev != nullptr)` for
  defensive intent, but the project doesn't use asserts in production
  code.)

### L5. `BLE_STALE_MS = 300000` and `BLE_SCAN_DURATION_SEC = 3` mix unit suffixes inconsistently

- **File**: `src/OTGW-firmware/SATble.ino:37-38`
- **Severity**: Low
- **Description**: One constant is `_MS`, the next is `_SEC`. The mixed
  suffix is a long-standing convention in this file (the diff only
  removes `BLE_SCAN_INTERVAL_MS` and tweaks comments), so this is
  pre-existing and out of strict scope. Flagging as a follow-up only:
  consider unifying on millis everywhere; the `_SEC` constant is passed
  as-is to `_pBLEScan->start(seconds, ...)` so it has to stay seconds for
  that call site, but a `static_assert(BLE_STALE_MS / 1000 > BLE_SCAN_DURATION_SEC)`
  would catch a future mis-tuning.
- **Fix recommendation**: Optional `static_assert`.

### L6. `bleMacToCompact` rejects malformed MAC by writing empty string but does not log

- **File**: `src/OTGW-firmware/MQTTstuff.ino:1942-1968`
- **Severity**: Low
- **Description**: A malformed MAC produces silent empty output, which
  the caller treats as "skip this slot" via `if (macCompact[0] == '\0')
  continue;`. There's no observability into how often this fires.
  Not a correctness issue (the input is the BLE library's own
  formatted MAC, which is well-formed by construction), but if the API
  ever changes upstream the failure mode is invisible.
- **Fix recommendation**: One-line `MQTTDebugTf(PSTR("[ble-disc] malformed
  MAC %s\r\n"), macWithColons);` before the early return at line 1955 /
  1959. Hidden behind the existing MQTT debug toggle, no production cost.

### L7. `stripped = line.strip()` change in evaluate.py PROGMEM scanner could mask leading-tab violations

- **File**: `evaluate.py:1772`
- **Severity**: Low
- **Description**: The original used `line.lstrip()`, the new code uses
  `line.strip()`. Functionally for the comment/define test the trailing
  whitespace doesn't matter, except for the `endswith('\\')` check, which
  was the whole point. So this change is correct and necessary. Worth a
  one-line comment explaining why both ends are stripped (otherwise a
  future "let's go back to lstrip for symmetry" patch breaks the
  continuation detection).
- **Fix recommendation**: Add a comment:
  `# strip() not lstrip(): trailing whitespace would mask the backslash`.

### L8. `bDiscoveryPublished` field placement breaks alphabetical-by-prefix ordering in `BLESensorData`

- **File**: `src/OTGW-firmware/SATble.ino:54-65`
- **Severity**: Low
- **Description**: `BLESensorData` puts `bValid` then `iLastSeenMs` then
  `bDiscoveryPublished` (the new field). ADR-051 prescribes Hungarian
  prefixing (`b`/`s`/`i`/`f`) for the settings/state structs; this is
  a local module struct, not a settings/state section, so ADR-051
  technically doesn't bind. Still, having two `b` fields with an `i`
  field between them is a visual irritation.
- **Fix recommendation**: Move `bDiscoveryPublished` next to `bValid`.
  Cosmetic. One-line move.

## Strengths observed

- **Sign-extension fix (TASK-491)** is exactly the kind of defensive
  correctness fix the project values: the code handles a case that
  almost certainly never fires in production (negative TrSet) but the
  cost is two casts and the comment carries the rationale forward.
  This earns its place in the codebase.
- **TASK-489 slot-recycling reset** is a clean follow-up: the bug
  description (`bDiscoveryPublished` flag survives slot recycling) is
  precise, the fix is one `strcmp + assignment`, and the comment in
  `SATble.ino:248-255` explains the invariant succinctly.
- **TASK-492 consolidation** is a real DRY win — the old
  `bleMacCompactLocal` was a textbook copy-paste of code that already
  existed in MQTTstuff. Eliminating it is unambiguously better.
- **`evaluate.py` helper extraction** (`collect_firmware_source_files`)
  removes 8 near-identical 3-line blocks and adds the `*.ino.cpp`
  exclusion in one place. Solid maintenance work.
- **No `String` regressions in hot paths**: `std::string` for NimBLE
  service-data is the right tool (it's a transient call-site local,
  not stored); the older `String svcData = ...` block is gone.
- **PROGMEM compliance**: Every new debug message uses `F()` or
  `PSTR()`; every new format string uses `snprintf_P`; every comparison
  uses literal char (no `strcmp` with PSTR-incompatible literals).
- **ADR-090 re-entrancy guard pattern**: the new code does not introduce
  any new shared scratch buffer that would need a guard. The BLE
  helpers all use stack-local buffers under the discovery heap-min
  gate, so they cannot collide with `doAutoConfigure`'s scratch.
- **OT v4.2 spec citations** in code comments
  (`OTDirect.ino:1875-1882`, `OTDirecttypes.h:54-60`,
  `tests/otdirect_pic_parity_fixture.md`) make the TT/TC state machine
  reviewable against the PIC `gateway.asm` ground truth without a
  spec PDF in the other monitor.
- **Async BLE scan** removes a latent watchdog risk (3-second blocking
  scan in `loop()`); the 3-arg `start(duration, isContinue, restart)`
  form with `restart=true` correctly handles overlapping scan windows.
