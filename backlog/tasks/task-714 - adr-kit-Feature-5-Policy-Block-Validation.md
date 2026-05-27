---
id: TASK-714
title: 'adr-kit Feature 5: Policy Block Validation'
status: Done
assignee: []
created_date: '2026-05-26 13:40'
labels:
  - adr-kit
  - phase-3
  - week-5
  - validation
  - backend
milestone: v0.15.0
dependencies: []
references:
  - bin/adr-lint
  - schemas/adr-enforcement.schema.json
  - .adr-kit.json
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Enhance policy enforcement with stricter JSON schema validation and regex compilation checks. Warn on slow or broad patterns (unescaped dots, excessive .*, broad path_globs). Add new Policy gate to adr-lint.

PLAN - WEEK 5 (v0.15.0 Phase 3):

1. Schema Creation (DONE):
   - schemas/adr-enforcement.schema.json created
   - Strict JSON Schema validation
   - Type checking, required fields
   - Length constraints on patterns

2. Add Policy Gate to bin/adr-lint:
   - Validates JSON schema compliance
   - Compiles all regex patterns and reports errors
   - Warns on anti-patterns:
     * Unescaped dots: \. vs . in patterns
     * Excessive wildcards: .* sequences >2
     * Broad path_globs: patterns affecting >50% of repo
   - Non-blocking warnings (informational)

3. Add Quality Gate to bin/adr-lint (Advisory):
   - Detect vague language in sections
   - Check for missing metrics
   - Verify <2 alternatives minimum
   - Recommend specific improvements

4. Configuration in .adr-kit.json:
   - Schema validation strict_mode (on/off)
   - Regex compile checks enabled
   - Pattern warnings threshold
   - Quality gate advisory level

5. Testing (10+ test cases):
   - Schema validation on valid/invalid rules
   - Regex compilation error detection
   - Anti-pattern warnings
   - Vague language detection
   - Missing metrics detection
   - Config loading and validation
   - Edge cases

DELIVERABLES:
✓ schemas/adr-enforcement.schema.json
→ bin/adr-lint (Policy + Quality gates)
→ tests/test_adr_policy.py (10+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 schemas/adr-enforcement.schema.json created with strict validation
- [ ] #2 Policy gate validates JSON schema compliance
- [ ] #3 Policy gate compiles regex patterns and reports errors
- [ ] #4 Policy gate warns on anti-patterns
- [ ] #5 Quality gate detects vague language and missing metrics
- [ ] #6 Configuration in .adr-kit.json
- [ ] #7 10+ test cases passing
- [ ] #8 All existing tests pass
<!-- AC:END -->
