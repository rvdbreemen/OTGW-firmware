# Phase 3A: Test Coverage & Quality

## Summary

- **Critical gaps**: 4 must-cover invariants with zero verification (buffer-arithmetic, burst-cooldown timing, VH burst-quiesce symmetry, ghost ADR citations)
- **Recommended additions**: 6 new `evaluate.py` checks (all regex-level, five-minute cost each) + 1 promotion of the orphaned Dallas test to a host-compiled unit harness
- **Low-effort wins**: 3 (retire `bool definition_sites` dead local in ADR-064 gate; ADR number-exists grep; `incPublishedTopicCount()` call-site audit gate promised in ADR-062)

## Calibration note

This codebase is ESP8266 Arduino firmware. "Unit test coverage" as measured by line percentages is fantasy: every non-trivial function reaches into `Serial`, `ESP.getFreeHeap()`, `MQTTclient`, `LittleFS`, or `time(nullptr)`. There is no PlatformIO config (confirmed: `ls platformio.ini` returns absent), no unity framework, no mock harness. The single `tests/test_dallas_address.cpp` is Arduino-sketch-style — it assumes `Arduino.h` and `Serial`, which means it only runs on a board. It has no Makefile/CMake hook. It is effectively orphaned.

What matters for this branch is **invariants**: the new code introduces roughly fifteen discrete "this must always be true" statements, most of which can be verified either (a) by a regex-level `evaluate.py` check that takes under five minutes to write, or (b) by arithmetic the reviewer can do at their desk and encode as a static assertion. Neither requires a test runner.

The Phase 1 and Phase 2 reviewers already caught the findings that matter — this Phase 3A asks: "which of those findings would a cheap automated gate have caught before the reviewer had to?" Almost all of them. That is the real indictment of the current test posture: the bugs found were not subtle. They were arithmetic. Arithmetic is what computers do.

## What this branch should verify (Section 1 invariants)

Grouped by branch change, with file:line anchors from `dev..HEAD`.

**Discovery verification state machine** (`MQTTstuff.ino:186-340`)
- I1. Buffer always restored to `MQTT_CLIENT_BUFFER_SIZE` on every exit path. Covered in code at `MQTTstuff.ino:247`, `280`, `303`, `600` — four exit paths. No automation confirms all four are reachable.
- I2. `iPublishedTopicCount` reset to zero whenever the streaming phase restarts (`MQTTstuff.ino:1248`). Corollary: every streaming helper in `mqtt_configuratie.cpp` that writes a retained discovery topic must call `incPublishedTopicCount()`. ADR-062 promised a CI gate for this; it was never implemented.
- I3. Verify-window heap-abort must not mask true-missing (Phase 1A MED + Phase 2B MED-1). Invariant: three distinct outcomes (pass / missing / aborted) must be distinguishable in `state.discovery`.
- I4. `endDiscoveryVerification` is idempotent — second call is a no-op (guard `if (!verifyActive) return`). This is already coded but there is no unit witness.

**Status-burst cooldown** (`MQTTstuff.ino:87-118`)
- I5. `STATUS_BURST_COOLDOWN_MS < typical Status cadence`. Field data: 3s. Current constant: 10000ms. **Arithmetic violation**. Phase 2B HIGH-1.
- I6. `endStatusBurst()` is paired with a prior `beginStatusBurst()` on every path; 500ms self-heal is a safety net, not the primary contract.
- I7. VH publishers must wrap themselves in the same begin/end pair as non-VH publishers (`OTGW-Core.ino:1667-1732`). Phase 1A HIGH #1.

**Cumulative heap diagnostics** (`MQTTstuff.ino:1079-1109`)
- I8. `char json[384]` is sufficient for every worst-case `%lu` expansion of 13× `unsigned long` + 4× `unsigned/unsigned short` counters + format overhead. **Arithmetic failure**: 465 bytes needed. Phase 2B CRITICAL.
- I9. `getHeapHealth()` tier-transition counters only increment on genuine transitions (hysteresis). Currently no hysteresis → counters inflate. Phase 1A MED.
- I10. Hourly publish runs exactly once per hour (cadence) — depends on I12 below.

