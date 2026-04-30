# Phase 3: Testing, Documentation & ADR Audit

Consolidated from `phase3a-testing.md`, `phase3b-documentation.md`, and
`phase3c-adr-audit.md` (the latter added by owner request).

## Test Coverage Findings (Phase 3A)

**Counts**: 0 Critical, 2 High, 5 Medium, 4 Low.

### High

- **3A-H1**: 700+ lines of new firmware C++ shipped in the session;
  zero new executable tests. The numerical-contract fixes (TASK-491
  sign-extend, TASK-495 -40..127 clamp, TASK-466 honour/release deltas,
  f8.8 cast math) are not pinned by an automated check. Concrete
  proposal: `tests/test_otdirect_override.cpp` (~120 lines, 6
  assertions) leveraging the existing host-compilable C++ test template
  at `tests/test_dallas_address.cpp` + `tests/README.md`.
- **3A-H2** (companion): BLE byte-layout parsers (ATC/pvvx, BTHome v2)
  and BTHome encrypted-flag rejection are likewise unguarded.
  Recommended: `tests/test_ble_parsers.cpp` covering the parser
  boundary-cases.

### Medium / Low

- 3A-M1..5: queue-coalesce semantics (no host test), MAC-filter bypass
  scenarios, etc. See full report.
- 3A-L1..4: minor fixture additions and docstring suggestions.

### Note on scope manifest

3A flagged that the scope manifest (`00-scope.md`) erroneously claims
`tests/test_evaluate.py` and `evaluate.py` were touched in the diff
window. They live in the lower-bound commit `ace21a48` itself, not
strictly in `ace21a48..a61373b9`. This was already 1B-M4. Reasonable to
leave as-is for the final report.

## Documentation Findings (Phase 3B)

**Counts**: 0 Critical, 3 High, 5 Medium, 5 Low.

### High

- **3B-H1**: `iBleInterval` WebUI tooltip in
  `src/OTGW-firmware/data/index.js:5833` and `:5948` still describes
  the setting as "scan rate". Post-TASK-494 the actual semantics is
  publish/state-update cadence — the tooltip is now actively
  misleading. Quick fix: 2-line tooltip rewrite.
- **3B-H2**: The new per-MAC MQTT topic family
  `OTGW/sat/ble/<mac>/{temp,rh,bat,rssi}` (TASK-488) is completely
  absent from `docs/api/MQTT.md`. Non-HA integrators who script
  against the contract have no documentation to point to.
- **3B-H3**: `CHANGELOG.md` `[Unreleased]` does not mention any of the
  8 session commits, including the user-visible TT/TC behavioural
  change. Release-blocking gap once 2.0.0 cuts a build.

### Medium

- 3B-M1: ADR-092 prose does not yet reflect TASK-494's continuous-scan
  or TASK-497's portMUX (cross-references with Phase 3C-1 and 3C-2).
- 3B-M2: `c4-code-otdirect.md` and `c4-code-sat.md` have measurable
  drift — stale `BLEDevice.h` reference, 7-vs-8 entry override-table
  count, pre-TASK-466 `TC=` / MsgID 1 mapping.
- 3B-M3: ADR-077 conformance comment partially honoured — function
  comment is honest about single-buffer, but the block-comment 75
  lines above still claims "two-pass shape ADR-077 requires".

### Low

- 3B-L1..5: minor inline comment + cross-reference touch-ups.

### Strengths

- Inline source documentation is the standout strength: the
  sign-extension cast comment in `OTDirect.ino`, the cross-task model
  comment block in `SATble.ino:71-81`, and the TT/TC parity rows in
  `tests/otdirect_pic_parity_fixture.md` are all precise and
  actionable.

## ADR Audit Findings (Phase 3C, owner-requested)

**Counts**: 3 mandatory ADR-action items + 2 optional.

### Mandatory

- **3C-1 — ADR-090 drift**: ADR-090 sub-rule 4 says "no volatile
  required, cooperative single-threaded model" with an explicit
  foresight clause that FreeRTOS would invalidate it. TASK-497 added
  `portMUX_TYPE _bleSensorsMux` cross-task on ESP32-S3. The foresight
  clause fired; ADR-090 is unamended. Action: amend ADR-090 (or write
  ADR-094 as a sibling) covering the FreeRTOS cross-task case.
- **3C-2 — ADR-092 drift**: ADR-092 says "Periodic-scan model
  retained" with `BLE_SCAN_DURATION_SEC = 3`. TASK-494 (same release)
  removed both. Action: amend ADR-092 to reflect continuous-scan as
  the final implementation, plus the iBleInterval semantics shift.
- **3C-3 — ADR-068 drift + new coalesce semantics**: ADR-068
  documents `OT_CMD_QUEUE_SIZE = 8` (pre-existing drift; code is 12).
  Plus TASK-494's coalesce-by-MsgID semantics are uncovered. The
  TASK-494 plan-file claim that "ADR-064 / ADR-087 already cover the
  queue model" was incorrect — ADR-068 is the right neighbour, and it
  does NOT cover coalesce. Action: amend ADR-068 OR write ADR-093 for
  the coalesce semantics + queue-is-the-channel principle.

### Optional / defensible no-ops

- TT/TC state machine (TASK-466): PIC-parity, fixture-documented in
  `tests/otdirect_pic_parity_fixture.md`. No new ADR needed if the
  fixture is treated as the authoritative parity record. (Defensible.)
- ADR-077 bounded-payload exception (3B-M3): a one-line addendum to
  ADR-077 noting that single-buffer publish is acceptable when the
  payload is statically bounded would close the conformance gap, but
  is also defensible as "implicit in the spirit".

## Cross-phase signal

Three independent phase-3 tracks all surface ADR-drift or doc-drift on
the same set of architectural changes (TASK-466, TASK-487, TASK-488,
TASK-494, TASK-497). The pattern: when feature work moves fast and
multiple subsystems are touched, the architectural-record layer drifts
faster than the code layer. Phase 3C's audit is the highest-leverage
mitigation — three concrete amendments close out drift on three
ADRs that future code is expected to respect.

## Recommended action plan (post-Phase-3)

Priority-ordered, ahead of Phase 4:

1. **3B-H3** (CHANGELOG) — minutes of work, release-blocking when
   2.0.0 cuts. Fix first.
2. **3B-H1** (iBleInterval tooltip) — minutes of work, actively
   misleading users today.
3. **3C-1, 3C-2, 3C-3** (ADR amendments) — bundle into one
   docs commit. Closes the drift before any future PR builds on the
   stale ADR text.
4. **3B-H2** (MQTT API docs for per-MAC topics) — half-hour of work
   to add the new topic family with payload examples.
5. **3A-H1** (`tests/test_otdirect_override.cpp`) — 1-2 hour effort
   to author + verify; pins the four numerical-contract fixes against
   regression.
6. Mediums and Lows: defer to a docs-sweep follow-up commit.

## Phase 3 critical issues for Phase 4 context

Phase 4 will examine framework / language best practices and
CI/CD-DevOps. The Phase 3 findings that are likely to affect those:

- ADR drift (3C) means existing CI gates may be stale relative to the
  ADRs they enforce. Phase 4B (CI/DevOps) should cross-check.
- Lack of executable tests (3A-H1) is a CI-test-coverage concern that
  Phase 4B should weigh.
- WebUI tooltip drift (3B-H1) is a build-time concern (the static
  HTML/JS is part of the LittleFS image; outdated text reaches end
  users on flash).
