---
id: TASK-834
title: 'feat-2.0.0: port headless-Edge stderr silence from dev'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 09:10'
updated_date: '2026-06-06 09:11'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Forward-port the Chromium logging-suppression launch flags (--disable-logging, --log-level=3, --disable-features=msSmartScreenProtection) from dev so headless Edge stderr ERROR spam no longer mixes with capture console output. Whole-file copy of the dev superset.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 2.0.0 browser capture launches Edge with the logging-suppression flags; browser.log unaffected
- [x] #2 PowerShell payload parses clean
<!-- AC:END -->
