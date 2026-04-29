# ADR-091 Design-System Class Drift Gated by evaluate.py

## Status

Accepted (2026-04-28)

## Context

TASK-435 Patch E removed the legacy `index.css`, `index_dark.css`, and
FSexplorer stylesheets after wiring the token-driven `ds-tokens.css` and
`components.css` design system. HTML and JavaScript continued to emit class
names, and missing selectors became visual-only regressions: the browser shows
unstyled content, but firmware builds and API tests stay green.

`tools/check_design_system_drift.py` was created as the manual diagnostic. It
found the cleanup baseline and fed the CSS repair sequence. TASK-470 promotes
that detector into the permanent evaluator so new drift is visible in the same
local/CI path as the existing ADR-080 gates.

The TASK-470 text names TASK-469 as a baseline prerequisite. In this backlog
snapshot TASK-469 is a flashing-tooling task, so the CSS baseline evidence is
actually TASK-473 for legacy/pre-Patch-E CSS and TASK-475 for SAT post-Patch-E
CSS. TASK-469 remains referenced because it is present in TASK-470 metadata and
history.

## Decision

Gate design-system class drift in `evaluate.py` via
`WorkspaceEvaluator.check_design_system_drift()`.

The check scans `src/OTGW-firmware/data/*.html` and `*.js` for literal class
references, scans `*.css` for class selectors, and reports any referenced class
that has no CSS selector unless it appears in the documented allowlist in
`evaluate.py`.

The allowlist is grouped by reason:

- behavior/state hooks used only by JavaScript routing or hardware capability
  toggles;
- `design.html` reference-only demo helpers, whose styles are local to that
  reference page;
- browser/native utility classes where visual styling intentionally comes from
  another class or the native control.

For the first release cycle this gate reports actionable drift as `WARN`, not
`FAIL`, to absorb false positives from the regex-based scanner. TASK-480 tracks
the promotion to `FAIL` after one release with no accepted false-positive
reports.

## Consequences

New Web UI HTML or JavaScript that emits a class must either define a matching
CSS selector in `data/*.css` or add a reviewed allowlist entry with a concrete
reason. Unexplained drift becomes visible in both `python evaluate.py --quick`
and full `python evaluate.py`.

The standalone `tools/check_design_system_drift.py` remains available for manual
audits, but it imports the shared helpers from `evaluate.py` so the manual report
and CI gate use one scanner and one allowlist.

This is a pattern-level binding ADR under ADR-080. Enforcement is:

- `evaluate.py` `check_design_system_drift`;
- `tests/test_evaluate.py` `TestDesignSystemDriftHelpers`;
- `tools/check_design_system_drift.py` for the manual diagnostic view.

## Alternatives Considered

### Keep the detector standalone

Rejected. A manual-only tool was useful for the TASK-473/TASK-475 cleanup, but
it does not prevent silent drift in later Web UI work.

### Fail immediately on first release

Rejected. The scanner is intentionally regex-based rather than AST-based; a
one-release warning period gives maintainers a chance to classify edge cases
without normalising a permanently red gate.

### Parse JavaScript with a Node toolchain

Rejected for now. The firmware Web UI does not otherwise require Node, and the
literal-class scanner catches the actionable drift class with much less tooling
surface.

## Related

- ADR-080 Binding ADR rules must have a CI gate.
- TASK-470 Promote design-system drift detector to evaluate.py CI gate.
- TASK-469 Referenced in TASK-470 metadata as the baseline predecessor.
- TASK-473 Ported mechanically available legacy/pre-Patch-E CSS selectors.
- TASK-475 Added token-driven CSS for SAT post-Patch-E classes.
- TASK-480 Promote the WARN gate to FAIL after the grace release.
