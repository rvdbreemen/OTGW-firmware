---
id: TASK-676
title: 'feat-2.0.0: port TASK-674 — Tier-2 mainloop sync-blocker dispositions'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-23 10:55'
updated_date: '2026-05-23 10:55'
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
- [ ] #1 Item 5: http.setTimeout reduced from 1000 to 500 on 2.0.0; comment rewritten to explain the why, not the history
- [ ] #2 Item 6: Proposed ADR-108 authored capturing the 5s socketTimeout as an accepted sync-blocker; no code change to the value
- [ ] #3 Item 6: MQTTstuff.ino:1028 retains the 5s value; comment unchanged or only minimally annotated to cross-reference ADR-108
- [ ] #4 Item 7: no code change; documented in Final Summary as not-a-finding
- [ ] #5 python build.py --firmware exits 0 on 2.0.0
- [ ] #6 python evaluate.py --quick reports no new failures on 2.0.0
- [ ] #7 Sibling cross-references: TASK-676 links TASK-674; ADR-108 links ADR-080; commit message references both task IDs
<!-- AC:END -->
