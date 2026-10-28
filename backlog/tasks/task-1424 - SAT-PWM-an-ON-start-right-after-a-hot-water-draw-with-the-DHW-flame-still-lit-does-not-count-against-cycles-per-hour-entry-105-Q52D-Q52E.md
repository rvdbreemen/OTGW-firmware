---
id: TASK-1424
title: >-
  SAT PWM: an ON start right after a hot-water draw with the DHW flame still lit
  does not count against cycles per hour (entry 105, Q52D/Q52E)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-28T11:19:00Z'
updated_date: '2026-10-28T11:42:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
references:
  - docs/sat/sergeant-d-corrections.md
priority: high
ordinal: 588000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28 Q52D 'No' (entry 105, msgs 1564954645489782858, 1564956502849093675): corrects our entry 101 reading that TASK-1421 shipped in alpha.517/518 (counts such a start, as pwm.py:160). B (start, not counted; remember a flame lit while the boiler DHW-active bit was set) or C (no ON start while that flame burns; start and count after it goes out) asked as Q52E. Implementation waits for that answer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's choice B or C (Q52E) recorded in docs/sat/sergeant-d-corrections.md before implementation
- [ ] #2 Host test: ON start at the first tick after a draw with the flame still lit does not add to cycles_this_hour (red on alpha.518, green on fix)
- [ ] #3 A normal unlit or CH-lit ON start still counts (TASK-1421 tests stay green)
- [ ] #4 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 Q52E C (entry 106, msg 1564962902509752442): no ON start while a flame lit during boiler DHW-active still burns; start after it goes out, counted.
<!-- SECTION:NOTES:END -->
