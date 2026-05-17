---
id: TASK-608
title: >-
  Resolve 4 unresolved ADR references flagged by evaluate.py --quick (dev
  baseline)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 07:26'
updated_date: '2026-05-17 10:03'
labels:
  - ci
  - tech-debt
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py --quick fails the [ADR] References resolve check with '4 unresolved ADR reference(s) out of 1224' on the dev baseline (verified at commit 97fd601: Failed:1, Health 91.7%, identical before/after docs PR #581). Pre-existing baseline failure surfaced during the documentation review; it keeps every dev PR's 'Run evaluate.py --quick' check red and masks genuinely new evaluate.py failures. Identify the 4 dangling ADR references and resolve each (fix the citing reference, restore/add the missing ADR, or correct a supersession note).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The 4 unresolved ADR references are enumerated with citing file:line and the ADR-NNN each points at
- [ ] #2 Each reference is resolved (corrected citation, added/restored ADR, or fixed supersession link) with rationale recorded
- [ ] #3 python evaluate.py --quick reports the [ADR] References resolve check as passing (0 unresolved out of total)
- [ ] #4 python evaluate.py --quick shows no new failures vs the documented baseline (Health Score improves; Failed count for this check is 0)
<!-- AC:END -->
