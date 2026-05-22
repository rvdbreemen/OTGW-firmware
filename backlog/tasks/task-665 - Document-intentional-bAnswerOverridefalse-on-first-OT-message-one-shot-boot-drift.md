---
id: TASK-665
title: >-
  Document intentional bAnswerOverride=false on first OT message (one-shot boot
  drift)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 06:23'
updated_date: '2026-05-22 06:44'
labels:
  - docs
  - correctness
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Review finding L4: OTGW-Core.ino:4070-4106 — the very first OT message after boot (cntOTmessagesprocessed==1) is stored directly into delayedOTdata without running the override-detection path. bAnswerOverride=false is set before the branch. If the first frame ever processed is an A that happens to be an answer-override (gateway booted mid-pair), the gate would let it reach canonical/boiler topics once. Bounded to one frame; corrects itself on the next (B,A) pair. Agent recommended 'no action — informational'. Per project policy: surface this intentional one-shot bypass with a code comment so a future reader doesn't repeat the audit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Code comment added at OTGW-Core.ino:4070-area explaining the one-shot bypass: 'First OT frame after boot bypasses override-detection (no prior B to pair against). Worst case: one A-frame reaching canonical/boiler before the next (B,A) pair corrects it. Acceptable cosmetic drift; intentional.'
- [ ] #2 Same fix applied to 2.0.0 worktree (OTGW-Core.ino:4067/4074/4081/4112 — same logic)
- [x] #3 Both branches: comment placement does not change behaviour; version-bump policy: comment-only change is dev-firmware-code so requires prerelease bump per CLAUDE.md
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added comment block at src/OTGW-firmware/OTGW-Core.ino:4076 making the one-shot boot drift explicit. The comment documents (a) why the first frame cannot run (B,A)/(T,R) substitution detection — no prior delayedOTdata to pair against, (b) the worst-case drift — first frame happening to be an answer-override A reaches _boiler/canonical once, (c) the self-correction — next (B,A) pair recomputes correctly. No behaviour change. Prerelease 1.6.0-beta.17 -> 1.6.0-beta.18. evaluate.py --quick 34 pass / 0 fail. AC #2 (2.0.0 mirror) will be covered by the cross-branch port task. AC #3 (bump) confirmed. Commit 30e95210.
<!-- SECTION:FINAL_SUMMARY:END -->
