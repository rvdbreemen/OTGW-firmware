---
id: TASK-708
title: 'chore(adr-kit): run first retirement-audit on dev ADR set once skill available'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-26 11:01'
updated_date: '2026-06-02 16:14'
labels:
  - adr-kit
  - tooling
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Blocked on rvdbreemen/adr-kit issue #8 landing.\n\nOnce /adr-kit:retirement-audit is available, run it on the dev worktree (79 ADRs, ADR-001 to ADR-080). ADR-078 (defer HA Core aliases to 2.0.0) is already a likely candidate on dev since the SAT removal makes the defer-decision moot. Review all candidates and archive or deprecate as appropriate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 /adr-kit:retirement-audit skill is available in installed adr-kit version
- [ ] #2 Audit run on dev worktree; all candidates reviewed with user
- [ ] #3 ADR-078 specifically evaluated: deprecate or supersede as appropriate
- [ ] #4 Same audit run on 2.0.0 worktree (111 ADRs)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-02 audit run (adr-kit 0.19.0 bin/adr-retire, dev docs/adr, 83 ADRs):
- Result: 0 RETIRE, 0 REVIEW, 2 MONITOR(0.50), rest KEEP.
- MONITOR ADR-017 (WiFiManager) + ADR-025 (Safari WS upload): both = staleness(1.0)+tech_removal(1.0). Verified BOTH false positives: WiFiManager live (captive portal, pinned 2.0.15-rc.1); Safari WS handling live in webSocketStuff.ino + index.js/css. tech_removal is a case-sensitive single-token grep that misses multiword/versioned terms.
- ADR-078 evaluated: KEEP 0.00. Task hypothesis FALSIFIED - ADR-078 is about HA-Core alias topics (supersedes ADR-077), NOT SAT; it is the live record of the dev revert and must be kept.
- Conclusion dev: no deprecation/archival action warranted.
- Blocker cleared: /adr-kit retire skill + bin/adr-retire now present in plugin cache (0.13.3-0.19.0); adr-kit issue #8 landed.
<!-- SECTION:NOTES:END -->
