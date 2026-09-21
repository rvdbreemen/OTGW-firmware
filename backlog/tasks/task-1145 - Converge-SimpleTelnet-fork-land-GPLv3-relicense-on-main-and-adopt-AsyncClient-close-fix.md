---
id: TASK-1145
title: >-
  Converge SimpleTelnet fork: land GPLv3 relicense on main and adopt
  AsyncClient::close() fix
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-21 18:43'
updated_date: '2026-09-21 18:45'
labels: []
dependencies: []
ordinal: 284000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SimpleTelnet submodule pins 123106f (GPLv3 relicense), which is a sibling of origin/main (a15d7dd). The AsyncClient::close(bool) deprecation fix was merged upstream as PR #6 on main, but main is still MIT because the relicense never landed there. Result: the pinned commit carries GPLv3 but lacks the fix, producing ~27 deprecation warning instances per build (2 source lines in the MAX_CLIENTS template header). Converge by bringing the GPLv3 relicense onto main, then repointing the submodule.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GPLv3 relicense is present on SimpleTelnet origin/main
- [ ] #2 origin/main contains the AsyncClient::close() fix (no close(bool) call sites remain)
- [ ] #3 OTGW-firmware submodule pointer updated to the converged main commit
- [ ] #4 Build green for all three ESP32 targets after the pointer bump
- [ ] #5 AsyncClient::close(bool) deprecation warnings no longer appear in the build log
<!-- AC:END -->
