---
id: TASK-842
title: >-
  Rebuild 1.6.0 beta ladder for binary-search bisect of the HTTP-fragmentation
  regression
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-07 18:03'
updated_date: '2026-06-07 21:14'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Root cause (HTTP-serving heap fragmentation) is contained by the beta.2 gate, but the introducing commit is unknown (likely a cumulative threshold). Rebuild the 12 rebuildable in-line dev betas of the 1.6.0 cycle (beta.1,4,5,6,7,16,20,21,22,23,24,25) plus anchors 1.5.0 (GOOD) and 1.6.0 (BAD), firmware+filesystem each, labeled, into a root testset folder. George binary-searches: BAD -> go earlier, GOOD -> go later, until the exact GOOD/BAD boundary build is found. Builds done in an isolated temp worktree so the dirty dev tree is untouched. Artifacts are gitignored (not committed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All 12 betas + 2 anchors built firmware+filesystem from their in-line dev commits in an isolated worktree; dev working tree untouched
- [x] #2 Artifacts labeled by beta number (.ino.bin + .littlefs.bin + version) under a single root testset folder, which is gitignored
- [x] #3 A binary-search worksheet (order + BAD-go-earlier / GOOD-go-later rule + GOOD/BAD criterion) is provided for George
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Extended ladder: built the 9 missing reproducible 1.6.0-cycle rungs (beta.2,3,10,11,12,11r,13,15,19) after confirming their version.h commits exist (earlier 'no build' claim was wrong - first scan filtered on chore(release) message pattern and missed bumps riding feature commits). Full ladder now 23 builds. Bisect progress: beta.5 GOOD, beta.6 GOOD (served 521 HTTP reqs), beta.16 BAD (Exception epc1=4000df64). Boundary in beta.6->beta.16; next midpoint 05c_beta.12. Worktree removed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Built the full 1.6.0 beta bisect ladder (14 points: anchors 1.5.0 GOOD + 1.6.0 BAD, plus the 12 rebuildable in-line dev betas 1,4,5,6,7,16,20,21,22,23,24,25), firmware+filesystem each, in an isolated detached worktree (../OTGW-firmware-bisect, since removed) so the dirty dev tree was never touched. Artifacts labeled 00_..13_ by chronological order under bisect-testset/ (gitignored, 46MB, 2 bins each) with a README.txt giving the flash steps, GOOD/BAD criterion (maxBlock floor + reboot_log) and the binary-search rule (BAD=go earlier, GOOD=go later). 13/14 built first pass; 1.5.0 failed on a worktree nested-lib population race and was rebuilt to complete 14/14. Binary search starts at 06_beta.16. Each build self-identifies via its 1.6.0-beta.N+<sha> version banner.
<!-- SECTION:FINAL_SUMMARY:END -->
