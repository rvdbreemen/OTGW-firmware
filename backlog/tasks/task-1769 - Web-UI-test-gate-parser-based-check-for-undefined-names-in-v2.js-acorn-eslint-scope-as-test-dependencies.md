---
id: TASK-1769
title: >-
  Web UI test gate: parser-based check for undefined names in v2.js (acorn +
  eslint-scope as test dependencies)
status: To Do
assignee: []
created_date: '2026-11-09T23:11:00Z'
updated_date: '2026-11-09T23:13:00Z'
labels:
  - webui
  - test
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 866000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-10 after beta.166: TASK-1764 shipped esc() calls outside the functions that define esc, a ReferenceError on the SAT page. tests/webui/v2-nested-helper-scope.test.mjs (TASK-1768) catches nested-helper scope errors and a shadowed t(), but a fully generic 'every referenced name is declared' check needs a JS parser: a regex version gives false positives (getters, method shorthand, regex literals). A hand run of acorn 8.12.1 + eslint-scope 8.2.0 (globalScope.through) found exactly the regression. Adding them is a new (test-only) dependency: needs Robert's OK and an ADR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded (ADR or rejection) on adding acorn + eslint-scope as test-only dependencies
- [ ] #2 If accepted: a webui test lists every unresolved name in v2.js against an explicit browser-global allowlist; red on beta.166 (esc), green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Open-source weighing (project rule 'reuse open source first'): candidates acorn (MIT, ~130 KB, zero dependencies, ESTree parser used by webpack/rollup/node itself, release 8.12.1 2024) and eslint-scope (BSD-2, scope analysis from the ESLint project, deps esrecurse + estraverse, 8.2.0); alternative: eslint itself with no-undef (much heavier, ~100 transitive packages); building a scope analyser ourselves rejected (a regex version was tried 2026-11-10 and gave false positives on getters, method shorthand and regex literals). Recommendation: acorn + eslint-scope as test-only devDependencies (never shipped to the device, no runtime/flash impact), pinned versions, run in tests/webui only. Needs Robert's OK + ADR because it is a new dependency.
<!-- SECTION:NOTES:END -->
