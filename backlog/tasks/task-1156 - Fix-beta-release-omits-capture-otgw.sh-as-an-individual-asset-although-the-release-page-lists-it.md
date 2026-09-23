---
id: TASK-1156
title: >-
  Fix: beta release omits capture-otgw.sh as an individual asset although the
  release page lists it
status: To Do
assignee: []
created_date: '2026-09-23 18:09'
labels:
  - bug
  - ci
dependencies: []
references:
  - .github/workflows/beta-prerelease.yml
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: medium
ordinal: 230000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The beta-prerelease workflow writes a RELEASE_ASSETS.md / release body whose "The individual files" table lists capture-otgw.sh (Linux, WSL, macOS capture), but neither gh release create nor the draft top-up gh release upload in .github/workflows/beta-prerelease.yml attaches scripts/capture-otgw.sh (or capture-settings.example.json). The script only ships inside the flash-bundle zip under capture/.

Field impact: mrfox7688 (GH #682, 2026-09-23) said he cannot run the capture script because he is on macOS. Verified on v1.7.6-beta.5: assets list has capture-mqtt-debug.bat and capture-usb-serial.bat, no capture-otgw.sh; the zip does contain capture/capture-otgw.sh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The next published beta lists capture-otgw.sh among its individual release assets (gh release view <tag> --json assets)
- [ ] #2 Every file named in the release-body asset table is attached to the release, checked against the create and the draft top-up upload lists
- [ ] #3 chmod/exec bit question for a directly downloaded .sh is handled or documented in the release body (macOS users download it without the zip)
<!-- AC:END -->
