# ADR-001 Clean Baseline

## Status

Accepted, 2026-04-25.

## Context

Synthetic fixture: a minimal ADR that has every canonical section.

## Decision

This file is used as the PASS-strictly fixture in adr-lint test cases.

## Alternatives Considered

- Skip this fixture: rejected; we need a positive control.
- Reuse an existing example: rejected; tests should not depend on documentation files.

## Consequences

The test suite has a stable PASS baseline.

## Related Decisions

- None.

## References

- `tests/test_adr_lint.py`: the test that uses this fixture.
