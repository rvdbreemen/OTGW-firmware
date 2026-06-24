---
id: TASK-929
title: 'docs: release 1.7.0 documentation (since v1.6.1)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 22:19'
updated_date: '2026-06-24 22:31'
labels:
  - docs
  - update-docs
  - release
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release 1.7.0 docs pass. PREV=v1.6.1. Themes: heap/crash hardening (TASK-837/841/842/843/852/901), gateway-override surfacing (TASK-805/ADR-082), WiFi-quality banner (TASK-834), headless WiFi provisioning (TASK-900), ~25 perf(ram) commits. Breaking: none (buffer shrinks have verified headroom).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 API docs updated for gateway-override REST/MQTT surface (openapi.yaml, README.md, MQTT.md)
- [x] #2 Contributors gathered (gh PRs + Discord #beta-testing + #devs-esp-firmware)
- [x] #3 RELEASE_NOTES_1.7.0.md generated
- [x] #4 RELEASE_GITHUB_1.7.0.md generated
- [x] #5 docs/BREAKING_CHANGES.md updated (none)
- [x] #6 README What's New + CHANGELOG.md updated
- [x] #7 Docs folder cleanup (archive old releases, move misplaced)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Release 1.7.0 docs: full RELEASE_NOTES + RELEASE_GITHUB, CHANGELOG [1.7.0], BREAKING_CHANGES (none), README What's New. API docs updated for gateway-override endpoint/topics (ADR-082), ID-27 stat_t retarget, 3 fragskip stat topics, plus sensors/boiler-support/ot-support gap closure. Contributors: GeorgeZ83, Richard_HA, crashevans, Ties, Remco, Mattie, Andre Brait. Old release notes archived to docs/releases/.
<!-- SECTION:FINAL_SUMMARY:END -->
