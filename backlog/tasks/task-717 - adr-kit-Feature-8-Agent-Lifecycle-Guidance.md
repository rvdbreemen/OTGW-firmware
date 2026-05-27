---
id: TASK-717
title: 'adr-kit Feature 8: Agent Lifecycle Guidance'
status: Done
assignee: []
created_date: '2026-05-26 13:44'
labels:
  - adr-kit
  - phase-4
  - week-8
  - guidance
  - agent
milestone: v0.15.1
dependencies: []
references:
  - agents/adr-generator.md
  - bin/adr-context
  - bin/adr-status
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Enhance agents/adr-generator.md with decision tree and quality scoring. Guide agents on when ADRs are needed (library choice, code pattern, governance → yes; otherwise → no). Provide quality feedback with 0.0-1.0 score from 4 gates (completeness, evidence, clarity, consistency).

PLAN - WEEK 8 (v0.15.1 Phase 4):

1. Decision Tree Logic:
   Question 1: Library/technology choice? → YES → ADR needed
   Question 2: Code pattern/architecture? → YES → ADR needed
   Question 3: Governance/process decision? → YES → optional ADR
   Otherwise → No ADR needed
   
2. Quality Scoring (4 Gates):
   a) Completeness Gate (0.0-1.0):
      - All required sections present
      - Description >100 chars
      - At least 2 alternatives
      - Consequences included
   
   b) Evidence Gate (0.0-1.0):
      - At least 1 citation/reference
      - Links to incidents, RFCs, docs
      - Quantified measurements if relevant
   
   c) Clarity Gate (0.0-1.0):
      - Readability score (Flesch-Kincaid equivalent)
      - Single clear decision (no hedging)
      - Traceable identifiers (file paths, functions)
   
   d) Consistency Gate (0.0-1.0):
      - Bidirectional relationships validated
      - Related ADRs mentioned
      - No conflicting constraints with other ADRs

3. Quality Feedback:
   - Score breakdown per gate (4 scores)
   - List of specific quality issues
   - Actionable recommendations
   - Severity: high/medium/low

4. Integration:
   - adr-context: load relevant ADRs for context
   - adr-status: show post-decision feedback
   - agents/adr-generator.md: main integration point

5. Testing (10+ test cases):
   - Decision tree logic for each path
   - Quality scoring for each gate
   - Score range validation (0.0-1.0)
   - Issue detection accuracy
   - Integration with adr-context
   - Integration with adr-status
   - And more

DELIVERABLES:
→ agents/adr-generator.md (decision tree + quality scoring)
→ tests/test_adr_quality.py (10+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision tree added to agents/adr-generator.md
- [ ] #2 Quality scoring algorithm with 4 gates implemented
- [ ] #3 Completeness gate checks sections and content
- [ ] #4 Evidence gate checks citations and references
- [ ] #5 Clarity gate uses readability heuristics
- [ ] #6 Consistency gate validates relationships
- [ ] #7 Score returned as 0.0-1.0 with breakdown
- [ ] #8 Quality issues listed with recommendations
- [ ] #9 adr-context integrated for context loading
- [ ] #10 adr-status integrated for feedback
- [ ] #11 10+ test cases passing
- [ ] #12 All existing tests pass
<!-- AC:END -->
