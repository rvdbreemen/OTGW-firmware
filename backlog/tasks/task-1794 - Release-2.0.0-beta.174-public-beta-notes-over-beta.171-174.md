---
id: TASK-1794
title: 'Release: 2.0.0-beta.174 public beta notes over beta.171-174'
status: To Do
assignee: []
created_date: '2026-11-11T05:17:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 889000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta in the 08:00 slot of 2026-11-11 via /beta-release-2x (invoked by Robert). Candidate beta.174 (bceb92c86), soaked on .63 (Classic + S3 Mini) since 00:28 with MQTT connected since 03:59. Docs-only release commit on top (beta.170 model, no bump).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release notes + CHANGELOG + README committed and checked (no TODO/em dash/relative links)
- [ ] #2 Release commit built (2 SUCCESS lines, fresh artifacts) and OTA bench-checked on .63: fwversion, settings unchanged, bootcount stable >= 5 min, no crashlog
- [ ] #3 Tag v2.0.0-beta.174 pushed on the release commit; GitHub prerelease published with --latest=false; latest stays v1.7.9; download SHA matches
- [ ] #4 #alpha-testing zip post and #beta-testing announcement sent and read back
<!-- AC:END -->
