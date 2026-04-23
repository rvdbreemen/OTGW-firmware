---
id: TASK-385
title: 'Fix: text fields render dark in light mode (1.4.2-beta)'
status: To Do
assignee: []
created_date: '2026-04-23 06:53'
updated_date: '2026-04-23 06:54'
labels:
  - bug
  - ui
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - '2026-04-23 02:09Z'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Discord #beta-testing (andrebrait, 2026-04-23 02:09Z): after upgrading from 1.3.10 to latest 1.4.2-beta, text fields render with dark appearance in LIGHT theme. Screenshot attached to Discord message. May be a side-effect of the recent cross-browser color-scheme hardening (commit 7a894f50) or the design-system fonts patch (commit 97b46807). Also possibly mobile-browser specific — need to know browser/OS.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Screenshot from andrebrait saved or inspected to see exact rendering
- [ ] #2 Root cause identified (CSS regression in light theme, color-scheme interaction, or mobile-specific)
- [ ] #3 Fix confirmed by andrebrait on 1.4.2-beta hardware
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: screenshot + browser/OS info from andrebrait. Possible regressions to check: ds-tokens.css (commit 97b46807), color-scheme: light (commit 7a894f50), or cross-theme input-changed contrast work (ae959676).
<!-- SECTION:NOTES:END -->
