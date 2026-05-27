---
id: TASK-716
title: 'adr-kit Feature 7: ADR Health Dashboard'
status: Done
assignee: []
created_date: '2026-05-26 13:43'
labels:
  - adr-kit
  - phase-4
  - week-7
  - dashboard
  - observability
milestone: v0.15.1
dependencies: []
references:
  - bin/adr-status
  - bin/adr-retire
  - agents/adr-generator.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement bin/adr-status tool for repository-wide visibility. Reports total count, accepted/proposed/superseded/deprecated breakdown, average age, enforcement health table per ADR, retirement candidates with recommendations. JSON/Markdown/table output.

PLAN - WEEK 7 (v0.15.1 Phase 4):

1. Core Tool (DONE - skeleton):
   - bin/adr-status created (~300 lines)
   - Summary statistics calculated
   - Multiple output formats

2. Summary Statistics:
   - Total ADR count
   - Status breakdown: accepted, proposed, superseded, deprecated
   - Average age (days)
   - Recent changes (last 7 days)
   - Health percentage (accepted %)

3. Enforcement Health Table:
   - Per-ADR violation count
   - Per-ADR timing data
   - Policy validity status
   - Last checked date
   - Recommendations for each

4. Retirement Candidates:
   - Identify candidates from adr-retire
   - Rank by confidence score
   - Show top 5-10 candidates
   - Include recommendation

5. Output Formats:
   - JSON: machine-readable with all details
   - Markdown: formatted for documentation
   - Table: terminal-friendly aligned format
   - HTML (optional): formatted for web

6. Integration:
   - agents/adr-generator.md: post-decision feedback
   - GitHub Actions: daily health checks
   - Dashboard view in web UI (optional)

7. Testing (15+ test cases):
   - Summary stats accuracy
   - Status breakdown counts
   - Age calculation
   - Enforcement health table
   - Retirement candidates
   - JSON output format
   - Markdown output format
   - Table alignment
   - Performance <500ms
   - And more

DELIVERABLES:
✓ bin/adr-status (~300 lines)
→ agents/adr-generator.md (feedback integration)
→ tests/test_adr_status.py (15+ cases)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 bin/adr-status tool created with all statistics
- [ ] #2 Summary stats: total, status breakdown, average age
- [ ] #3 Enforcement health table with violations and timing
- [ ] #4 Retirement candidates identified and ranked
- [ ] #5 JSON output includes all statistics
- [ ] #6 Markdown output is human-readable
- [ ] #7 Table output with proper alignment
- [ ] #8 Performance <500ms on 30-ADR suite
- [ ] #9 agents/adr-generator.md updated with feedback
- [ ] #10 15+ test cases passing
- [ ] #11 All existing tests pass
<!-- AC:END -->
