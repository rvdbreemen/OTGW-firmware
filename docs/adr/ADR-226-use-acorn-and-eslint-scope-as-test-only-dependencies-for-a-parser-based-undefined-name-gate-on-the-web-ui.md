---
id: "ADR-226"
title: "Use acorn and eslint-scope as test-only dependencies for a parser-based undefined-name gate on the web UI"
status: "Accepted"
date: "2026-11-10"
binding: false
gate: "tests/webui/v2-undefined-names.test.mjs"
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "web UI tests"
  - "undefined names"
  - "ReferenceError"
  - "test dependencies"
  - "npm"
aliases:
  - "parser gate"
  - "undefined-name gate"
  - "acorn"
  - "eslint-scope"
components:
  - "tests/webui/package.json"
  - "tests/webui/v2-undefined-names.test.mjs"
symbols:
  - "ALLOW"
  - "unresolvedNames"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-226 Use acorn and eslint-scope as test-only dependencies for a parser-based undefined-name gate on the web UI

## Status

Accepted, 2026-11-10.
coordinating session); acceptance of this record waits for his explicit yes on the record itself.

## Status History

```yaml
status_history:
  - date: 2026-11-10
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-10
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted decision after all four verification gates passed
    changed_via: adr-kit lifecycle
```

## Context

The v2 web UI (user interface) is one large script, `src/OTGW-firmware/data/v2.js` (about 8,000 lines, no build
step, no bundler, no type checker). Release 2.0.0-beta.166 shipped calls to the helper `esc()` from
functions outside the scope that defines it. The browser threw a `ReferenceError` (a runtime error
for a name that resolves to no declaration), the SAT (Smart Autotune Thermostat) page stopped rendering and a lock on it was not
applied. All web UI tests at the time were regex or DOM (Document Object Model) tests; none of them could see scope.
TASK-1768 added `tests/webui/v2-nested-helper-scope.test.mjs`, which catches the specific pattern
of beta.166, but not a misplaced name in general.

Constraints: the firmware build and the LittleFS image must not change; the project rule is to reuse
an open-source solution before building one, with as few dependencies as possible; until now the
web UI tests ran on plain `node --test` with no npm packages at all, and no CI (continuous integration) job runs them (they
run locally through `python scripts/run_tests.py --webui`).

## Decision

Add acorn (a JavaScript parser) and eslint-scope (scope analysis on its syntax tree) as pinned,
test-only npm dependencies in `tests/webui/package.json`, and gate `v2.js` with
`tests/webui/v2-undefined-names.test.mjs`: every name that eslint-scope cannot resolve to a
declaration must be on an explicit allowlist of browser and ECMAScript globals, or the test fails.

The test parses `v2.js` as a classic script (`ecmaVersion: 'latest'`), runs
`eslint-scope.analyze()`, and reads the global scope's `through` list: the references that leave
every scope unresolved. On the current `v2.js` that list has 42 names, all browser or language
globals. On the beta.166 `v2.js` (commit c701f6b95) it also contains `esc` (line 6973), so the gate
fails on exactly the shipped bug. A second check fails when an allowlist entry is no longer used, so
the allowlist cannot silently grow.

## Decision Contract

### Must

- Pin exact versions (`acorn` 8.19.0, `eslint-scope` 9.1.2) in `tests/webui/package.json` and
  commit `tests/webui/package-lock.json` (excepted from the root `package-lock.json` ignore rule).
- Keep `tests/webui/node_modules/` out of git (`.gitignore`).
- Fail with an install instruction (`npm ci --prefix tests/webui`) when the packages are missing,
  never skip silently.
- Add a name to the allowlist only when the browser or the language really provides it.

### Must Not

- Load acorn, eslint-scope or any npm package from firmware sources, from `src/OTGW-firmware/data/`
  or from the LittleFS image.
- Use a version range (`^`, `~`) for these packages.

### Exceptions

- None.

### Verification

- `node --test tests/webui/v2-undefined-names.test.mjs`: 2/2 green on the current `v2.js`;
  with `OTGW_V2_JS` pointing at the beta.166 `v2.js` it fails with `esc (line 6973)`.
- `npm audit --prefix tests/webui`: 0 vulnerabilities on 2026-11-10.

## Alternatives Considered

- **ESLint with the `no-undef` rule.** Uses the same parser and scope analysis underneath, but brings
  about a hundred transitive packages and a configuration file for one rule. Rejected: much heavier
  for the same check.
- **TypeScript `tsc --checkJs`.** Finds undefined names, but is tens of megabytes and reports many
  false positives on untyped DOM code, which would need annotations in `v2.js`. Rejected: changes the
  shipped file and costs far more than it catches here.
- **A hand-written walker on acorn alone (or regex).** Would re-implement scope analysis: hoisting,
  closures, function parameters, catch clauses, block scope. Exactly what eslint-scope does and easy
  to get subtly wrong. Rejected under the reuse-open-source rule.
- **Do nothing (keep only the TASK-1768 pattern test).** Catches the beta.166 shape, not a misplaced
  name in general. Rejected: the beta.166 regression cost a public release.

## Consequences

**Positive:**

- Any undeclared name in `v2.js` fails a local test before a build, instead of a page failing in a
  tester's browser.
- Small footprint: 6 packages, 944 KB (kilobytes) installed, all under the MIT licence or the BSD-2-Clause (Berkeley Software Distribution) licence; nothing reaches a device.

**Negative:**

- The first npm dependency in the repository: a fresh checkout needs `npm ci --prefix tests/webui`
  once before `run_tests.py --webui`. Mitigation: the test says so when the packages are missing.
- New browser APIs used by `v2.js` need an allowlist entry. Mitigation: the failure names the line,
  and the stale-entry check keeps the list honest.
- Supply-chain risk of third-party packages. Mitigation: exact pins and a lockfile, test-only use,
  `npm audit` when the pins move.

## Open Questions

- [x] Robert's explicit yes on this record (the dependency itself was approved on 2026-11-10). — **Answered 2026-11-10 by User: Robert van den Breemen:** Yes: Robert accepted the record in session otgw-firmware-80 (AskUserQuestion, 2026-11-10).

## Related Decisions

- **ADR-192 (Ship only the v2 web UI on the 2.0.0 line)**: the gate protects that single UI file.
- **ADR-146 (No ArduinoJson)**: unrelated in scope; this ADR adds no firmware dependency.

## References

- TASK-1769 (this gate), TASK-1768 (`tests/webui/v2-nested-helper-scope.test.mjs`), TASK-1764
  (the beta.166 regression).
- `tests/webui/v2-undefined-names.test.mjs`, `tests/webui/package.json`,
  `tests/webui/package-lock.json`.
- acorn: <https://github.com/acornjs/acorn> (MIT); eslint-scope:
  <https://github.com/eslint/js/tree/main/packages/eslint-scope> (BSD-2-Clause); versions and
  licences checked with `npm view` on 2026-11-10.
