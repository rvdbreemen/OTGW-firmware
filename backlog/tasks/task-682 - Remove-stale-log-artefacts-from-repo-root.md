---
id: TASK-682
title: Remove stale log artefacts from repo root
status: To Do
assignee: []
created_date: '2026-05-24 05:37'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two large log dump files were sitting in the repo root (OTGW serial capture and MQTT Explorer export). They are not source artefacts and bloat the working tree. Remove them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW_1.6_beta_16.txt is deleted
- [ ] #2 mqtt-01JXSX5BR2SZR76CNXTGQ0MJ06-OpenTherm Gateway (OTGW)-d7ae4d508faf5e84e2e33c79f872a380-4.json is deleted
- [ ] #3 git status shows working tree clean after commit
<!-- AC:END -->
