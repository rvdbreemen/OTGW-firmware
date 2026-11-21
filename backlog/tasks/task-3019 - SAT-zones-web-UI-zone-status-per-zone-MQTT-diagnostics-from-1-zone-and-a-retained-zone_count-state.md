---
id: TASK-3019
title: >-
  SAT zones: web UI zone status, per-zone MQTT diagnostics from 1 zone, and a
  retained zone_count state
status: To Do
assignee: []
created_date: '2026-11-21T17:16:00Z'
labels:
  - sat
  - mqtt
  - webui
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1134000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573748508622721095 (2026-11-21), three requests: (1) the web UI shows no zone status; add temperature, target and active per zone. (2) sat/zone/<n>/active|output|error are only published when zone_count > 1, so a single zone cannot be checked; publish them from 1 zone. Verified at origin/dev: satPublishZoneDiagnostics() is called only under 'if (settings.sat.iZoneCount > 1)' (SATcontrol.ino:8330; also :6602). (3) zone_count has a set command (MQTTstuff.ino:775, SATzonecount) but no retained state topic, so clients cannot read it back. Tester request: needs Robert's OK. Fits the TASK-3014 naming work (areas vs zones).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides which of the three go in
- [ ] #2 Per the decision: zone diagnostics publish from zone_count >= 1; zone_count state topic (retained or heartbeat per ADR-111); web UI zone status rows; host test on the real publisher red before / green after; screenshots for the UI
<!-- AC:END -->
