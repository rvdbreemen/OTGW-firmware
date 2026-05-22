---
id: TASK-653
title: Comprehensive multi-perspective review of dev branch (50 commits since main)
status: To Do
assignee: []
created_date: '2026-05-22 05:37'
labels:
  - review
  - dev-branch
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Broad review of origin/main..origin/dev (50 commits, ~17k+/-23k LOC) covering all relevant perspectives: code correctness, security, ESP8266 platform compliance (PROGMEM/RAM/heap/yield), ADR-compliance, frontend/UI, release-/CI-pipeline, docs/guides, and backlog hygiene. Findings to be consolidated into chat summary plus this backlog task; follow-up fixes become separate tasks.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Code-correctness review completed and findings documented
- [ ] #2 Security review completed and findings documented
- [ ] #3 ESP8266-platform review (PROGMEM/RAM/heap/yield/static-buffer) completed and findings documented
- [ ] #4 ADR-compliance check against all Accepted ADRs touching changed code completed
- [ ] #5 Frontend/UI review (FSexplorer touch fix, HA discovery JS, data/*) completed
- [ ] #6 Release- and CI-pipeline review (beta-prerelease workflow, evaluate.py, build.py submodule fix, versioning policy) completed
- [ ] #7 Docs/integration-guides review (openHAB, Domoticz, Dutch MQTT cleanup, daily issue reports) completed
- [ ] #8 Backlog hygiene review completed (every dev commit traceable to a TASK; completed tasks have Final Summary; no orphan In Progress)
- [ ] #9 Findings consolidated in Final Summary with severity + location + suggested follow-up tasks
- [ ] #10 Follow-up backlog tasks created for each actionable High/Medium finding
<!-- AC:END -->
