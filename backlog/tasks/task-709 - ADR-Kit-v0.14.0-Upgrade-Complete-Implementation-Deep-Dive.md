---
id: TASK-709
title: 'ADR-Kit v0.14.0 Upgrade: Complete Implementation Deep Dive'
status: In Progress
assignee: []
created_date: '2026-05-26 11:03'
updated_date: '2026-05-26 11:06'
labels: []
milestone: v0.14.0 Upgrade
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Comprehensive implementation guide for HIGH and MEDIUM priority features from claude-mini analysis. Includes code sketches, architecture decisions, test strategies, and rollout plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR template updated with status_history, superseded_by, supersedes, domain_tags fields
- [ ] #2 adr-judge parses and appends to status_history (non-destructive)
- [ ] #3 adr-lint validates status_history is append-only + supersession chains bidirectional
- [ ] #4 bin/adr-retire detects 4 retirement signals with >90% accuracy
- [ ] #5 adr-judge --profile shows per-rule timing
- [ ] #6 Pre-commit hook warns if >5s (non-blocking)
- [ ] #7 25+ new test cases covering all features
- [ ] #8 v0.13 ADRs auto-migrate to v0.14 format on first run
- [ ] #9 All features documented with examples
- [ ] #10 v0.14.0 released and installable via plugin marketplace
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
# ADR-Kit v0.14.0: Deep Implementation Dive — Phase 1 & 2

## Overview
This plan covers the HIGH priority features (1–6) for adr-kit v0.14.0–v0.14.1.
Complete code sketches, test strategies, and integration examples included below.

---

## Feature 1: Append-Only Status History + Supersession Tracking

### Architecture
- Status changes logged in frontmatter, never rewrite content
- Audit trail: every status change has timestamp, actor, reason
- Bidirectional supersession: if ADR-A.superseded_by=ADR-B, then ADR-B.supersedes includes ADR-A

### Frontmatter Schema (v0.14)
```yaml
---
id: ADR-NNN
status: accepted

# Append-only history (new entries only at end)
status_history:
  - { date: "2025-01-01", status: "proposed", changed_by: "claude", reason: "initial", changed_via: "adr-generator" }
  - { date: "2025-01-05", status: "accepted", changed_by: "human", reason: "approved PR #123", changed_via: "adr-judge" }

# Bidirectional supersession tracking
superseded_by: null  # null if active, else "ADR-XXX"
supersedes: []       # list of ADR-XXX this one replaces

# Implicit decision tracking (auto-updated by judge)
last_referenced_sha: null
last_referenced_date: null
last_referenced_in: null

# Domain tagging (optional, for semantic search)
domain_tags: [backend, data-fetching]
importance: high  # high|medium|low
---
```

### Key Implementation Points
1. **adr-judge changes:** Append to status_history instead of rewriting Status line
2. **adr-lint changes:** Validate history is append-only, supersession chains bidirectional
3. **Migration:** v0.13 ADRs auto-convert to v0.14 format on first judge run
4. **Tests:** 20+ test cases covering history immutability, chain validation, migration

---

## Feature 2: Automated Retirement Detection

### Four Retirement Signals
1. **90-day staleness:** `last_referenced_date < now - 90` AND no incoming refs in git log
2. **Tech removal:** ADR mentions MySQL, MySQL not in package.json/pyproject.toml/go.mod anymore
3. **Supersession chain broken:** ADR-A.superseded_by=ADR-B but ADR-B status is still Proposed
4. **Policy rule mismatch:** Enforcement forbids pattern P, but P never appears in codebase (0 CI violations)

### Implementation: `bin/adr-retire`
- New CLI tool (~400 lines Python)
- Outputs JSON + Markdown with candidates + confidence scores
- Recommends: mark_deprecated, review_enforcement, review_supersession, or keep
- Used by GitHub Actions weekly cron (Monday 6am UTC)

### Integration: `/adr-kit:retire` skill
- Interactive approval workflow
- User reviews candidates
- On approve: calls adr-judge to append status change to history

### Test Strategy
- 15+ retirement scenarios: stale, tech-removed, chain-broken, no-signals
- Accuracy >90% on test set
- Performance: <2s on 30-ADR repo

---

## Feature 3: Performance-Bounded Enforcement Hooks

### Design
- `adr-judge --profile` shows per-rule timing
- Pre-commit hook warns if >5s (non-blocking)
- `.adr-kit.json` config allows timeout tuning
- `adr-lint` warns on slow patterns

### Implementation Details
1. **adr-judge --profile output:**
   ```
   Enforcement rule timing:
     ✓ forbid_import (ADR-0001): 0.12ms
     ✓ forbid_pattern (ADR-0002, ADR-0005): 0.34ms
     ⚠️ llm_judge (ADR-0012): 1.2s (timeout: 30s)
   Total: 1.66s / 5000ms ✓
   ```

2. **Pre-commit hook:** Measures adr-judge execution time, warns if >5s
3. **Config in .adr-kit.json:**
   ```json
   {
     "judge": {
       "pre_commit_timeout_ms": 5000,
       "pre_push_timeout_ms": 15000,
       "llm_timeout_ms": 30000
     }
   }
   ```

