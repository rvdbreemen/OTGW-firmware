---
id: TASK-325
title: '[A11Y-M2] Add aria-labels to OT-Direct override action/msgid/value inputs'
status: To Do
assignee: []
created_date: '2026-04-18 19:27'
labels:
  - accessibility
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
index.html:118,:126,:127 use placeholder-only labeling. WCAG 1.3.1 and 3.3.2 fail. Placeholders are not labels for screen readers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 action <select>, msgid <input>, value <input> each have a descriptive aria-label
- [ ] #2 Or visible <label for=> equivalent added
- [ ] #3 Placeholder attributes remain as hints but no longer carry the label
<!-- AC:END -->
