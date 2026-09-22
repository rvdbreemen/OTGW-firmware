---
id: TASK-1145
title: >-
  Converge SimpleTelnet fork: land GPLv3 relicense on main and adopt
  AsyncClient::close() fix
status: Done
assignee:
  - '@claude'
created_date: '2026-09-21 18:43'
updated_date: '2026-09-21 20:03'
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
- [x] #1 GPLv3 relicense is present on SimpleTelnet origin/main
- [x] #2 origin/main contains the AsyncClient::close() fix (no close(bool) call sites remain)
- [x] #3 OTGW-firmware submodule pointer updated to the converged main commit
- [x] #4 Build green for all three ESP32 targets after the pointer bump
- [x] #5 AsyncClient::close(bool) deprecation warnings no longer appear in the build log
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Premise correction: the task description assumed the convergence still had to be made. It did not. Upstream had already re-applied the GPLv3 relicence on main (51f7f34, bff4080) and merged PR #7; the local view of origin/main was simply stale at a15d7dd. A merge commit had been prepared locally (ffc46ac, built via git commit-tree so the running build's working tree was never touched); git refused the push as non-fast-forward, the fetch revealed the real state, and the commit was discarded. Nothing was pushed to rvdbreemen/SimpleTelnet.

Verified before moving: git merge-tree showed a clean merge; git diff origin/main 123106f contains nothing unique to the pinned commit, so the move is purely additive.

Landed as cf44a7e96 under prerelease alpha.366 (bump required: src/libraries/** is a triggering path). CHANGELOG entry added under [Unreleased] -> Fixed.

Scope note beyond the original description: the move also adopts a909731, which is behavioural, not cosmetic. _flushTx(idx) before clearing _tx, and removal of the unconditional inbound-discard loop at accept/teardown. The latter affects the raw ser2net/OTmonitor bridge (ADR-143), where there is no telnet negotiation to strip and the loop could only destroy user data. A green build does not exercise either path.

AC4 met: build.bat green on all three targets (esp32, esp32-classic, esp32-combo), literal 'Build completed successfully!' present, artifacts at 2.0.0-alpha.366+cf44a7e. evaluate.py --quick exit 0, 0 failed, 1 warning, health 98.7%. Pushed to origin/dev (2c13e0460..cf44a7e96).

AC5 precision note: the AsyncSimpleTelnet instances are gone (27 -> 0). ONE AsyncClient::close(bool) warning remains in the build log, from .pio/libdeps/esp32/espMqttClient/src/Transport/ClientAsync.cpp:45 - a different upstream library, outside this task's scope. The AC was worded absolutely ('no longer appear in the build log') and is therefore not literally satisfied; the SimpleTelnet-scoped intent is. Recorded here rather than silently reinterpreted.

Open, deliberately not blocking this task: the two behavioural fixes (_flushTx before _tx clear; removal of the inbound-discard loop) are unexercised by a green build. They want a bench check against OTmonitor over the ser2net bridge (ADR-143).

Cross-reference from the 1.x line (TASK-1146, GH #685): the a909731 adopted here also closes a user-visible defect, it is not only the cosmetic warning cleanup.

Removing the unconditional inbound-discard loop is what makes port 25238 keep the first command a client sends. Before it, a client that pipelined a command with connect() lost that command, because the server only accepts on the next loop() pass and the drain ran on a buffer that already held it. Home Assistants opentherm_gw integration (pyotgw writes PS=0 about 20 ms after the handshake) failed with cannot_connect because of exactly this.

Measured on the 1.x bench unit before and after the equivalent change: zero-delay writes went from 5/8 answered to 8/8, and pyotgw 2.2.3 from failing to 3/3 connects. This tree has carried the fix since alpha.366; the 1.x line got a backport of the same commit today.

No AC change intended here, this is context only. ACs #4 and #5 still stand on their own terms.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Moved the SimpleTelnet submodule from the diverged 123106f onto upstream main (260aca5), converging a fork that had split the GPLv3 relicence away from the fixes.

The task premise turned out to be wrong in a useful way: no convergence work was needed. Upstream had already re-applied the relicence on main (51f7f34, bff4080) and merged PR #7; the local origin/main was stale at a15d7dd. A merge commit prepared locally was discarded after git refused the push as non-fast-forward and the fetch revealed the real state. Nothing was pushed to rvdbreemen/SimpleTelnet.

The move is purely additive - git diff origin/main 123106f holds nothing unique to the pinned commit - and brings three changes: the AsyncClient::close(bool) -> close() deprecation fix (behaviour-preserving; the overload forwards to close() and documents its parameter as ignored), and two behavioural fixes in the telnet/ser2net data path. _flushTx(idx) now runs before _tx is cleared on slot release, so bytes write() reported as accepted are no longer destroyed with our own ring ahead of lwIP. The unconditional inbound-discard loop at accept and teardown is removed: it could not distinguish an IAC sequence from payload, costing a pipelined command at connect() and, on the raw ser2net/OTmonitor bridge (ADR-143) where nothing needs stripping, destroying user data outright.

Shipped as cf44a7e96 under alpha.366 with a CHANGELOG entry. Build green on all three ESP32 targets, evaluate.py clean. SimpleTelnet deprecation warnings 27 -> 0; one unrelated instance remains in espMqttClient.

Not covered by this task: the two behavioural fixes are unexercised by a green build and want a bench check against OTmonitor.
<!-- SECTION:FINAL_SUMMARY:END -->
