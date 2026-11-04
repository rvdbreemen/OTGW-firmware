---
id: TASK-1609
title: 'Connection map flow dots loop seamlessly (PR #695, Andre Brait)'
status: To Do
assignee: []
created_date: '2026-11-04T17:57:00Z'
updated_date: '2026-11-04T18:02:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 749000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Contributor PR #695 by Andre Brait: .cn-flow reused dashMove (-19, sized for .dash 4 15) while its dasharray period is 13, so the dots jumped back every cycle. Own cnFlowMove keyframes shift exactly one period (-13); v2-bundle.css regenerated. Side effect: dots move 13 px/s instead of 19. Taken over with the author kept, without the PR's beta.71 bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 cn-flow uses its own keyframes with a -13 offset; .dash keeps dashMove
- [ ] #2 v2-bundle.css regenerated from v2.css and served by the device
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Host evidence (2026-11-04, lane-prs afa1ce3ae): build.generate_v2_css_bundle regenerated v2-bundle.css from v2.css with no diff to the committed bundle (76403 B); cnFlowMove present in v2.css and the bundle. Contributor tested the served file on an ESP32-S3. Not yet: our filesystem build and a browser check of the animation (after beta.77).
<!-- SECTION:NOTES:END -->
