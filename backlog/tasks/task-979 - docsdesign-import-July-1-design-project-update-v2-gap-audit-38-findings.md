---
id: TASK-979
title: 'docs(design): import July-1 design-project update + v2 gap audit (38 findings)'
status: Done
assignee:
  - '@claude'
created_date: '2026-07-01 21:58'
updated_date: '2026-07-01 22:00'
labels: []
dependencies: []
ordinal: 191000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The claude.ai/design project 'Mockups into design system' (eb26ec05) updated the ground-truth prototype docs/design/boiler-dashboard-concepts.html (+530/-45 vs the 2026-06-26 import TASK-933 aligned against). Imported the updated mockup into the repo and ran a 7-area adversarially-verified audit (workflow wf_9d378fc8-1c6, 14 agents) of the current v2 UI against it. Output: docs/audits/2026-07-01-v2-design-update-audit.md — 38 findings (37 confirmed, 1 corrected). Implementation follows as separate tasks after maintainer plan approval.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Updated mockup imported at docs/design/boiler-dashboard-concepts.html
- [x] #2 Audit doc with file:line evidence + fix directions committed under docs/audits/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Imported the July-1 update of the design ground truth (boiler-dashboard-concepts.html, +530/-45) from the claude.ai/design project and produced an adversarially-verified 38-finding gap audit of the live v2 UI against it (docs/audits/2026-07-01-v2-design-update-audit.md). All gaps are UI-side; firmware already exposes the needed data (device/info, health, otgw/overrides, pic/*, filesystem/files, webhook/test). Implementation tasks follow after maintainer approval of the plan.
<!-- SECTION:FINAL_SUMMARY:END -->
