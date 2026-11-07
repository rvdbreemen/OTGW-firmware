---
id: TASK-1712
title: Public beta v2.0.0-beta.130
status: In Progress
assignee: []
created_date: '2026-11-07T21:34:00Z'
labels:
  - release
  - beta
dependencies: []
ordinal: 823000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 22:3x: "Release beta now", outside the 08:00/20:00 slot and before a 1 h soak (waiver, stated in the notes). Candidate: origin/dev 406fd569a (beta.129 firmware, TASK-1702 54757840a, plus tests/backlog). Robert accepted the two breaking changes (TASK-1684, TASK-1695): "Ja akkoord."
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 release notes docs/releases/RELEASE_NOTES_2.0.0-beta.130.md, CHANGELOG rolled over, README links updated
- [ ] #2 release commit bumps to 2.0.0-beta.130; evaluate --quick 0 failed
- [ ] #3 esp32-combo build: 2 ESP32S3 images, Build completed successfully
- [ ] #4 OTA bench check on the OTGW Classic + S3 Mini (.63): settings unchanged, new fwversion, PIC detected, no bootcount move and no crashlog after 5 min
- [ ] #5 annotated tag, draft prerelease with assets and SHA256SUMS, published with --latest=false, downloaded zip matches SHA256SUMS
- [ ] #6 #alpha-testing zip post and #beta-testing announcement
- [ ] #7 1 h soak before release
<!-- AC:END -->
