---
id: TASK-1644
title: >-
  Setting to switch the telnet debug console (port 23) off, default on (GitHub
  #154)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T04:09:00Z'
updated_date: '2026-11-05T04:11:00Z'
labels:
  - github
  - security
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 769000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/154 point 4. Robert 2026-11-05: a setting, default ON. Today debugTelnet.begin(false) runs unconditionally (networkStuff.ino:640) and the console has no login while keys u/i drive the output GPIO (handleDebug.ino:425-434).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Persisted bool setting (ADR-051 naming), default true, in GET/POST /api/v2/settings, v2 Settings UI and settingsHelp.h
- [ ] #2 Off: TCP connect to port 23 refused without reboot (apply-live like applyLegacyPort25238Setting); On: accepted again; Debug macros safe with no telnet server
- [ ] #3 Host test red on old code / green on fix, evaluate --quick green, bench: connect refused/accepted on a bench unit
- [ ] #4 docs/api settings docs and ADR-032 consequence note updated
<!-- AC:END -->
