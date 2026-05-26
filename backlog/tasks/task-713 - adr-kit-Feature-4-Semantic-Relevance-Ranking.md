---
id: TASK-713
title: 'adr-kit Feature 4: Semantic Relevance Ranking'
status: To Do
assignee: []
created_date: '2026-05-26 13:39'
labels:
  - adr-kit
  - phase-2
  - week-4
  - ranking
  - backend
milestone: v0.14.1
dependencies: []
references:
  - bin/adr-context
  - .adr-kit.json
  - agents/adr-generator.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement bin/adr-context tool for intelligent ADR lookup. Uses heuristic scoring with 5 weighted signals: keywords, domain tags, related decisions, acceptance status, recency. Returns top N ranked ADRs for task context.

PLAN - WEEK 4 (v0.14.1 Phase 2):

1. Core Tool (DONE - skeleton):
   - bin/adr-context created (~250 lines)
   - 5-signal heuristic scoring implemented
   - Query parsing and domain inference
   - Config loading from .adr-kit.json

2. Five Scoring Signals (weights configurable in .adr-kit.json):
   - exact_keyword: 0.40 (keywords in title/decision match query)
   - domain_tag: 0.25 (domain inference: backend/frontend/infra/security/data)
   - related_decisions: 0.15 (ADR-XXX mentions in related decisions)
   - acceptance_status: 0.10 (prefer Accepted over Proposed)
   - recency: 0.10 (newer ADRs ranked higher)

3. extract_keywords() Function:
   - Parse task query into keyword list
   - Filter by minimum length (3+ chars)
   - Rank keywords by relevance
   - Return sorted keyword list

4. infer_task_domain() Function:
   - Detect domain from query keywords
   - Domains: backend, frontend, infra, security, data
   - Default: backend if ambiguous
   - Return inferred domain

5. score_adr() Function:
   - Combine 5 weighted signals
   - Return score 0.0-1.0
   - Show signal breakdown in detailed output
   - Handle missing/invalid data gracefully

6. load_adr_context() Function:
   - Return top N ADRs (default N=5, configurable)
   - Filter by minimum relevance threshold (0.1 default)
   - Sort by score descending
   - Include ADR ID, title, relevance score

7. Output Formats:
   - JSON: detailed with all scores
   - Text: simple ranked list
   - HTML (optional): formatted for web display

8. Integration Points:
   - agents/adr-generator.md: inject top 5 ADRs for context
   - skills/judge/SKILL.md: "Load relevant ADRs? (Y/n)"
   - agents/adr-generator.md: context loading flow

9. Testing (15+ test cases):
   - Find matching ADRs by keywords
   - Keyword scoring accuracy
   - Domain inference for each domain
   - Status preference (Accepted > Proposed)
   - Recency weighting
   - --limit flag restricts results
   - JSON output format validation
   - Weights loaded from config
   - Performance <100ms on 30-ADRs
   - Default limit is 5
   - Domain filtering
   - Empty query handling
   - And 3 more cases

DELIVERABLES:
✓ bin/adr-context (~250 lines)
→ agents/adr-generator.md (context injection)
→ skills/judge/SKILL.md (context prompt)
→ tests/test_adr_context.py (15+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bin/adr-context tool created with all 5 scoring signals
- [ ] #2 Weights configurable in .adr-kit.json with defaults
- [ ] #3 extract_keywords() parses query correctly
- [ ] #4 infer_task_domain() returns correct domain
- [ ] #5 score_adr() returns 0.0-1.0 score
- [ ] #6 load_adr_context() returns top N ADRs
- [ ] #7 Performance <100ms on 30-ADR suite
- [ ] #8 agents/adr-generator.md updated with context injection
- [ ] #9 skills/judge/SKILL.md updated with context prompt
- [ ] #10 15+ test cases passing
- [ ] #11 All existing tests pass
<!-- AC:END -->
