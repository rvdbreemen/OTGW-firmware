---
id: TASK-2924
title: >-
  Telnet debug help: the H) heap-hold line prints on 'l' (toggle MyDEBUG)
  instead of in the 'h' menu
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T18:36:00Z'
updated_date: '2026-11-19T18:50:00Z'
labels:
  - telnet
  - debug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1061000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-docs, 2026-11-19: handleDebug.ino prints 'H) bench: starve the heap / release it (prerelease builds only)' inside case 'l' (toggle MyDEBUG), so the 'h' help menu never lists the H key and pressing l prints an unrelated line. Prerelease builds only (#if defined(_VERSION_PRERELEASE)).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The H) line is printed in the 'h' help menu (prerelease builds only) and no longer by case 'l'
- [x] #2 Host check on the real handleDebug.ino source: red on the old placement, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 otgw-dev: H) help line moved from case 'l' to the 'h' menu (after z), inside the _VERSION_PRERELEASE guard. tests/test_debug_help_heaphold.py reads the real handleDebug.ino: red on the old placement (2 of 3 failing: H) not in the 'h' block, H) printed by 'l'), green 3/3 on the fix; tests/test_heaphold_hook.py 8/8. Bench for AC1 (09, .61): 'h' lists H), 'l' only toggles MyDEBUG.
<!-- SECTION:NOTES:END -->
