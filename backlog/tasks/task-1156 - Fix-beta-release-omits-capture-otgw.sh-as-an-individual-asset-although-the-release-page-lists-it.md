---
id: TASK-1156
title: >-
  Fix: beta release omits capture-otgw.sh as an individual asset although the
  release page lists it
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-23 18:09'
updated_date: '2026-09-23 18:54'
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
- [x] #2 Every file named in the release-body asset table is attached to the release, checked against the create and the draft top-up upload lists
- [x] #3 chmod/exec bit question for a directly downloaded .sh is handled or documented in the release body (macOS users download it without the zip)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Both upload paths (gh release create and the draft top-up gh release upload) now attach scripts/capture-otgw.sh. Every file in the release-body asset table appears in both lists (checked by grep count: 2 each).
- Release text (RELEASE_ASSETS.md and release_body.md) names the shell capture and says to run it as `bash capture-otgw.sh --host ...`, so a direct download needs no chmod +x. Checked: `bash scripts/capture-otgw.sh --help` runs without the exec bit.
- capture-settings.example.json stays bundle-only: the table does not list it, and only the Windows capture reads it.
- Workflow YAML parses (yaml.safe_load).
- mrfox7688 was given a direct link to the tag copy on GH #682 (comment 5800959707).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The beta release page listed capture-otgw.sh as a separate asset, but the workflow never attached it. The file only shipped inside the flash-bundle zip, so a macOS tester on GH #682 concluded there was no capture option for macOS.

Changes (.github/workflows/beta-prerelease.yml, commit 3b22de4d7):
- scripts/capture-otgw.sh is added to both the create and the draft top-up upload lists.
- The release text names the macOS/Linux capture and gives it as `bash capture-otgw.sh --host <gateway>`.

Verified: the YAML parses, every asset in the table appears in both upload paths, and the script runs through bash without the exec bit.

Open: AC #1 can only be checked once the next beta is published (gh release view <tag> --json assets). Left In Progress for that reason. No beta will be cut just to check it.
<!-- SECTION:FINAL_SUMMARY:END -->
