# ADR-080 Binding ADR rules must have a CI gate

## Status

Accepted (2026-04-19)

## Context

The 2026-04-18 multi-dimensional code review (security, performance, architecture, testing, accessibility) surfaced a recurring pattern: ADRs marked `Status: Accepted` and labeled binding had drifted quietly into non-compliance because no automated check enforced them.

Concrete example: **ADR-004 "No String in hot paths"**. The rule has been in effect since well before v1.3.5 and is referenced from `CLAUDE.md`. The review's widened String-class regex (TASK-296) found **19 existing String usages** in hot-path files (`SATble.ino`, `SATweather.ino`, `OTGW-Core.ino`, `restAPI.ino`). None were intentional exceptions; they had simply accumulated over many PRs because:

- The pre-review `evaluate.py` regex matched only `String foo = ...`, missing `String foo;`, `String foo(x);`, and `String& foo`.
- The threshold was set at 5 with WARN severity rather than FAIL.
- No test ever proved the regex still caught what it claimed to catch.

The rule was binding on paper and guideline in practice. Reviewers (human and AI) could not consistently catch fresh violations during PR review because the muscle memory for "check every new `String`" was never trained by a tool.

Similar risk exists for other pattern-level rules:

- **PROGMEM compliance** (every string literal in flash): already has a check in `evaluate.py`. Good.
- **No ArduinoJson**: has a check. Good.
- **Typed control flow (no string tokens as discriminators)** from `CLAUDE.md`: no check. ADR-078's 76-branch dispatch-table refactor was only possible because a reviewer manually read the block. A 76-branch chain can grow again between reviews.
- **`static char` + `strlcpy` over `String`** for request handlers: no direct check.
- **No `Serial.print` after OTGW init**: has a check in `evaluate.py` (`check_coding_standards`).

ADRs that describe *system-level* choices (HTTP-only LAN, dual encapsulating settings/state structs, single-point-of-instantiation) are not amenable to automated checks in the same way; their compliance is reviewed structurally during architecture review, not line-by-line.

Alternatives considered:

1. **Keep the status quo**: binding rules + manual review. Rejected because the review already showed this produces quiet drift; the same ADRs will regress again.
2. **Make every binding rule FAIL in CI immediately**: rejected because it creates a large cleanup dependency on every existing violation before any new PR can merge. The String-class case alone blocks 19 cleanups.
3. **Add CI gates gradually, and be honest about which ADRs lack one** (chosen). Distinguish pattern-level ADRs (must eventually be gated) from system-level ADRs (not expected to be gated). When a pattern-level ADR lands without a feasible gate, downgrade its label.

## Decision

Any ADR with `Status: Accepted` that declares a **code-level pattern** (a rule that can be spotted by scanning a source line — pattern presence/absence, function usage, naming convention) **must have a corresponding check** in `evaluate.py` or `tests/test_*.py` within one PR cycle of acceptance.

When a gate is infeasible, the ADR's header is amended with an explicit label:

```markdown
## Status

Accepted (guideline-level — see ADR-080 for why no CI gate)
```

and the ADR is moved from "binding" to "guideline" in `CLAUDE.md`'s ADR Guidelines section.

### Classification rule

| ADR type | Examples | Gate required? |
|---|---|---|
| Code-level pattern | ADR-004 (no String in hot paths), CLAUDE.md PROGMEM rule, No ArduinoJson, typed control flow | Yes — evaluate.py or tests |
| Structural/architectural | ADR-044 (single instantiation), ADR-051 (dual structs), ADR-077 (streaming discovery), ADR-078 (dispatch tables), ADR-079 (per-component headers) | No — reviewed in PR structurally |
| Historical/incident | ADR-040 (publishToSourceTopic fix) | No — fix is in the code; regression is caught by the code itself |
| Policy/system-level | "HTTP/WS only, no HTTPS/WSS"; WiFi reconnect tuning | No — reviewed at system-design level |

### Gate placement

- Preferred: add the check to `evaluate.py` as a method in `WorkspaceEvaluator` and register it in the always-run quick pipeline (so CI catches new violations before they land). Include corresponding cases in `tests/test_evaluate.py`.
- Acceptable: add the check as a dedicated pytest in `tests/` that walks the source tree; wire into `evaluate.py --quick` if feasible.

### Gate exit criteria

Every new pattern-level ADR includes, in its "Consequences" section, one of:

1. A link to the specific `evaluate.py` check or `tests/test_*.py` that enforces it, OR
2. A one-sentence note: *"Gate pending — tracked as TASK-&lt;id&gt;"*, with the follow-up task open in the backlog.

