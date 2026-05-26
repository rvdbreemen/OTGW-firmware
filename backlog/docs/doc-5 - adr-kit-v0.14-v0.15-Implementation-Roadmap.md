---
id: doc-5
title: adr-kit v0.14-v0.15 Implementation Roadmap
type: other
created_date: '2026-05-26 12:41'
---
# adr-kit v0.14–v0.15 Implementation Roadmap

## Overview
8-feature implementation plan spanning 4 phases across 8 weeks (v0.14.0 → v0.15.1). These features address the knowledge-noise problem identified in claude-mini issue #138: 20 ADRs created in 8 days without retirement mechanisms causes decision fatigue.

## Phase 1: Foundation (v0.14.0) – Weeks 1–2

### Feature 1: Append-Only Status History for ADRs (Week 1)
**Goal:** Add immutable status tracking to ADR template and implement parsing/appending logic in adr-judge.

**Deliverables:**
- Update `templates/adr-template.md` with `status_history` field (YAML array: date, status, changed_by, reason, changed_via)
- Enhance `bin/adr-judge` with `parse_status_history()` and `append_to_status_history()` functions
- Auto-migrate v0.13 ADRs to v0.14 format on first judge run
- Implement status_history validation in `bin/adr-lint` (new Audit gate)
- Create comprehensive test suite (20+ cases)

**Acceptance Criteria:**
- Status history field added to adr-template.md with YAML structure documented
- parse_status_history() function implemented in adr-judge with backward compatibility
- append_to_status_history() function implemented and never overwrites existing entries
- Auto-migration code converts v0.13 ADRs on first judge run without data loss
- Audit gate in adr-lint validates status_history is append-only and consistent
- 20+ test cases passing (test_adr_status_history.py)
- All existing tests still pass
- Documentation updated with status_history examples

### Feature 2: Automated Retirement Detection (Week 2)
**Goal:** Implement bin/adr-retire tool with 4 retirement signal detectors.

**Deliverables:**
- New `bin/adr-retire` CLI tool (~400 lines)
- Four detector functions: detect_90day_staleness(), detect_tech_removal(), detect_supersession_broken(), detect_policy_mismatch()
- JSON and Markdown output with confidence scores
- GitHub Actions workflow for weekly retirement audits
- Create companion skill (skills/retire/SKILL.md)
- Create comprehensive test suite (15+ cases)

**Performance Requirement:** <2s on 30-ADR repository

**Acceptance Criteria:**
- bin/adr-retire tool created with all 4 detectors implemented
- Each detector returns confidence score (0.0-1.0) based on signal strength
- JSON output includes all 4 signal scores for each candidate
- Markdown output is human-readable with ranked candidates
- Performance tested: <2s execution on 30-ADR test suite
- 15+ test cases passing (test_adr_retire.py)
- GitHub Actions workflow (.github/workflows/adr-retire-audit.yml) creates weekly issues
- skills/retire/SKILL.md created with usage examples
- All existing tests still pass

---

## Phase 2: Observability & Ranking (v0.14.1) – Weeks 3–4

### Feature 3: Performance-Bounded Hooks (Week 3)
**Goal:** Add timing instrumentation to git hooks and pre-commit workflows.

**Deliverables:**
- Add `--profile` flag to `bin/adr-judge` for per-rule timing output
- Add `--dry-run-enforcement ADR-XXX` mode for testing single ADRs
- Update `templates/githooks/pre-commit` to measure and warn on >5s executions
- Create performance-budgeting configuration in `.adr-kit.json`
- Create comprehensive test suite (10+ cases)

**Performance Requirement:** pre-commit <5s, pre-push <15s

**Acceptance Criteria:**
- --profile flag added to adr-judge, outputs per-rule timing and budget utilization
- --dry-run-enforcement flag added, tests single ADR without modifying state
- pre-commit hook measures execution time and warns non-blockingly if >5s
- pre-push hook timeout set to 15s with graceful fallback
- .adr-kit.json created with configurable timeouts (pre_commit_timeout_ms, pre_push_timeout_ms)
- Timing data collected via environment hooks, not modifying ADR content
- 10+ performance test cases passing (test_adr_performance.py)
- All existing tests still pass

### Feature 4: Semantic Relevance Ranking (adr-context) (Week 4)
**Goal:** Implement bin/adr-context tool for intelligent ADR lookup using heuristic scoring.