**Time-boundary dispatcher** (ADR-064, `OTGW-firmware.ino` + `helperStuff.ino`)
- I11. Each of `minuteChanged` / `hourChanged` / `dayChanged` / `yearChanged` has **exactly one caller**. **Already enforced** by `check_time_boundary_single_caller` in `evaluate.py:159-219` (this branch shipped it). This is the gold-standard pattern for the rest of this list.
- I12. First-minute-after-boot dispatch fires all consumers (sentinels are `-1`). Documented in Phase 1B MED, but not in ADR-064 Consequences.

**Nightly restart refactor** (`OTGW-firmware.ino`)
- I13. Restart fires at `settings.iRestartHour`, exactly once per day. Phase 1A MED: `minute() == 0` safety guard was lost during refactor. With `hourChanged()` gating the outer check, this is no longer required for correctness, but it is not verified.

**ADR governance** (cross-cutting)
- I14. Every `ADR-\d+` reference in source or docs resolves to an actual file in `docs/adr/`. Phase 1B CRITICAL: ADR-062 and ADR-064 cite ghost ADRs 077/078/080 that do not exist. `ls docs/adr | grep -E "07[78]|080"` returns nothing.
- I15. ADR files that claim binding CI gates actually have matching checks in `evaluate.py`. ADR-062 claims two (`check_discovery_counter_instrumented`, `check_publishedtopic_counter_reset`); only the ADR-064 pattern was wired up.

## CI state today

- **`evaluate.py` wired into CI**: **NO**. Confirmed — `ls .github/workflows/` shows only `opentherm-v42-spec-audit.yml` (runs `tools/opentherm_v42_spec_audit.py`) and `trigger-copilot-agent.yml` (issue-creation helper). There is no workflow that runs `python evaluate.py`. The script must be run manually by the developer.
- **`evaluate.py` current checks** (as of `dev..HEAD`):
  1. `check_code_structure` — required file presence, `#ifndef`/`#define` header guards on `.h` files.
  2. `check_build_system` — Makefile presence + `binaries|clean|upload|filesystem` targets.
  3. `check_version_info` — `_SEMVER_FULL` and `_BUILD` macro presence in `version.h`.
  4. `check_time_boundary_single_caller` — **new this branch** — ADR-064 gate for `minuteChanged`/`hourChanged`/`dayChanged`/`yearChanged`.
  5. `check_coding_standards` — counts `Serial.print` calls (except in `SetupDebug`), counts `String name = ` declarations.
  6. `check_memory_usage` — flags `char X[N]` where N > 1024.
  7. `check_dependencies` — extracts `lib install` from Makefile, checks `@version` pinning.
  8. `check_documentation` — README sections, comment ratio in `.ino` files.
  9. `check_security` — regex for hardcoded `password|passwd|pwd|secret|key`, unsafe `strcpy|strcat|sprintf|gets`.
  10. `check_git_repository` — branch name, clean tree, `.gitignore` presence.
  11. `check_filesystem_data` — `data/` file count and sizes.
- **Other CI gates**: `opentherm-v42-spec-audit.yml` runs on PRs to `main|dev|dev-*|copilot-*|release/**` branches, executing `tools/opentherm_v42_spec_audit.py`. Out of scope for this branch (no OT spec touch). That's it. No build-firmware CI, no lint, no test runner.

The single branch addition (`check_time_boundary_single_caller`, evaluate.py:159-219) is the correct template. It is ten lines of Python, catches a class of bug that is otherwise invisible, has a false-positive escape (comment-line skip at line 192), and runs in under a second. Every other finding below proposes more of the same.

## Findings

### [HIGH] `sendMQTTheapdiag` buffer size has no arithmetic gate

