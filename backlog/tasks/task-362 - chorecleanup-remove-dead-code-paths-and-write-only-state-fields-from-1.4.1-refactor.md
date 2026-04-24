---
id: TASK-362
title: >-
  chore(cleanup): remove dead code paths and write-only state fields from 1.4.1
  refactor
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:35'
updated_date: '2026-04-21 17:31'
labels:
  - code-review
  - cleanup
  - refactor
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 1A dead-code analysis identified 14 items introduced or left behind by this branch. Bulk cleanup PR, non-functional changes only. Post-merge is fine; does not block 1.4.1 shipping.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 state.discovery.bVerificationActive removed (written 3x, never read; isDiscoveryVerificationActive is single source of truth)
- [x] #2 state.heapdiag.iLastPublishedEpoch removed OR consumed in devinfoV2 JSON
- [x] #3 endDiscoveryVerification and tickDiscoveryVerification demoted to static inside MQTTstuff.ino
- [x] #4 isDripDeferred demoted to static (single in-TU caller)
- [x] #5 (void)yearFlag cast removed at OTGW-firmware.ino:324
- [x] #6 Per-line // ADR-086: single caller comments collapsed into block header
- [x] #7 HEAP_FRAG_PROMOTE_MAXBLOCK changed from #define to constexpr
- [x] #8 DiscoverySection 3 bytes padding comment removed (not load-bearing per ADR-051)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Remove state.discovery.bVerificationActive from DiscoverySection and its 3 writes in MQTTstuff.ino.
2. Remove state.heapdiag.iLastPublishedEpoch from HeapDiagSection and its single write in sendMQTTheapdiag.
3. Demote endDiscoveryVerification + tickDiscoveryVerification to static in MQTTstuff.ino; remove forward decls from OTGW-firmware.h.
4. Demote isDripDeferred to static; remove header decl.
5. Remove (void)yearFlag dead cast at OTGW-firmware.ino:324.
6. Collapse per-line // ADR-086: single caller comments at lines 295/297/299/407 into block-header reference.
7. SKIP - HEAP_FRAG_PROMOTE_MAXBLOCK is in helperStuff.ino (out of scope); document in final summary.
8. Remove "3 bytes padding" comment from DiscoverySection in OTGW-firmware.h (trailing bVerificationActive removal also drops the padding line).
Run full build at the end.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Seven of eight ACs satisfied in this pass.
- state.discovery.bVerificationActive: removed from DiscoverySection; three writes eliminated (startDiscoveryVerification / endDiscoveryVerification / tickDiscoveryVerification). isDiscoveryVerificationActive() reading the static-local verifyActive flag is the single source of truth.
- state.heapdiag.iLastPublishedEpoch: removed from HeapDiagSection; single write in sendMQTTheapdiag eliminated. No readers existed.
- endDiscoveryVerification + tickDiscoveryVerification: demoted to static in MQTTstuff.ino. Forward decls removed from OTGW-firmware.h (kept start/isActive since they have external callers in OTGW-firmware.ino / restAPI.ino / handleDebug.ino).
- isDripDeferred: demoted to static; header decl removed.
- (void)yearFlag dead cast at OTGW-firmware.ino:324: removed; replaced silencer comment with a one-line clarifier about SR=22 via sendtimecommand being the sole current yearly consumer.
- Four // ADR-086: single caller repeats collapsed into one block header above the three helper calls; line 407 repeat folded into the trailing comment on the same line.
- DiscoverySection padding comment removed (trailing bool removed so alignment note no longer meaningful; ADR-051 says struct is not flash-serialised anyway).

AC #7 (HEAP_FRAG_PROMOTE_MAXBLOCK #define -> constexpr) not done: helperStuff.ino is outside this agent's file ownership. Recommend a follow-up with the appropriate agent or include in a later hygiene pass. No functional impact.

Build: python build.py --firmware passed (1.4.1-beta+deaddd8).

Follow-up to the earlier TASK-362 completion: AC#7 closed after Agent X could not edit helperStuff.ino (outside its file ownership). Changed #define HEAP_FRAG_PROMOTE_MAXBLOCK 1536 to constexpr uint32_t HEAP_FRAG_PROMOTE_MAXBLOCK = 1536 at helperStuff.ino:732. Build passes. All 8 ACs now checked.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Dead-code and write-only-state cleanup from the 1.4.1 refactor. Non-functional; all paths keep the same behavior with less surface.

Changes:
- Removed state.discovery.bVerificationActive: the mirror bool was written in 3 places and never read. Single source of truth is now isDiscoveryVerificationActive() reading the MQTTstuff.ino static-local verifyActive flag. Removed the trailing padding comment at the same time (struct is not flash-serialised per ADR-051 so padding is invisible).
- Removed state.heapdiag.iLastPublishedEpoch: written once in sendMQTTheapdiag, never read anywhere.
- Demoted endDiscoveryVerification() and tickDiscoveryVerification() to static in MQTTstuff.ino; removed their forward decls from OTGW-firmware.h. startDiscoveryVerification() and isDiscoveryVerificationActive() stay visible — they have external callers in OTGW-firmware.ino, restAPI.ino, and handleDebug.ino.
- Demoted isDripDeferred() to static; removed header decl (single caller in loopMQTTDiscovery, same TU).
- Removed (void)yearFlag dead cast at OTGW-firmware.ino:324. yearFlag is already consumed by sendtimecommand(dayFlag, yearFlag) two lines earlier, so the cast was a no-op silencer. Replaced with a one-line clarifier about SR=22 being the current sole yearly consumer.
- Collapsed four repeated // ADR-086: single caller trailing comments in OTGW-firmware.ino (lines 295/297/299/407) into a single block header above the hourChanged/dayChanged/yearChanged triad plus a compact inline comment on the minuteChanged() dispatcher call. The CI rule in evaluate.py::check_time_boundary_single_caller is the real enforcement.
- Removed the "3 bytes padding" comment from DiscoverySection (happened as part of AC #1 since the bool was the last field).

Not in scope (see below):
- AC #7 (HEAP_FRAG_PROMOTE_MAXBLOCK #define -> constexpr) is in helperStuff.ino, outside this agent's file ownership. Needs a follow-up with the owner of that file, or a later hygiene pass. Purely cosmetic; no functional impact.

Risk: minimal. No runtime behavior change; only visibility reductions and removal of dead writes. endDiscoveryVerification / tickDiscoveryVerification were only called from MQTTstuff.ino internally, so static is strictly equivalent for the linker.

Build: python build.py --firmware passed (1.4.1-beta+deaddd8).
<!-- SECTION:FINAL_SUMMARY:END -->
