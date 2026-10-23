---
id: TASK-1317
title: 'Build-zip poster: keep the version readable in the Discord attachment name'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T12:30:00Z'
updated_date: '2026-10-23T12:31:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 440000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Discord strips '+' from attachment names: OTGW-firmware-esp32-combo-2.0.0-alpha.465+5cf2758-flash.zip arrived as ...alpha.4655cf2758-flash.zip (message 1563164311466283070), which reads as alpha.4655. Upload under a name with '_' instead of '+' (curl -F filename=); the local zip keeps its name.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The upload name replaces + with _ (dry run prints it)
- [ ] #2 One real post shows the attachment as ...alpha.N_<hash>-flash.zip
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
One change in scripts/discord-post-build.sh: upload_name = zip name with + -> _, passed as curl -F ...;filename=; dry run prints it. Evidence comes with the next real post (TASK-1312 build).
<!-- SECTION:PLAN:END -->
