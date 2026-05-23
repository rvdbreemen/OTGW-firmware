---
id: TASK-676
title: 'feat-2.0.0: port TASK-674 — Tier-2 mainloop sync-blocker dispositions'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-23 10:55'
updated_date: '2026-05-23 11:06'
labels:
  - mainloop
  - responsiveness
  - research
dependencies: []
priority: low
ordinal: 54000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of TASK-674 (dev). Same three sync-blockers, per-item disposition adjusted for 2.0.0 divergence.

Item 5 (webhook.ino:222): same as dev — drop http.setTimeout from 1000ms to 500ms; replace history-only comment with rationale-only.

Item 6 (MQTTstuff.ino:1028): no code change. 2.0.0 already runs MQTTclient.setSocketTimeout(5) with an in-code comment that explains the HTTP/WS-responsiveness trade-off. Author a Proposed 2.0.0 ADR-108 (sibling of dev ADR-080) capturing the 5s envelope as the accepted sync-blocker on this branch — divergence from dev's 15s is intentional and documented.

Item 7 (sensors_ext.ino:260): no code change. setWaitForConversion(false) already absorbs 750ms; the ~10ms/sensor is OneWire-protocol-inherent.

Master plan / scope follows dev TASK-674.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Item 5: http.setTimeout reduced from 1000 to 500 on 2.0.0; comment rewritten to explain the why, not the history
- [x] #2 Item 6: Proposed ADR-108 authored capturing the 5s socketTimeout as an accepted sync-blocker; no code change to the value
- [x] #3 Item 6: MQTTstuff.ino:1028 retains the 5s value; comment unchanged or only minimally annotated to cross-reference ADR-108
- [x] #4 Item 7: no code change; documented in Final Summary as not-a-finding
- [x] #5 python build.py --firmware exits 0 on 2.0.0
- [x] #6 python evaluate.py --quick reports no new failures on 2.0.0
- [x] #7 Sibling cross-references: TASK-676 links TASK-674; ADR-108 links ADR-080; commit message references both task IDs
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
2.0.0 sibling of dev TASK-674. Tier-2 sync-blocker dispositions, branch-local where 2.0.0 has already diverged.

**Item 5 — `http.setTimeout` 1000→500 ms.** Identical patch to dev. Same retry-budget reasoning (`WH_PENDING → WH_RETRY_WAIT`).

**Item 6 — `MQTTclient.setSocketTimeout(5)`.** No code change. The in-code comment at `MQTTstuff.ino:1023-1027` already explains the responsiveness trade-off; this task adds one cross-reference line to ADR-108. Authored ADR-108 (Proposed) capturing the 5 s envelope as the accepted sync-blocker on this branch with explicit rationale for the divergence from dev's 15 s. Includes an Enforcement block that guards the literal `5` on 2.0.0.

**Item 7 — `sensors.getTempC()`.** Same disposition as dev: not-a-finding. `setWaitForConversion(false)` already absorbs the 750 ms; the residual ~10 ms/sensor is OneWire bus-physics.

**Tests**
- `python build.py --firmware --target esp8266`: exit 0, binary 0.82 MB (no size regression). ESP32 target skipped in the remote sandbox — the toolchain download is blocked by sandbox SSL cert pinning, not a code issue. Reproduce locally to verify the ESP32 build.
- `python evaluate.py --quick`: 60/68 pass, 0 fail. The single warning (`mqtt_configuratie.cpp not found`) is pre-existing branch-local layout drift, not introduced here.

**Open items**
- ADR-108 stays Proposed pending user review.
- Hardware/beta validation on 2.0.0 — confirm the 5 s socketTimeout remains adequate on slow brokers; if regressions appear, ADR-108 is the supersession anchor.
- The commit on this branch is unsigned (sandbox local-signer rejects worktree-relative paths with `missing source`). User authorised `--no-gpg-sign` for the single commit.

Refs TASK-674 (dev sibling), ADR-080 (Accepted on dev), ADR-108 (Proposed here), PR #635, PR #636.
<!-- SECTION:FINAL_SUMMARY:END -->