- **Invariant at risk**: I8 — `snprintf_P` into `char json[384]` silently truncates at 384 bytes; the math says worst-case output is 465 bytes. The malformed JSON is then published **retained** — broker holds the corruption indefinitely. Phase 2B CRITICAL.
- **Branch change that introduces it**: `src/OTGW-firmware/MQTTstuff.ino:1083`.
- **Proposed test**: `evaluate.py` check `check_json_buffer_arithmetic` — for each `snprintf_P(json, sizeof(json), PSTR(...), ...)` in the firmware, parse the format string, count `%lu=10`, `%u=5`, `%d=11`, `%s` (skip — variable), fixed literals, and compare against the buffer size. Fail if worst-case ≥ buffer. First pass can be scoped to `sendMQTTheapdiag` only — one regex to locate the function, one to parse the format. Extensible later.
- Alternative (even cheaper, same outcome): host-compiled one-file test that calls `snprintf` with `ULONG_MAX` in every counter slot and asserts the return < `sizeof(json)`. 20 lines of C99, no dependencies. Hook into `tests/` directory with a `Makefile` snippet.
- **Cost**: S (evaluate.py check, ~30 lines) or XS (arithmetic unit test, ~20 lines).

### [HIGH] `STATUS_BURST_COOLDOWN_MS` has no sanity gate against Status cadence

- **Invariant at risk**: I5 — if cooldown ≥ inter-burst gap, drip never ticks. Phase 2B HIGH-1.
- **Branch change that introduces it**: `src/OTGW-firmware/MQTTstuff.ino:107`.
- **Proposed test**: `evaluate.py` check `check_status_burst_cooldown_bound`. Regex for `STATUS_BURST_COOLDOWN_MS\s*=\s*(\d+)`; fail if value ≥ 3000 (documented typical Status cadence from tester logs), unless preceded within 5 lines by a comment containing the token `// verified tuning` (escape hatch for deliberate overrides). Ten lines of Python. Would have caught the `10000` default that ships today.
- **Cost**: XS.

### [HIGH] VH publishers lack a static-invariant gate on Status-burst wrapping

- **Invariant at risk**: I7 — `publishMasterStatusVHState` / `publishSlaveStatusVHState` do not call `beginStatusBurst` / `endStatusBurst`. Non-VH equivalents do (`OTGW-Core.ino:1568`, `1581`, `1609`, `1623`). Phase 1A HIGH #1 + Phase 2B HIGH-2.
- **Branch change that introduces it**: `src/OTGW-firmware/OTGW-Core.ino:1667-1732` (VH blocks never updated to match pattern).
- **Proposed test**: `evaluate.py` check `check_status_publishers_wrap_burst`. Regex finds every function whose name matches `publish(Master|Slave)Status.*State`. Inside each, count `beginStatusBurst(` and `endStatusBurst(` calls. Fail if the function has length > 5 lines AND zero burst calls AND name contains `Status`. Alternative: hardcode the list `publishMasterStatusState|publishSlaveStatusState|publishMasterStatusVHState|publishSlaveStatusVHState` and require both markers. Fifteen lines. Would have caught this directly.
- **Cost**: S.

### [HIGH] Ghost ADR references have no resolution gate

- **Invariant at risk**: I14 — readers cannot verify claims citing ADR-077/078/080 that do not exist. Phase 1B CRITICAL.
- **Branch change that introduces it**: `docs/adr/ADR-062-retained-discovery-verification.md:123-125`, `docs/adr/ADR-064-time-boundary-single-caller-contract.md:127`.
- **Proposed test**: `evaluate.py` check `check_adr_references_resolve`. Scan all files under `docs/adr/` and all `.ino`/`.h`/`.cpp` under `src/OTGW-firmware/` for the regex `ADR-(\d{3})`. For each unique match, check that `docs/adr/ADR-\1-*.md` exists. Fail on any unresolved reference. Twelve lines. Escape hatch: skip references in strings like `"ADR-NNN"` or `\bADR-\d+\s*(future|proposed|TBD)` if the team wants forward citations; otherwise strict.
- **Cost**: XS.

