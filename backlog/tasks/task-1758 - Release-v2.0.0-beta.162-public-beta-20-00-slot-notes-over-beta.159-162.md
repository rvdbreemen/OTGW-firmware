---
id: TASK-1758
title: 'Release: v2.0.0-beta.162 public beta (20:00 slot, notes over beta.159-162)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T17:07:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 858000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta via /beta-release-2x, coordinator assignment 2026-11-09. Candidate beta.162 (5786928c8), release commit docs-only on origin/dev (src identical). Bundle: TASK-1743, 1741, 1733/ADR-223, 1747; tests 1742, 1739, 1736, 1738; docs 1755.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes, CHANGELOG rollover and README link in one docs-only commit on origin/dev; lint clean (no em dash, no relative link)
- [ ] #2 Candidate soak: beta.162 on the bench unit at least 1 h with MQTT, no restart, no crashlog (or a recorded decision)
- [ ] #3 esp32-combo build from the release commit: 2x 'Successfully created ESP32S3 image', fresh artifacts with the release githash
- [ ] #4 Tag v2.0.0-beta.162 on the release commit; GitHub prerelease with --latest=false; asset sizes equal build/, SHA256 of the downloaded zip equals SHA256SUMS; latest stays v1.7.9
- [ ] #5 #alpha-testing zip post and #beta-testing text handed to the comms agent
<!-- AC:END -->
