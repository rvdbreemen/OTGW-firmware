---
id: TASK-1745
title: Public beta v2.0.0-beta.158 (08:00 slot, over beta.145-158)
status: To Do
assignee: []
created_date: '2026-11-09T05:43:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 849000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
08:00 public beta per Robert's 12-hour slot rule (/beta-release-2x). Candidate beta.158 (8a4b25d60): 6 h 21 min on bench .61 (OT-Direct, no PSRAM) without restart, MQTT connected, no crashlog. Release commit is docs-only on origin/dev 94406bafc (src identical to 8a4b25d60). Bundles TASK-1719, 1717, 1730, 1693, 1716, 1689, 1731, 1732, 1735, 1737, 1736, 1738, 1741 (R1), 1734.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes, CHANGELOG rollover and README at the tagged commit
- [ ] #2 Build esp32-combo from the tagged commit, SUCCESS lines and fresh artifacts
- [ ] #3 Bench evidence: candidate src identical and running without restart on the bench unit
- [ ] #4 GitHub prerelease published with --latest=false, assets and SHA256 match
- [ ] #5 #alpha-testing zip post and one #beta-testing announcement
<!-- AC:END -->