Merging a binding-labeled ADR without one of these is a review blocker.

### Initial audit

Applying this rule to the current ADR set (2026-04-19):

| ADR | Label | Gate |
|---|---|---|
| ADR-004 No String in hot paths | binding | `evaluate.py check_coding_standards` + `tests/test_evaluate.py TestScanStringUsagesDetailed` — adequate after TASK-H2 tightening |
| ADR-040 publishToSourceTopic scratch | historical | No gate needed (fix is in-place) |
| ADR-044 Single-point-of-instantiation | structural | Reviewed at PR, no gate |
| ADR-051 Dual settings/state structs | structural | Reviewed at PR, no gate |
| ADR-054 CSRF same-origin | pattern (partial) | **Gate gap** — collectHeaders mis-registration silently disabled CSRF (TASK-294 fixed it). Follow-up: add a test that verifies Origin/Referer are registered. |
| ADR-061 Unified platform abstraction | structural | Reviewed at PR |
| ADR-075 WiFi reconnect timeout | policy | No gate |
| ADR-076 SAT OPV calibration | algorithm | Validated by SAT tests (indirect) |
| ADR-077 Streaming MQTT HA discovery | structural | Reviewed at PR + `docs/api/MQTT_DISCOVERY_CHECK.md` procedure (manual, not CI) |
| ADR-078 MQTT dispatch tables | structural | Reviewed at PR |
| ADR-079 Per-component type headers | structural | Reviewed at PR + build-size diff |
| CLAUDE.md PROGMEM rule | binding | `evaluate.py check_progmem_compliance` — adequate |
| CLAUDE.md No ArduinoJson | binding | `evaluate.py check_no_arduinojson` — adequate |
| CLAUDE.md No Serial.print after init | binding | `evaluate.py check_coding_standards` — adequate |
| CLAUDE.md Typed control flow | binding | **Gate gap** — no check stops a new 76-branch strcasecmp chain. Follow-up task proposed. |
| CLAUDE.md Binary-safe compare (memcmp_P vs strncmp_P on binary) | binding | `evaluate.py check_binary_safe_compare` (INFO-level) — adequate |

Gate gaps identified above need follow-up tasks (tracked separately, one per gap).

## Consequences

### Positive

- Pattern-level drift becomes a compile-time / CI-time signal rather than a biannual-review signal.
- New ADR proposers have to think about enforcement up front, not as an afterthought. Makes the proposal tighter.
- Classification into pattern/structural/historical/policy gives reviewers a quick filter: "this ADR claims to be binding, but I don't see a gate — is the classification right?"
- The initial audit puts dates and follow-up tasks on every existing gap. No more undetected drift.

### Negative

- Two new gate gaps identified (ADR-054 CSRF collection, typed-control-flow) need work to close. Not a regression; just visible debt.
- Writing an ADR now carries a small extra design step: either write the check or downgrade the label. That friction is intentional and cheap.
- The classification rule has edge cases (e.g. "style guidance" in CLAUDE.md that is neither system nor pattern). The rule of thumb: if an automated check is both feasible *and* catches real regressions, it should exist.

### Neutral

- Existing binding ADRs that already have gates (ADR-004, PROGMEM, No ArduinoJson, No Serial.print after init) are unaffected.
- ADRs that were never meant to be pattern-level (ADR-044, ADR-051, ADR-077, etc.) get a cleaner classification label in `CLAUDE.md` but no behavior change.

## Gate (for ADR-080 itself)

This ADR is **meta-level** (a rule about rules). It is enforced by:

- A future `evaluate.py` check (follow-up task) that reads every `docs/adr/ADR-*.md`, finds the `## Status` line, and flags any ADR labeled `Accepted` or `binding` that does not reference an `evaluate.py` check, a test, or one of the accepted labels (`guideline-level`, `system-level`, `structural`, `historical`).
- Until that check lands, this ADR is enforced by PR review.

## Related

- **ADR-004** — First example of a binding pattern-level ADR; its gap was the trigger for ADR-080.
- **CLAUDE.md** — "ADR Guidelines" section cross-references this ADR for the binding/guideline distinction.
- **TASK-297** — Added `tests/test_evaluate.py`, the harness this ADR assumes for gate tests.
- **TASK-294** — CSRF gate gap surfaced by this ADR's audit.
- **TASK-307 / ADR-078** — Typed-control-flow gate gap surfaced by this ADR's audit.
- Retrospective discussion on 2026-04-19 that triggered this ADR: the 19-String-violation finding under TASK-296.
