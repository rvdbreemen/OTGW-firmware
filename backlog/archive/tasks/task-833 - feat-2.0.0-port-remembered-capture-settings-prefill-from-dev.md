---
id: TASK-833
title: 'feat-2.0.0: port remembered-capture-settings prefill from dev'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 08:58'
updated_date: '2026-06-06 08:59'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Forward-port the interactive-settings persistence/prefill from dev (capture-mqtt-debug.bat): save DeviceHost/BrokerHost/BrokerPort/Topic/Username to per-user JSON, pre-fill prompts with [default] next run, never persist password. dev script remains a clean superset, so port is a whole-file copy.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 2.0.0 script pre-fills prompts from saved settings; password never saved
- [x] #2 PowerShell payload parses clean; settings + browser + toggle/dump functions all present
<!-- AC:END -->
