---
id: TASK-723
title: 'fix(api): prevent beta.24 device-info 503 under heap pressure'
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-26 20:18'
updated_date: '2026-05-26 20:24'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Hardware capture log-issues/OTGW_1.6_beta_24.txt from firmware 1.6.0-beta.24+a16606a records six REST GET /api/v2/device/info => 503 responses plus cumulative WS_drops=18 and MQTT_drops=72 during the capture. TASK-701 is Done but did not define acceptance criteria for eliminating device-info failures. Diagnose and apply the smallest justified fix for the remaining endpoint failure, and assess whether the concurrent drop growth is causal or diagnostic load.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Document beta.24 reproduction evidence and root cause for /api/v2/device/info returning 503.
- [ ] #2 Apply a minimal fix so the identified device-info failure path no longer returns 503 under the reproduced heap-pressure condition.
- [ ] #3 Assess the observed MQTT/WebSocket drop growth and record whether additional remediation is required.
- [ ] #4 Run required validation including firmware plus filesystem artifact build in one validation pass and relevant evaluator/tests.
- [ ] #5 Record branch, coding agent, implementation notes, and final summary before completion.
<!-- AC:END -->
