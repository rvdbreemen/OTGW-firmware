---
id: TASK-470
title: >-
  feat(tooling): promote design-system drift detector to evaluate.py CI gate
  (Vorm B)
status: Done
assignee:
  - '@codex'
created_date: '2026-04-28 17:08'
updated_date: '2026-04-28 22:15'
labels:
  - tooling
  - design-system
  - ci-gate
  - evaluate.py
  - adr
dependencies:
  - TASK-469
references:
  - tools/check_design_system_drift.py
  - evaluate.py
  - docs/adr/
  - otgw_design_package/handoff.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Successor of `tools/check_design_system_drift.py` (Vorm A). Promote the standalone diagnostic script into a permanent `evaluate.py` check, formalised by an ADR per the ADR-080 meta-rule.

## Context

`tools/check_design_system_drift.py` was added to detect CSS class drift between `data/*.html`, `data/*.js` and `data/*.css` — a class is referenced (via `class="…"`, `classList.add('…')`, etc.) but never defined as a CSS selector. This caught the gap left by the design-package migration (TASK-435 Patch E) where legacy `index.css` was removed but JS still emits markup using legacy class names.

Vorm A (the standalone script) is intended as a manual diagnostic and one-shot porting aid for TASK-469. Vorm B (this task) makes it permanent and binding so drift cannot silently re-enter the codebase.

## Pre-conditions for starting this task

- TASK-469 (Port missing legacy widget classes into components.css) is **Done** — i.e., baseline is green, drift count = 0.
- Otherwise: starting now means the new check fires red on day one. People learn to ignore it ("rood is normaal"). Better to land on a clean baseline.

## Scope

### 1. Lift pure helpers into evaluate.py

Move (or duplicate, if shared logic risk is too high) these functions from `tools/check_design_system_drift.py` into the module-level pure-helpers section of `evaluate.py` (the area around `scan_string_usages_detailed`, `scan_binary_compare_calls`):

- `strip_css_comments`
- `strip_js_comments`
- `extract_classes_from_html`
- `extract_classes_from_js`
- `extract_class_definitions_from_css`
- `compute_drift`

Naming convention: prefix with the file's local style (e.g., `_DS_CLASS_ATTR_RE` for module-level regex, `extract_html_classes_with_locations`).

### 2. Add `WorkspaceEvaluator.check_design_system_drift()`

Wire it into the existing dispatch:
- Section header: `# ===== DESIGN SYSTEM CHECKS =====`
- Run by default in full mode; **decide whether to include in `--quick`** (recommendation: include — the check is fast and the cost of late discovery is high).
- Severity progression:
  1. First release: WARNING (in case of edge-case false positives)
  2. After one release with no false-positive reports: ERROR

Output format aligned with existing checks: per-class one line with file:line of first offence, summary count, total severity bucket.

### 3. Add unit tests

`tests/test_evaluate.py` (or new `tests/test_design_system_drift.py`):

- `test_extract_classes_from_html_simple` — single class, multiple classes, both quote styles
- `test_extract_classes_from_js_classlist_api` — `add`, `remove`, `toggle`, `replace` (two args)
- `test_extract_classes_from_js_template_literal` — class= inside backtick string
- `test_extract_classes_from_js_skips_template_placeholders` — `${cls}` isn't extracted as a literal class
- `test_extract_class_definitions_compound_selectors` — `.foo.bar` produces both, `.foo .bar` produces both, `.foo:hover` produces foo
- `test_extract_class_definitions_skips_comments` — `/* .foo */` isn't extracted
- `test_compute_drift_set_arithmetic` — used minus defined = missing; defined minus used = dead
- `test_drift_against_known_fixture` — small synthetic data/ fixture in `tests/fixtures/design_system/` with deliberately missing class

### 4. Write ADR-091 (or next free number) per ADR-080

ADR title: "Design-system class drift gated by evaluate.py check"

Status: Proposed → Accepted after the four verification gates (Completeness, Evidence, Clarity, Consistency).

Body sections per ADR template:
- **Context:** TASK-435 design-package migration removed legacy index.css. JS continues to emit legacy class names. Without an automated check, regressions are silent (visual-only).
- **Decision:** Gate design-system class drift in evaluate.py via `check_design_system_drift`. Severity ERROR after a one-release WARNING grace period.
- **Consequences:** Any new HTML/JS that emits a class must either define a CSS rule for it or be added to a documented exception list (e.g., browser-state-only classes). The check fires in CI and locally before commit.
- **Related:** ADR-080 (CI-gate meta-rule), TASK-435 (origin), TASK-469 (port-missing-classes baseline-green precondition), TASK-471 (this).

### 5. Documentation

- Update `CLAUDE.md` ADR-Guidelines section to list ADR-091.
- Update `evaluate.py` module docstring to mention design-system check.
- Add a short note in `otgw_design_package/handoff.md` referencing the gate so future package authors know it exists.

## Acceptance Criteria
<!-- AC:BEGIN -->
- AC1: `evaluate.py` contains `check_design_system_drift()` with module-level pure helpers; no logic duplication with `tools/check_design_system_drift.py` (either tool re-uses the helpers, or the standalone is deprecated/removed).
- AC2: Tests in `tests/` cover at minimum: HTML class extraction, JS classList API, JS template-literal extraction, compound CSS selector handling, comment stripping, drift set arithmetic.
- AC3: `python evaluate.py` and `python evaluate.py --quick` both run the check successfully on a green baseline (drift count = 0) without errors.
- AC4: ADR-091 is in `docs/adr/` with Status `Accepted`, references the evaluate.py gate per ADR-080, and lists TASK-469 as the baseline-green prerequisite.
- AC5: One release cycle of WARNING severity, then promote to ERROR. Track via follow-up task or in this task's history.

