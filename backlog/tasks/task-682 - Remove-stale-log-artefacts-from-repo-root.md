---
id: TASK-682
title: Remove stale log artefacts from repo root
status: Done
assignee:
  - '@claude'
created_date: '2026-05-24 05:37'
updated_date: '2026-05-24 05:37'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two large log dump files were sitting in the repo root (OTGW serial capture and MQTT Explorer export). They are not source artefacts and bloat the working tree. Remove them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGW_1.6_beta_16.txt is deleted
- [x] #2 mqtt-01JXSX5BR2SZR76CNXTGQ0MJ06-OpenTherm Gateway (OTGW)-d7ae4d508faf5e84e2e33c79f872a380-4.json is deleted
- [x] #3 git status shows working tree clean after commit
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Delete the two stale log files at repo root\n2. Verify git status\n3. Commit and push
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed two stale log dump files from the repo root: the OTGW serial capture (OTGW_1.6_beta_16.txt, ~1.4 MB) and the MQTT Explorer export JSON (~747 KB). Neither was a source artefact.
<!-- SECTION:FINAL_SUMMARY:END -->