### [MEDIUM] `incPublishedTopicCount` call-site integrity gate (the CI gate ADR-062 promised)

- **Invariant at risk**: I2 — every streaming helper writing a retained discovery topic must call `incPublishedTopicCount()`. If a future `streamSomeNewDiscovery` helper is added without the call, `iPublishedTopicCount` under-counts → verify closes with `expected < actual` → spurious "missing" → unnecessary republish of all 80 topics daily.
- **Branch change that introduces it**: `src/OTGW-firmware/mqtt_configuratie.cpp` — 5 call sites at lines 2013, 2047, 2176, 2414, 2496. ADR-062 explicitly promises this CI gate (ADR-062:110, 123-125) but it is not in `evaluate.py`.
- **Proposed test**: `evaluate.py` check `check_discovery_stream_helpers_increment`. Find every function in `mqtt_configuratie.cpp` whose name starts with `stream` and ends with `Discovery`. For each, require at least one `incPublishedTopicCount(` call in its body. The body-extraction is fiddly in regex — practical shortcut: for each `bool streamXxxDiscovery(` definition, require the matching `incPublishedTopicCount()` call within the next 400 source lines (functions are ≤ 200 lines). Alternative, simpler but coarser: count the number of `bool stream\w+Discovery\s*\(` definitions and the number of `incPublishedTopicCount(` call sites in the same file, assert equal. 5/5 today, and any new stream helper either adds both or fails the gate. Ten lines.
- **Cost**: S.

### [MEDIUM] Verification buffer restoration — no guaranteed-reached gate

- **Invariant at risk**: I1 — four `setBufferSize(MQTT_CLIENT_BUFFER_SIZE)` restoration sites (`MQTTstuff.ino:247, 280, 303, 600`) cover four exit paths (setBufferSize-failed-early, normal-close, disconnect-fast-close, connect-time-defensive-restore). A fifth exit path added later could leak the 1024-byte buffer across reconnects.
- **Branch change that introduces it**: `MQTTstuff.ino:186-340` (new state machine).
- **Proposed test**: Manual walkthrough is the practical option here. The state-machine invariant is "every path that sets `verifyBufferResized = true` must reach a corresponding reset". A regex-level check would be a heuristic only: `count(verifyBufferResized\s*=\s*true)` ≤ `count(verifyBufferResized\s*=\s*false)` — insufficient but cheap. Better: keep this on the author's side as a code-review invariant, documented with a comment block at the top of the verify state machine enumerating all exit paths. Promote to assertion-in-code: add `ESP.getMaxFreeBlockSize()` sanity checks to `endDiscoveryVerification` that log if buffer is still >= 900 bytes after restore attempt.
- **Cost**: M (requires thinking). Recommend: document-the-invariant, not automate.

### [MEDIUM] `getHeapHealth` tier transition hysteresis — promote to unit test

- **Invariant at risk**: I9 — transitions increment on every boundary crossing, so telemetry counters `iEnteredLowCount` / `iEnteredWarningCount` / `iEnteredCriticalCount` inflate under boundary chatter. Phase 1A MED + Phase 2B MED-2.
- **Branch change that introduces it**: `src/OTGW-firmware/helperStuff.ino` (getHeapHealth + callers).
- **Proposed test**: This is the one piece of branch logic that is **actually host-testable**. `getHeapHealth(freeHeap, maxBlock)` is pure arithmetic over two integers — no ESP.* calls needed if we pass freeHeap and maxBlock as arguments. Sketch:
  ```c
  // tests/test_heap_health.cpp — host-compilable (gcc -DHOST_TEST)
  extern HeapHealth getHeapHealthFor(uint32_t freeHeap, uint32_t maxBlock);
  int main() {
    // Sweep 2048 → 6144, step 128, then back down — simulate boundary oscillation
    unsigned transitions = 0;
    HeapHealth prev = healthy;
    for (int cycle = 0; cycle < 10; ++cycle) {
      for (uint32_t fh = 2048; fh <= 6144; fh += 128) {
        HeapHealth h = getHeapHealthFor(fh, fh); // assume non-fragmented
        if (h != prev) ++transitions; prev = h;
      }
      for (uint32_t fh = 6144; fh >= 2048; fh -= 128) { /* same */ }
    }
    // With hysteresis: transitions should be ≤ 6 (3 thresholds × up+down, one pass).
    // Without hysteresis today: expect 60+ transitions. Assert ≤ 10.
    assert(transitions <= 10);
  }
  ```
  To enable, `getHeapHealth()` needs a pure wrapper that takes the two numbers as arguments (trivial refactor: current body already reads `ESP.getFreeHeap()` and `ESP.getMaxFreeBlockSize()` once at the top — extract them). Host-compiled with `gcc tests/test_heap_health.cpp -o test_heap_health && ./test_heap_health`. Runs in ms.