**Deliverables:**
- New `bin/adr-context` CLI tool (~250 lines)
- Five scoring signals with configurable weights in `.adr-kit.json`
- extract_keywords() and infer_task_domain() functions
- score_adr() function with weighted scoring algorithm
- Integrate into agents/adr-generator.md for context injection
- Update skills/judge/SKILL.md with "Load relevant ADRs?" prompt
- Create comprehensive test suite (15+ cases)

**Performance Requirement:** <100ms on 30-ADR repository

**Scoring Signal Weights (configurable):**
- Keywords: 0.40
- Domain tags: 0.25
- Related decisions: 0.15
- Acceptance status: 0.10
- Recency: 0.10

**Acceptance Criteria:**
- bin/adr-context tool created with all 5 scoring signals
- Weights configurable in .adr-kit.json with sensible defaults
- extract_keywords() parses task query and returns ranked keyword list
- infer_task_domain() returns one of: backend, frontend, infra, security, data
- score_adr() returns score 0.0-1.0 based on weighted signal combination
- load_adr_context() returns top N ranked ADRs (default N=5, configurable)
- Performance tested: <100ms on 30-ADR test suite
- agents/adr-generator.md updated with context injection flow
- skills/judge/SKILL.md updated with context loading prompt
- 15+ test cases passing (test_adr_context.py)
- All existing tests still pass

---

## Phase 3: Validation & Portability (v0.15.0) – Weeks 5–6

### Feature 5: Policy Block Validation (Week 5)
**Goal:** Enhance policy enforcement with stricter JSON schema validation and regex compilation checks.

**Deliverables:**
- Create `schemas/adr-enforcement.schema.json` with stricter validation
- Add Policy gate to `bin/adr-lint` (validates schema, compiles regexes, warns on performance anti-patterns)
- Add Quality gate to adr-lint (advisory: vague language detection, <2 alternatives, missing metrics)
- Create `.adr-kit.json` configuration schema
- Create comprehensive test suite (10+ cases)

**Non-Blocking Warnings For:**
- Unescaped dots
- Broad .* sequences
- path_globs affecting >50% of repo

**Acceptance Criteria:**
- schemas/adr-enforcement.schema.json created with strict validation
- Policy gate added to adr-lint, validates JSON schema compliance
- Policy gate compiles all regex patterns and reports errors
- Policy gate warns on anti-patterns: unescaped dots, excessive .* sequences, overly broad path_globs
- Quality gate added to adr-lint (advisory), detects vague language, missing metrics, <2 alternatives
- .adr-kit.json schema documented with all configurable fields
- Configuration loading tested with valid and invalid JSON
- 10+ test cases passing (test_adr_policy.py)
- All existing tests still pass

### Feature 6: Multi-Language Script Generation (Week 6)
**Goal:** Implement bin/adr-generate-scripts tool to produce standalone validation scripts.

**Deliverables:**
- New `bin/adr-generate-scripts` CLI tool (~350 lines)
- Three template scripts: `templates/validate_adr_template.go`, `.rs`, `.sh`
- generate_go_script(), generate_rust_script(), generate_shell_script() functions
- Scripts output to project root (.generated)
- Integrate into skills/init/SKILL.md with "Generate validation scripts?" prompt
- Create comprehensive test suite (15+ cases)

**Scripts Must Be:**
- 100% standalone (no adr-kit imports)
- Executable without dependencies
- Validate against same Enforcement rules as adr-judge

**Acceptance Criteria:**
- bin/adr-generate-scripts tool created with all 3 language generators
- templates/validate_adr_template.go implemented (~80 lines) with checkForbidImport() function
- templates/validate_adr_template.rs implemented (~80 lines) with Regex crate usage
- templates/validate_adr_template.sh implemented (~50 lines) with grep-based validation
- Scripts generated to .generated/ directory, are standalone executables
- Scripts validate against same Enforcement rules as adr-judge
- Generated scripts tested in CI/CD pipeline
- skills/init/SKILL.md updated with script generation prompt
- 15+ test cases passing (test_adr_generate_scripts.py)
- All existing tests still pass

---

## Phase 4: Intelligence & Guidance (v0.15.1) – Weeks 7–8