### Test Cases
- Fast rules (<100ms)
- LLM rules (~1s, separate timing)
- Total <5s on 10-ADR repo
- Profile output readable

---

## Phase 1 Timeline (Weeks 1–2)

### Week 1: Status History Foundation
- [ ] Update adr-template.md with new frontmatter fields
- [ ] adr-judge: status history parsing + appending logic
- [ ] adr-lint: Audit gate (history validation)
- [ ] Migration: v0.13 → v0.14 auto-conversion
- [ ] Tests: 15+ test cases for history immutability

### Week 2: Retirement Automation
- [ ] `bin/adr-retire`: 4-signal detection
- [ ] `/adr-kit:retire` skill: interactive approval
- [ ] GitHub Actions: weekly audit cron template
- [ ] Tests: 15+ retirement scenarios
- [ ] Documentation: "ADR retirement guide"

**Milestone:** "ADRs immutable; status tracked; retirement automation in place"

---

## Phase 2 Timeline (Weeks 3–4)

### Week 3: Performance Budgets
- [ ] adr-judge: --profile mode with per-rule timing
- [ ] Pre-commit hook: warn if >5s (non-blocking)
- [ ] .adr-kit.json: timeout configuration
- [ ] adr-lint: detect slow patterns (excessive backtracking, etc.)
- [ ] Tests: <5s performance on 30-ADR repo

### Week 4: Health Dashboard
- [ ] `bin/adr-status`: health report generator
- [ ] `/adr-kit:status` skill: interactive dashboard
- [ ] GitHub Actions: weekly health cron (create issue if candidates)
- [ ] Tests: 30-ADR repo produces expected output
- [ ] Documentation: "ADR health monitoring"

**Milestone:** "ADR health visible; enforcement performance observable; audits automated"

---

## Success Criteria

### Operational
- [ ] v0.14.0 released with status history + retirement detection
- [ ] Weekly health audits automated (GitHub Actions cron)
- [ ] <5s pre-commit hook on repos with 50 ADRs
- [ ] 0 regression bugs vs. v0.13

### Quality
- [ ] 25+ new test cases (5+ per feature)
- [ ] Migration tested on 2–3 real repos
- [ ] All features documented with examples
- [ ] Backward compatible with v0.13 ADRs

### Adoption
- [ ] v0.14.0 installable via plugin marketplace
- [ ] 2+ active repos using retirement automation within 1 month
- [ ] <1h onboarding time for new users (unchanged vs. v0.13)

---

## Integration Examples

### Example 1: v0.13 → v0.14 Migration
```bash
cd existing-project-with-v0.13-adrs/
/adr-kit:upgrade  # or /adr-kit:init

# Auto-migration runs:
# - Scans all ADRs, backfills status_history with legacy status + date
# - Adds new frontmatter fields (superseded_by, domain_tags, etc.)
# - Tests: status_history immutability confirmed
```

### Example 2: Weekly Retirement Audit
```bash
# GitHub Actions runs Monday 6am UTC
bin/adr-retire --format markdown --output-file /tmp/report.md

# Output includes:
# - Summary (total, active, candidates)
# - Candidates with signals + confidence
# - Recommended actions
# - Links to create issues

# GitHub Actions creates issue if candidates exist
```

### Example 3: Performance Monitoring
```bash
# During development
git add src/main.py
bin/adr-judge --diff <(git diff --cached) --profile

# Output:
# ✓ forbid_import (ADR-0001): 0.12ms
# ✓ forbid_pattern (ADR-0002): 0.34ms
# ⚠️ llm_judge (ADR-0012): 1.2s (timeout: 30s)
# Total: 1.66s / 5000ms ✓
```

---

## Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Status history migration fails | Low | High | Extensive tests; dry-run option; rollback |
| Retirement detection too noisy | Medium | Medium | Conservative thresholds; confidence scores; manual approval |
| Performance regression | Low | Medium | Profiling from day 1; <5s gate in tests |
| Backward compat breaks | Low | High | Auto-migration tested on real repos |

---

## Files Modified/Created

### Week 1–2
- templates/adr-template.md (update)
- bin/adr-judge (enhance: status history, parsing, appending)
- bin/adr-lint (enhance: Audit gate)
- bin/adr-retire (NEW)
- skills/retire/SKILL.md (NEW)
- tests/test_adr_status_history.py (NEW)
- tests/test_adr_retire.py (NEW)

### Week 3–4
- bin/adr-judge (enhance: --profile)
- bin/adr-lint (enhance: slow pattern detection)
- bin/adr-status (NEW)
- templates/githooks/pre-commit (update: timing warning)
- .adr-kit.json (schema update)
- skills/status/SKILL.md (NEW)
- .github/workflows/adr-retire-audit.yml (NEW)
- tests/test_adr_performance.py (NEW)

---

## Next Steps

1. **Review & approval:** Confirm timeline, identify implementation lead
2. **Setup:** Create v0.14-dev branch, feature branches per week
3. **Week 1 kickoff:** Start template + status history logic
4. **Weekly reviews:** Check progress, iterate on edge cases
5. **Field testing:** Deploy v0.14.0-beta to 2–3 repos before stable
<!-- SECTION:PLAN:END -->
