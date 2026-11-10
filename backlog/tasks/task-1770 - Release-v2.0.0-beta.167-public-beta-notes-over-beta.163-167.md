---
id: TASK-1770
title: Release v2.0.0-beta.167 public beta (notes over beta.163-167)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T03:09:00Z'
updated_date: '2026-11-10T03:22:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 867000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta of beta.167 (e79e49522) at ~04:15-05:00 on 2026-11-10 (12 h rule waived by Robert for this one, long overnight soak). Bundles TASK-1749, 1752, 1762, 1764, 1768 since beta.162. Run via /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit with notes, CHANGELOG rollover, README links; lint clean (no em dash/TODO/relative link/board name)
- [x] #2 Soak of both bench units read and recorded (bootcount, crashlog, MQTT, heap)
- [x] #3 esp32-combo build from the release commit with both SUCCESS lines, under the commit-charge limit
- [x] #4 Tag pushed, prerelease published with --latest=false, assets equal build/, download SHA matches
- [x] #5 #beta-testing text to 97 and posted; #alpha-testing covered by 80's zip post
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 04:0x-04:3x. AC1: commit ddf849818 on e79e49522 (notes via mk167.py from the beta.162 notes; CHANGELOG [2.0.0-beta.167]; README 3 links); lint: no em dash/TODO/relative link/board name; evaluate --quick 86 pass / 0 fail. AC2: soak read 04:1x: .61 2.0.0-beta.167+e79e495 uptime 3:35 bootcount 201 (OTA 199+2) MQTT on heap 71.3-71.6 KB, crashlog only the old 12:02 beta.162 panic (TASK-1763); .63 2.0.0-beta.166+c701f6b (C code identical to 167, UI only) uptime 4:53 bootcount 178 MQTT on heap 55.0 KB, crashlog empty; .63 ping 10 % loss max 3.5 s (HTTP 200 in 460-720 ms), coordinator files it as a task. Gates before: screenshots of 4 languages (Home/Settings/Monitor) checked, SAT page on 167 seen on 80's screenshot, 80's webui suite green except the TASK-1766 flake, src diff 166->167 version lines only. AC3: commit charge 73.1 % before the build; build.bat 04:19: 2x 'Successfully created ESP32S3 image', 'Build completed successfully'. AC4: tag v2.0.0-beta.167 -> ddf849818; prerelease published --latest=false, Latest stays v1.7.9; 7 assets equal build/; flash zip holds capture and flash tools; download sha256 4e5e4ba4...359c equals SHA256SUMS. Safeboot asset is the cached safeboot build (sha a4b46042..., same as beta.162). URL https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.167. AC5 pending: #beta-testing text (1584 chars) sent to 97; #alpha-testing covered by 80's zip post 1569496816255377473 and 97's note 1569496962171015303.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: release commit ddf849818 on e79e49522 (notes over beta.163-167, CHANGELOG [2.0.0-beta.167], README links); lint clean; evaluate --quick 86/0. AC2: soak 04:1x: .61 beta.167 3:35 h, bootcount 201 (=OTA+2), MQTT on, heap 71.3-71.6 KB, only the old TASK-1763 crash entry; .63 beta.166 (same C code) 4:53 h, bootcount 178, MQTT on, heap 55.0 KB, crashlog empty; .63 ping loss noted for a coordinator task. Gates: 4-language screenshots, SAT page on 167, webui suite (except TASK-1766 flake), src diff 166->167 version lines only. AC3: build.bat 04:19 at 73.1 % commit: 2x SUCCESS + 'Build completed successfully'. AC4: tag v2.0.0-beta.167 -> ddf849818; prerelease --latest=false (Latest v1.7.9); 7 assets = build/; download sha256 4e5e4ba4...359c = SHA256SUMS; https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.167. AC5: #beta-testing post by 97, msg 1569552659424485459, read back identical; #alpha-testing covered by 80's zip post 1569496816255377473. Not verified: this release build flashed on a bench unit (same code as e79e49522 on .61); the flame-out protection on a real boiler; SAT/Advanced pages per language.
<!-- SECTION:FINAL_SUMMARY:END -->
