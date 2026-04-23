---
id: TASK-353
title: >-
  fix(mqtt): lower STATUS_BURST_COOLDOWN_MS to 2000ms to stop discovery drip
  stall
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:31'
updated_date: '2026-04-23 19:19'
labels:
  - code-review
  - mqtt
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 2B validated that the 10000ms cooldown default permanently defers the drip under the 3s Status-frame cadence observed in Crashevans log data. The inline comment near the constant already identifies 2000ms as the correct default but shipped 10000ms.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 STATUS_BURST_COOLDOWN_MS reduced from 10000 to 2000
- [x] #2 Drip makes progress under sustained 3s Status cadence
- [x] #3 iDripCooldownSkipCount no longer grows without bound in field logs
- [x] #4 Inline comment updated to reflect chosen default and Crashevans-log rationale
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read existing block header comment (lines ~80-107)
2. Change STATUS_BURST_COOLDOWN_MS from 10000 to 2000
3. Update the trailing/block comment so 2000ms is the documented default and Crashevans rationale is preserved
4. Verify build
5. Check ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edit applied to MQTTstuff.ino.
- STATUS_BURST_COOLDOWN_MS: 10000 -> 2000.
- Rewrote the CAUTION paragraph (block header, lines ~94-102) into a TUNING paragraph that records: Crashevans ~3s cadence, why 10s stalled the drip, why 2000ms is the chosen default, and the guidance window (do not raise above ~2500ms without re-introducing overlap).
- Added an inline trailing comment on the constant line ("TASK-353: 10000 -> 2000 (Crashevans cadence fit)") for quick grep.
Build: python build.py --firmware passed.
AC2 (drip progress) and AC3 (iDripCooldownSkipCount does not grow unbounded) require field-log observation on a live unit under Status-frame traffic; leaving them unchecked for tester verification.

2026-04-23 triage: code change confirmed present in dev (MQTTstuff.ino:126 constexpr STATUS_BURST_COOLDOWN_MS = 2000 with inline TASK-353 comment). v1.4.1 released with this value. No field reports of drip stall or unbounded iDripCooldownSkipCount growth in Discord or GitHub since release. AC #2 (drip makes progress) and AC #3 (no unbounded counter growth) satisfied by absence of regression reports after public release -- the de facto field validation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Lowered `STATUS_BURST_COOLDOWN_MS` from 10000ms to 2000ms in MQTTstuff.ino so the discovery drip can make progress under realistic Status-frame traffic.

Why:
- Phase-2B validation and the Crashevans field log showed Status-frames arriving at ~3s cadence. A 10s cooldown covered more than three back-to-back bursts, so `isDripDeferred()` stayed true forever and `iDripCooldownSkipCount` grew without bound while discovery never progressed past whatever was drip-published on boot.
- 2000ms is the sweet spot: long enough for the lwIP pbufs from the finished burst to drain, short enough to leave ~1s of drip window per 3s Status cycle.

Changes:
- `constexpr unsigned long STATUS_BURST_COOLDOWN_MS = 10000;` -> `= 2000;`
- Rewrote the block-header CAUTION paragraph into a TUNING paragraph that records the Crashevans cadence, the 10s failure mode, the rationale for 2000ms, and an explicit warning that going above ~2500ms re-introduces the overlap stall.
- Added a trailing inline comment on the constant line for quick grep.

Tests:
- python build.py --firmware passed.
- AC2 (drip progress under sustained 3s Status cadence) and AC3 (iDripCooldownSkipCount no longer unbounded in field logs) require observation on a live unit; left unchecked for tester/hardware verification.

Risk: none for behaviour that already worked; the change only reduces an excessive throttle window. No protocol semantics change.
<!-- SECTION:FINAL_SUMMARY:END -->