- **Cost**: S (refactor + test, ~60 lines total).

### [LOW] ADR-064 gate has a dead local variable

- **Invariant at risk**: none — maintainability only. `definition_sites` list is populated but never read (`evaluate.py:183, 194`). Dead code in a governance gate looks bad.
- **Branch change that introduces it**: `evaluate.py:183`.
- **Proposed fix**: Either use it (emit an INFO result listing where the helper is defined) or delete it. Three-line change.
- **Cost**: XS.

### [LOW] `iRepublishTriggeredCount` / `iVerifyRunCount` — telemetry semantics unit test

- **Invariant at risk**: `iVerifyRunCount` increments at start of verify (Phase 2A LOW); counter inflates on broker flaps. Not a test gap as much as a design gap — but a host-compiled state-machine test that drives begin/tick/end through all five exit paths and asserts counter deltas match a fixed expectation would make the semantics explicit and catch future regressions.
- **Proposed fix**: Skip for now. Phase 2A already flagged this as a LOW; the semantics decision (increment-on-start vs increment-on-clean-close) should be resolved first.
- **Cost**: M (and premature).

### [LOW] Dallas address conversion test — orphan disposition

- **Invariant at risk**: `tests/test_dallas_address.cpp` tests `getDallasAddress()`. Not in this branch's scope, but worth addressing.
- **Proposed action**: Either (a) delete it (currently it assumes `Arduino.h`, runs on a board, has no automation hook — so it provides zero ongoing value); or (b) rewrite as a host-compiled test without `Arduino.h` (the function is 8-byte hex conversion, no MCU dependencies). Recommend (b): ~30 lines, proves the test model is viable and gives us a second reference for future host tests. If (b) is too much work, (a) is better than leaving it in limbo.
- **Cost**: S (rewrite) or XS (delete).

## Phase 1/2 findings that a test would have caught

