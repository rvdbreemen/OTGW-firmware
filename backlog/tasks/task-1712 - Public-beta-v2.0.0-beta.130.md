---
id: TASK-1712
title: Public beta v2.0.0-beta.130
status: Done
assignee: []
created_date: '2026-11-07T21:34:00Z'
updated_date: '2026-11-07T22:35:00Z'
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
- [x] #1 release notes docs/releases/RELEASE_NOTES_2.0.0-beta.130.md, CHANGELOG rolled over, README links updated
- [x] #2 release commit bumps to 2.0.0-beta.130; evaluate --quick 0 failed
- [x] #3 esp32-combo build: 2 ESP32S3 images, Build completed successfully
- [x] #4 OTA bench check on the OTGW Classic + S3 Mini (.63): settings unchanged, new fwversion, PIC detected, no bootcount move and no crashlog after 5 min
- [x] #5 annotated tag, draft prerelease with assets and SHA256SUMS, published with --latest=false, downloaded zip matches SHA256SUMS
- [x] #6 #alpha-testing zip post and #beta-testing announcement
- [ ] #7 1 h soak before release
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Published v2.0.0-beta.130 (prerelease, --latest=false; Latest stays v1.7.9) on 2026-11-07, outside the 08:00/20:00 slot and without the 1 h soak: Robert's waiver ("Release beta now"), stated in the notes disclaimer and Known issues. AC1: docs/releases/RELEASE_NOTES_2.0.0-beta.130.md (362 lines, no em dash/TODO, no relative links), CHANGELOG rolled over to [2.0.0-beta.130] with the missing TASK-1686/1692/1709/1679/1667/1687 entries added, README links. AC2: release commit af8ebb561 (bump beta.129 -> beta.130, 55 files), evaluate --quick 0 failed. AC3: build.bat esp32-combo exit 0, artefacts beta.130+af8ebb5 22:56-22:58; build.bat writes no log, so the 'Successfully created ESP32S3 image' count could not be taken; instead the .ino.bin holds the version string and the flash zip holds all 14 files (capture tools, new .bat without PowerShell, Safari support). AC4: .63 (Classic + S3 Mini, PIC 16F1847 fw 6.8): ota_flash app+fs OK, ETag af8ebb5-/v2.html.gz; settings diff = exactly the 13 OTD keys TASK-1684 removes, nothing else; after 5 min bootcount 161 -> 163, no crashlog, MQTT connected, min free heap 24316 (beta.118: 24132). No boiler/thermostat on this bench, so OT traffic is not exercised. AC5: tag v2.0.0-beta.130 -> af8ebb561, 7 assets with sizes equal to build/, published; downloaded flash zip sha256 ab5cb830... equals SHA256SUMS. AC6: #alpha-testing 1568748840058884120 (ledger row added), #beta-testing 1568755560587989032 (Robert approved the text, read back intact). AC7 (1 h soak) waived by Robert, left unchecked; substitute: beta.129 (same firmware) ran 1 h 21 min on .61 (OT-Direct) with MQTT, no restart, no crashlog, min heap 18384. Follow-up: watch the field for TASK-1701 under a real heat demand and the heap/connection-gap known issues.
<!-- SECTION:FINAL_SUMMARY:END -->