### Feature 7: ADR Health Dashboard (adr-status) (Week 7)
**Goal:** Implement bin/adr-status tool for repository-wide visibility.

**Deliverables:**
- New `bin/adr-status` CLI tool (~300 lines)
- Summary statistics: total, accepted, proposed, superseded, deprecated, avg age
- Enforcement health table: violations, timing, policy validity per ADR
- Retirement candidate summary with actionable recommendations
- JSON, Markdown, and table output formats
- Integrate into agents/adr-generator.md as post-decision feedback
- Create comprehensive test suite (15+ cases)

**Performance Requirement:** <500ms on 30-ADR repository

**Acceptance Criteria:**
- bin/adr-status tool created with all statistics and health checks
- Summary stats calculated: total, accepted, proposed, superseded, deprecated, avg age
- Enforcement health table generated per ADR with violation counts and timing data
- Retirement candidates identified and ranked by confidence score
- JSON output includes all statistics and candidate details
- Markdown output is human-readable with formatted tables
- Table output suitable for terminal display with alignment
- Performance tested: <500ms on 30-ADR test suite
- agents/adr-generator.md updated with status feedback integration
- 15+ test cases passing (test_adr_status.py)
- All existing tests still pass

### Feature 8: Agent Lifecycle Guidance (Week 8)
**Goal:** Enhance agents/adr-generator.md with decision tree and quality scoring.

**Deliverables:**
- Decision tree in agents/adr-generator.md: when ADRs needed
- Quality scoring algorithm using 4 gates
- Return score breakdown + list of specific quality issues
- Integrate adr-context for relevant ADR injection
- Integrate adr-status for post-decision feedback
- Create comprehensive test suite (10+ cases)

**Quality Gates:**
1. Completeness (all sections filled)
2. Evidence (citations/references)
3. Clarity (readability score)
4. Consistency (relationships to other ADRs)

**Decision Tree:**
- Library choice? → YES → ADR
- Code pattern? → YES → ADR
- Governance? → YES → optional ADR
- Else → no ADR needed

**Acceptance Criteria:**
- Decision tree added to agents/adr-generator.md
- Quality scoring algorithm implemented with 4 gates
- Completeness gate checks all sections filled and >100 chars description
- Evidence gate checks citations/references count and quality
- Clarity gate uses readability heuristics (Flesch-Kincaid equivalent)
- Consistency gate validates relationships to other ADRs are bidirectional
- Score returned as 0.0-1.0 with breakdown per gate
- Specific quality issues listed with actionable recommendations
- adr-context integrated for relevant ADR suggestions on creation
- adr-status integrated for post-decision feedback
- 10+ test cases passing (test_adr_quality.py)
- All existing tests still pass

---

## Testing Strategy

**Total Test Cases: 110+**

- Feature 1 (Status History): 20 test cases
- Feature 2 (Retirement Detection): 15 test cases
- Feature 3 (Performance): 10 test cases
- Feature 4 (Relevance Ranking): 15 test cases
- Feature 5 (Policy Validation): 10 test cases
- Feature 6 (Script Generation): 15 test cases
- Feature 7 (Health Dashboard): 15 test cases
- Feature 8 (Agent Guidance): 10 test cases

All tests use pytest with parametrized fixtures and mocks for external dependencies.

---

## Release Schedule

- **v0.14.0** (end of Week 2): Features 1–2 (Status History + Retirement Detection)
- **v0.14.1** (end of Week 4): Features 3–4 (Performance + Relevance Ranking)
- **v0.15.0** (end of Week 6): Features 5–6 (Policy Validation + Script Generation)
- **v0.15.1** (end of Week 8): Features 7–8 (Health Dashboard + Agent Guidance)

---

## Key Architectural Decisions

1. **Status History Format:** YAML array in frontmatter (human-readable, git-diff-friendly)
2. **Append-Only Enforcement:** Enforced at write time, not read time (prevents accidental rewrites)
3. **Auto-Migration:** Happens silently, preserves v0.13 ADRs without manual action
4. **Backward Compatibility:** v0.13 ADRs work with v0.14 adr-judge without changes
5. **Performance Budgeting:** Configurable thresholds with non-blocking warnings
6. **Standalone Scripts:** Generated scripts have zero external dependencies
7. **Quality Scoring:** Deterministic heuristic gates (no LLM round-trip for scoring)