- [x] #1 evaluate.py contains check_design_system_drift() with module-level pure helpers; no duplicated logic with tools/check_design_system_drift.py
- [x] #2 tests/ covers HTML class extraction, JS classList API, JS template-literal extraction, compound CSS selector handling, comment stripping, and drift set arithmetic
- [x] #3 python evaluate.py and python evaluate.py --quick both run the check successfully on a green baseline (drift count = 0) without errors
- [x] #4 ADR-091 (or next free number) accepted in docs/adr/, references the evaluate.py gate per ADR-080, lists TASK-469 as the baseline-green prerequisite
- [x] #5 Severity progression handled: WARNING for one release cycle, then promoted to ERROR (track via follow-up task or task history)
<!-- AC:END -->

## Definition of Done (task-specific)

- All ACs checked
- `evaluate.py --quick` adds ≤ 200ms wall-clock for the new check (regex over ~10 small files = essentially free; verify)
- A deliberate regression test: introduce a class in `index.html` that is not defined in CSS → check fails with the expected message → revert → check passes
- `tools/check_design_system_drift.py` decision: keep as-is (with note about CI version), or remove and document the move. Recommendation: keep both — the standalone is useful for one-off auditing without spinning up the full evaluator.

## Out of scope (deliberate)

- AST-based parsing (parsing JS to follow dynamic class assembly): regex catches 90% for 5% of the complexity. Out unless we hit a specific false-negative case.
- Stylelint or other npm-toolchain integrations: this firmware ships without a Node toolchain by design.
- Cross-file dead-import detection in JS — different problem class.
- Alerting/Slack/email on CI failures — that is a CI infrastructure concern, not a check concern.

## References

- `tools/check_design_system_drift.py` — Vorm A (the standalone diagnostic that this task formalises)
- `evaluate.py:40-117` — module-level pure-helper pattern to follow
- `docs/adr/ADR-080-...md` — meta-rule that mandates this kind of CI gate for binding pattern-level ADRs
- `otgw_design_package/handoff.md` — Patch E removed legacy CSS without documenting required class porting; this task closes that loop
- TASK-435 / TASK-458 / TASK-461 / TASK-467 / TASK-469 — design-package implementation chain
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Move design-system drift parsing helpers into evaluate.py and make the standalone tool import them. 2. Add check_design_system_drift to quick and full evaluation as a WARN gate with an allowlist for intentional non-style hooks. 3. Add focused tests for extraction, comments, selectors, drift arithmetic, allowlist behavior, and a synthetic regression. 4. Add ADR-091 and update CLAUDE.md plus design-package handoff docs. 5. Run tests, quick/full evaluation, and the standalone JSON diagnostic before closing ACs.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-28 Codex resumed TASK-470 on feature-dev-2.0.0-otgw32-esp32-sat-support; reviewing existing partial implementation before finishing.

Tightened classList literal extraction to capture all quoted arguments in multi-arg calls; covered by tests because sat.js already uses a three-arg classList.remove.

Verification: tests/test_evaluate.py passed (45 tests); tests/test_build.py passed (9 tests); build.py completed ESP8266 and ESP32-S3 firmware/filesystem/distribution builds successfully.

Design drift verification: tools/check_design_system_drift.py --json reports drift_count=0, allowlisted_count=32, used_count=238; direct shared-helper timing was 44.076 ms.

Deliberate regression check: temporarily added codex-temp-drift-check to index.html, standalone diagnostic reported drift_count=1 at index.html:84, then the temporary edit was reverted and the diagnostic returned to drift_count=0.

evaluate.py --quick --no-color --verbose and evaluate.py --no-color both execute Design-System Class Drift as PASS. Overall exit remains 1 because of pre-existing PROGMEM violations in debugStuff.ino, FSexplorer.ino, jsonStuff.ino, and MQTTstuff.ino; those files are outside TASK-470.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Promoted the design-system class drift detector into evaluate.py as check_design_system_drift with shared module-level helper functions and a documented allowlist for intentional no-style classes. The standalone tools/check_design_system_drift.py now imports the same scanner and allowlist so manual diagnostics and the CI gate do not diverge. Added focused unittest coverage for HTML extraction, JS classList and template literals, comment stripping, CSS selector extraction, drift arithmetic, allowlist behavior, fixture drift, and multi-argument classList calls. Added ADR-091 as Accepted, documented the gate in CLAUDE.md and otgw_design_package/handoff.md, and created TASK-480 to promote WARN drift severity to FAIL after the grace release. Verification: tests/test_evaluate.py and tests/test_build.py pass; build.py succeeds for ESP8266 and ESP32-S3; standalone drift JSON reports drift_count=0; a temporary index.html regression produced the expected missing-class report and was reverted. evaluate.py quick/full both run the design drift check as PASS, while the overall evaluator still exits 1 because of pre-existing PROGMEM findings outside TASK-470.
<!-- SECTION:FINAL_SUMMARY:END -->
