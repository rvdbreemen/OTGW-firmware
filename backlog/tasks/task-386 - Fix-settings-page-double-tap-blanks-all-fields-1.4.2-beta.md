---
id: TASK-386
title: 'Fix: settings page double-tap blanks all fields (1.4.2-beta)'
status: To Do
assignee: []
created_date: '2026-04-23 06:54'
updated_date: '2026-04-23 06:54'
labels:
  - bug
  - ui
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - '2026-04-23 02:11Z'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Discord #beta-testing (andrebrait, 2026-04-23 02:11Z): on 1.4.2-beta, tapping 'Settings' twice in quick succession clears all field values in the UI. Screenshot attached to Discord message. Likely a JavaScript race condition in the settings-tab click handler: the second tap fires while the first load is still in-flight, possibly re-rendering the template before the async fetch returns values. Mobile double-tap may trigger this more easily than desktop double-click.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Screenshot from andrebrait inspected to confirm what 'blank' looks like (empty values, missing rows, or loading state)
- [ ] #2 Root cause identified in the settings page JS handler
- [ ] #3 Fix: handler guards against concurrent invocations, or re-renders only after fetch completes
- [ ] #4 Verified by andrebrait on a mobile browser
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: screenshot + browser info. Suspect: async fetch race in data/index.js settings-tab handler.
<!-- SECTION:NOTES:END -->
