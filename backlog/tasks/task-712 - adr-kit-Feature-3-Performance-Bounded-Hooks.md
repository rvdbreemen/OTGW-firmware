---
id: TASK-712
title: 'adr-kit Feature 3: Performance-Bounded Hooks'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-26 13:38'
updated_date: '2026-05-27 10:01'
labels:
  - adr-kit
  - phase-2
  - week-3
  - observability
  - devops
milestone: v0.14.1
dependencies: []
references:
  - bin/adr-judge
  - templates/githooks/pre-commit
  - .adr-kit.json
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add timing instrumentation to git hooks and pre-commit workflows. Measure execution time, warn when thresholds exceeded, print detailed profile breakdowns per rule.

PLAN - WEEK 3 (v0.14.1 Phase 2):

1. Configuration (DONE - in .adr-kit.json):
   - pre_commit_timeout_ms: 5000 (5 seconds)
   - pre_push_timeout_ms: 15000 (15 seconds)
   - llm_timeout_ms: 30000 (30 seconds)
   - warn_on_exceed: true (non-blocking warnings)

2. Implement --profile flag in bin/adr-judge:
   - Measure execution time for each rule type
   - Output per-rule timing breakdown
   - Show budget utilization percentage
   - Display total time vs timeout threshold
   - Format: table with columns [Rule, Time(ms), Count, Avg(ms), Budget%]

3. Implement --dry-run-enforcement ADR-XXX mode:
   - Test single ADR without modifying state
   - Useful for debugging specific ADRs
   - Same enforcement logic as normal run
   - Output timing + results for that ADR only
   - Non-destructive (no side effects)

4. Update templates/githooks/pre-commit:
   - Measure total adr-judge execution time
   - Warn if execution time > 5 seconds (non-blocking)
   - Continue with commit (never block on timeout)
   - Print profile output with timing breakdown
   - Log timing data for trends

5. Performance Infrastructure:
   - Add timing wrappers around key functions
   - Track rule application time per type
   - No modification of ADR content (timing only)
   - Measure parsing, validation, enforcement separately

6. Testing (10+ test cases):
   - --profile flag outputs timing data
   - Budget info shown in profile output
   - --dry-run mode works without side effects
   - Timeout config loaded from .adr-kit.json
   - Hook warns when timing exceeds threshold
   - Hook doesn't block on timeout
   - Performance <5s on pre-commit, <15s on pre-push
   - Timing data doesn't modify ADRs
   - Per-rule breakdown accuracy
   - And more edge cases

DELIVERABLES:
✓ .adr-kit.json (performance config)
→ bin/adr-judge (--profile flag, --dry-run mode)
→ templates/githooks/pre-commit (timing checks)
→ tests/test_adr_performance.py (10+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 --profile flag added to adr-judge with per-rule timing output
- [ ] #2 --dry-run-enforcement ADR-XXX mode works without state changes
- [ ] #3 pre-commit hook measures execution time and warns if >5s
- [ ] #4 pre-push hook timeout set to 15s with graceful fallback
- [ ] #5 .adr-kit.json has configurable timeouts
- [ ] #6 Timing data doesn't modify ADR files
- [ ] #7 10+ test cases passing
- [ ] #8 Performance targets met: pre-commit <5s, pre-push <15s
<!-- AC:END -->