| Finding | Proposed automated check | Cost | Would-have-caught |
|---|---|---|---|
| VH publishers missing burst quiesce (1A HIGH #1, 2B HIGH-2) | `check_status_publishers_wrap_burst` (regex) | S | Yes — at commit time |
| `sendMQTTheapdiag` buffer overflow (2B CRITICAL) | `check_json_buffer_arithmetic` OR host-compiled snprintf test | XS | Yes — deterministically |
| `STATUS_BURST_COOLDOWN_MS` stall (2B HIGH-1) | `check_status_burst_cooldown_bound` (regex with escape hatch) | XS | Yes — the constant itself trips it |
| Ghost ADR-077/078/080 (1B CRITICAL) | `check_adr_references_resolve` (regex + file existence) | XS | Yes — immediately |
| `incPublishedTopicCount` coverage on new stream helpers (1B HIGH) | `check_discovery_stream_helpers_increment` (count-match regex) | S | Future additions, yes |
| Stale comments (1A HIGH #2, #4) | None practical — natural language | — | No |
| Hard-coded `6000` in REST (1A HIGH #3) | `evaluate.py` check for magic numbers vs named constants could flag; fuzzy and noisy | M | Maybe |
| Verify heap-abort masks failure (1A MED, 2B MED-1) | Host test driving begin/tick with simulated low heap, asserts outcome enum | M | Yes, if the outcome-enum refactor happens |
| `getHeapHealth` transition inflation (1A MED, 2B MED-2) | Host test (sketch above) | S | Yes — deterministically |

Seven of the nine findings Phase 1 and Phase 2 rated MEDIUM-or-higher are catchable by evaluate.py checks costing under an hour total. The remaining two (stale comments and heap-abort outcome semantics) are not automation-tractable but are catchable by a five-minute refactor each.

## Existing test assets assessment

- **`tests/test_dallas_address.cpp`**: Arduino-sketch-style test for hex-digit conversion. 143 lines. Assumes `Arduino.h` and `Serial`, so only runs on a physical board. No Makefile/CMake/PlatformIO hook. Orphaned — no way to run it in CI. Function under test (`getDallasAddress()`) is present in the firmware and the test logic is sound, but the harness is unusable. **Recommendation**: rewrite as host-compiled C++ (`gcc tests/test_dallas_address.cpp -o x && ./x`) using stub `typedef uint8_t DeviceAddress[8]` and stub `pgm_read_byte` → `*`. ~30 lines of edits. Establishes the pattern for future host tests (heap-health hysteresis, snprintf arithmetic).
- **`tests/otgw_simulation.log`**: 14KB log file, not clear from filename whether this is a captured fixture for future replay testing or stale artefact. Out of scope for this branch (not touched by `dev..HEAD`).
- **`evaluate.py`**: 785 lines. One check added on this branch (`check_time_boundary_single_caller`) — the gold-standard template. Eleven check functions total. Runs manually, not in CI. Covers structure, build config, coding standards (loose), memory (`char X[N>1024]` — fixed threshold), dependencies, documentation, security (hardcoded creds + unsafe strops), git, filesystem, versioning, ADR-064 single-caller. **Does not cover**: JSON buffer arithmetic, constant-value sanity bounds, ADR file resolution, `incPublishedTopicCount` call-site symmetry, Status-burst wrapping symmetry, heap-health hysteresis. All of those are regex-level additions on the same model as `check_time_boundary_single_caller`.
- **Conditional compilation for tests**: Grep for `TEST_BUILD` / `#ifdef TEST` / `ARDUINO_ARCH_NATIVE` / `__NATIVE__` under `src/` returns **no matches**. There is no host-compilation toggle in the firmware. Adding one would enable the heap-health test sketched above; without it, the refactor-and-extract approach (pass freeHeap/maxBlock as arguments) is the cleaner path.
- **`.github/workflows/`**: Two files. Neither runs `evaluate.py` or any firmware-level test. The Copilot workflow is an issue-creation helper. The OpenTherm v4.2 spec audit runs `tools/opentherm_v42_spec_audit.py` on PRs to the long-lived branches. **Recommendation for a follow-up task (not this branch)**: add a third workflow `evaluate.yml` that runs `python evaluate.py --quick` on the same branch set. Even without new checks, this alone would turn the ADR-064 single-caller gate from "manual discipline" into "automated enforcement", which is what ADR-080 (ghost — see finding above, but the concept is sound) claims to require.

## Synthesis

The branch is not under-tested in the naive sense — it is under-**gated**. Every HIGH and CRITICAL finding from Phase 1 and Phase 2 falls into one of three buckets: arithmetic the reviewer did by hand (buffer sizes, cooldown timings), regex-level symmetry (burst-wrap on non-VH but not VH, incrementer-call on 5/5 helpers but not guaranteed on 6/6), or reference integrity (citations to files that don't exist). All three buckets are evaluate.py material. The branch already shipped the template (`check_time_boundary_single_caller`). The next step is another five checks in the same shape, totalling maybe 80 lines of Python, and wiring `evaluate.py` into CI on PRs to `dev|1.4.*|release/**`. That single PR would convert this phase's findings from "issues the next release-candidate reviewer will have to re-find" into "issues that fail CI before a human sees them". Mostly harmless to add. Worth adding.
