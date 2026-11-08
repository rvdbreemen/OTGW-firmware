---
id: TASK-1742
title: 'settings-reboot-badge test: wait for the badge before reading it'
status: To Do
assignee: []
created_date: '2026-11-08T19:53:00Z'
labels:
  - webui
  - test
  - flaky
dependencies: []
priority: low
ordinal: 846000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tests/webui/settings-reboot-badge.test.mjs fails intermittently (about 4 in 42 runs) on Windows. Found while checking int-157 (coordinator, 2026-11-08). Bisect, 6 runs per commit, foreground, no build running: 2eaa4c304 0/6, e8b96e689 1/6, b5c19dbc3 0/6, 45814fd1e 0/6, 1f8b68492 2/6 (an OT-Direct test-only commit that touches no web UI), e404fb3cd 1/6, 78ddb64cb 0/6; earlier origin/dev 0/4. The failures are not a product regression: they are 'badge present: [null,null]' (the badge is read before it is rendered) and an ErrorEvent in the harness. The coordinator once saw the cursor assert ('auto' vs 'default'), probably the same race: the computed style is read before the badge's class/style is applied. Fix is test-side: wait (h.until) for the badge element and its final style before reading it. Separate from TASK-1739 (Chrome process tree leak).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The test waits for the rendered badge (element present with its final class) before it reads attributes and computed style
- [ ] #2 20 consecutive runs on Windows pass with the fix; the bisect numbers above are the before figure
<!-- AC:END -->
