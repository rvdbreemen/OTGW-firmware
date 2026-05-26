---
id: TASK-711
title: 'adr-kit Feature 2: Automated Retirement Detection'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 13:37'
updated_date: '2026-05-26 16:52'
labels:
  - adr-kit
  - phase-1
  - week-2
  - detection
  - backend
milestone: v0.14.0
dependencies: []
references:
  - bin/adr-retire
  - IMPLEMENTATION_STATUS_v0.14-v0.15.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement bin/adr-retire tool with 4 retirement signal detectors (90-day staleness, tech removal, broken supersession, policy mismatch). Returns ranked candidates with confidence scores in JSON/Markdown.

PLAN - WEEK 2 (v0.14.0 Phase 1):

1. Core Tool (DONE - skeleton):
   - bin/adr-retire created (~400 lines)
   - Four detector functions implemented
   - Confidence scoring (0.0-1.0) per signal
   - JSON/Markdown output formats

2. Signal 1: detect_90day_staleness()
   - Check status_history for recent updates
   - Score 1.0 if no updates in 90+ days
   - Score 0.0 if recent (within 90 days)
   - Default threshold: 90 days (configurable in .adr-kit.json)

3. Signal 2: detect_tech_removal()
   - Extract technology keywords from Decision section
   - Check if tech still present in codebase
   - Score 1.0 if tech is gone, 0.0 if present
   - Heuristic scan: grep for tech name in files

4. Signal 3: detect_supersession_broken()
   - Look for "Superseded by ADR-XXX" in content
   - Check if superseding ADR exists
   - Score 1.0 if reference is broken, 0.0 if valid
   - Always enabled (catches orphaned ADRs)

5. Signal 4: detect_policy_mismatch()
   - Parse ADR Enforcement block
   - Check for anti-patterns: unescaped dots, excessive .*, broad globs
   - Score based on number of issues
   - Threshold: 0.5+ issues/rules = potential mismatch

6. Scoring & Recommendations:
   - retirement_score = avg(signal1, signal2, signal3, signal4)
   - RETIRE (score >= 0.8), REVIEW (>= 0.6), MONITOR (>= 0.4), KEEP (< 0.4)

7. Output Formats: JSON, Markdown, Text with CLI flags

8. GitHub Actions: Weekly cron Monday 6am UTC, create issue if candidates found

9. Skills Integration: skills/retire/SKILL.md with user prompt

10. Testing (15+ test cases covering all signals, scoring, output formats, performance <2s)

DELIVERABLES:
✓ bin/adr-retire (~400 lines)
→ .github/workflows/adr-retire-audit.yml
→ skills/retire/SKILL.md
→ tests/test_adr_retire.py (15+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bin/adr-retire tool created with all 4 detectors implemented
- [ ] #2 Each detector returns confidence score (0.0-1.0)
- [ ] #3 Retirement score = average of 4 signals
- [ ] #4 JSON output includes all signal scores
- [ ] #5 Markdown output is human-readable
- [ ] #6 Recommendations: RETIRE (>=0.8), REVIEW (>=0.6), MONITOR (>=0.4), KEEP (<0.4)
- [ ] #7 Performance <2s on 30-ADR suite
- [ ] #8 15+ test cases passing
- [ ] #9 GitHub Actions workflow creates weekly issues
- [ ] #10 skills/retire/SKILL.md created
- [ ] #11 All existing tests pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-26: Picked up by @codex for Phase 1 execution. Implementation target is the standalone repo D:\Users\Robert\Documents\GitHub\RvdB\adr-kit on branch v0.14-dev. Initial inspection found bin/adr-retire present but with placeholder staleness, technology-removal, and policy-mismatch detectors plus placeholder tests; the workflow and retire skill deliverables are not yet present.
<!-- SECTION:NOTES:END -->
