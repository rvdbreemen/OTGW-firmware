---
id: TASK-1476
title: >-
  v2 capture mode: Stream-to-file probably never works on the plain-HTTP device
  page (showSaveFilePicker needs a secure context)
status: To Do
assignee: []
created_date: '2026-10-29T14:07:00Z'
labels:
  - webui
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 636000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1472 research: v2.js:3541/5752 feature-detects window.showSaveFilePicker; the File System Access API is exposed only in secure contexts, and the device serves plain HTTP (no HTTPS by project rule), so the button is likely always disabled with a misleading 'needs Chrome / Edge' tooltip. Not verified on a bench yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench check on http://<device>/ in Chrome: isSecureContext and typeof showSaveFilePicker recorded
- [ ] #2 If confirmed: Stream-to-file replaced by a mechanism that works over HTTP (e.g. the periodic auto-download already present), tooltip/text corrected; screenshot before/after
<!-- AC:END -->
