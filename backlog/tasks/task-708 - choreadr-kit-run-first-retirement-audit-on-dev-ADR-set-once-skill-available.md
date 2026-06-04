---
id: TASK-708
title: 'chore(adr-kit): run first retirement-audit on dev ADR set once skill available'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-26 11:01'
updated_date: '2026-06-04 22:20'
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
- [x] #1 /adr-kit:retirement-audit skill is available in installed adr-kit version
- [x] #2 Audit run on dev worktree; all candidates reviewed with user
- [x] #3 ADR-078 specifically evaluated: deprecate or supersede as appropriate
- [x] #4 Same audit run on 2.0.0 worktree (111 ADRs)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-02 audit run (adr-kit 0.19.0 bin/adr-retire, dev docs/adr, 83 ADRs):
- Result: 0 RETIRE, 0 REVIEW, 2 MONITOR(0.50), rest KEEP.
- MONITOR ADR-017 (WiFiManager) + ADR-025 (Safari WS upload): both = staleness(1.0)+tech_removal(1.0). Verified BOTH false positives: WiFiManager live (captive portal, pinned 2.0.15-rc.1); Safari WS handling live in webSocketStuff.ino + index.js/css. tech_removal is a case-sensitive single-token grep that misses multiword/versioned terms.
- ADR-078 evaluated: KEEP 0.00. Task hypothesis FALSIFIED - ADR-078 is about HA-Core alias topics (supersedes ADR-077), NOT SAT; it is the live record of the dev revert and must be kept.
- Conclusion dev: no deprecation/archival action warranted.
- Blocker cleared: /adr-kit retire skill + bin/adr-retire now present in plugin cache (0.13.3-0.19.0); adr-kit issue #8 landed.

2026-06-05 AC#4 (2.0.0 audit): blocked by a tool bug, root-caused and fixed.
- Root cause: ENFORCEMENT_BLOCK_RE in bin/adr-retire used a nested lazy quantifier (?:.*?
)*? under re.DOTALL. On an ADR with a ## Enforcement heading but no ```json fence, the regex backtracked catastrophically (~0.75s/7.5KB ADR, file-size dependent), stacking to a multi-minute hang across the set. 2.0.0 has 4 fence-less Enforcement ADRs: ADR-115/116/118/120. Not a file-count issue (src-scoped run still hung); not parallelizable (single regex).
- Fix: replaced with extract_section(Enforcement) + simple non-nested ```json fence match. Linear. Behaviour preserved (fence-less => no rules => policy_mismatch 0.0). Verified: 0.118s over 128 ADRs, 0 capture differences vs old regex on the 14 fenced ADRs. Committed dev b70762ed + 2.0.0 b5d85efe.
- Audit result (patched tool, --repo-root src): 123 ADRs, ALL KEEP, 0 retirement candidates on 2.0.0. The 4 fence-less Enforcement ADRs correctly score 0. No deprecation/archival action warranted on 2.0.0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ran the first ADR retirement audit on the 2.0.0 worktree (AC#4), the last open criterion of TASK-708.

The audit could not run: bin/adr-retire hung for minutes on the 123-ADR set. Root-caused to catastrophic regex backtracking in ENFORCEMENT_BLOCK_RE (nested lazy quantifier (?:.*?
)*? under DOTALL) triggered by 4 ADRs with a ## Enforcement heading but no ```json fence (ADR-115/116/118/120). Fixed by replacing the regex with a linear extract_section + simple fence match; behaviour preserved, capture parity verified, runtime cut from timeout to <1s of regex work. Fix committed on both branches (dev b70762ed, 2.0.0 b5d85efe) and to be filed upstream (rvdbreemen/adr-kit).

Audit outcome: 123 ADRs, all KEEP, zero retirement candidates on 2.0.0. No deprecate/archive/supersede action warranted. Combined with the 2026-06-02 dev run (also no action), TASK-708 closes with no ADR retirements on either worktree.
<!-- SECTION:FINAL_SUMMARY:END -->
