---
id: TASK-1858
title: >-
  Public beta v2.0.0-beta.203 (notes over beta.175-203, 1 h soak on both bench
  units)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T05:17:00Z'
updated_date: '2026-11-12T06:54:00Z'
labels:
  - release
  - 2.0.0
dependencies: []
priority: high
ordinal: 935000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's go (coordinator session, 2026-11-12 05:08:54 UTC, answer A: release beta.203 once the 1 h soak is green; 05:09:25 'Doe de slak op beide boards'). Docs-only release commit on top of the candidate 61b69e0 (beta.170/174 model, no bump), tag v2.0.0-beta.203, draft-first GitHub prerelease with --latest=false, #alpha-testing zip post, #beta-testing announcement by 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes, CHANGELOG [2.0.0-beta.203] and README links committed; no em dash, no OTGW32, no relative links
- [x] #2 1 h soak green on .61 and .63 (bootcount, crashlog, MQTT, heap) recorded
- [x] #3 Release build from the release commit: Build completed successfully, artifacts with the release githash; bench OTA check by 09
- [x] #4 Tag v2.0.0-beta.203 on the release commit; prerelease published with --latest=false; releases/latest still the 1.x stable; downloaded zip sha256 equals SHA256SUMS
- [ ] #5 Zip posted in #alpha-testing; #beta-testing announcement sent by 92 and read back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12: Robert typed /beta-release-2x in session d3 (07:28). Bump question (bot DM msg 1570317346185412753): Robert answered A by DM at 06:28:40 UTC (msg 1570324436509266005) and explained at 06:29:52 UTC (msg 1570324735953473658): a bump only when something other than banner files and documentation changes; documentation can be updated on its own and needs no bump on dev. So: docs-only release commit on the candidate 61b69e0 (origin/dev bf3c581ce adds a test stub only; src identical), tag v2.0.0-beta.203, no bump. Soak (09): .63 Classic+PIC 6.8 06:09:58-07:11:55 bootcount 13 unchanged, MQTT on, no new crash (old latched 2026-11-10 panic), freeheap 55016->53884, maxblk 31732; .61 OT-Direct 06:10:20-07:11:56 bootcount 445 unchanged, MQTT on, crashlog none, freeheap 68832->68668, maxblk 31732.

Robert's words verbatim (DM): 06:28:40 UTC msg 1570324436509266005: "A". 06:29:52 UTC msg 1570324735953473658: "Alleen als er iets verandert, anders dan alleen banner bestanden en documentatie, dan is dat een bump waard, documentatie mag ook los bijgewerkt worden. Daar is op dev in elk geval geen bump voor nodig, documentatie zit namelijk niet in de firmware release."

Phase 2-5 (2026-11-12): release commit 7655a91de (CHANGELOG, README, notes, this task; amended once before push only to quote Robert verbatim), evaluate --quick 86/0. Build from 7655a91de via build.bat esp32-combo: 2x 'Successfully created ESP32S3 image', 'Build completed successfully', artifacts 07:43-07:44 named 2.0.0-beta.203+7655a91 (ino.bin 2132576, littlefs 786432, zip 6309939). Bench by 09 on .63 (Classic + S3 Mini, PIC 6.8): ota_flash.py app+fs 07:46:19 exit 0; at +5 min fwversion 2.0.0-beta.203+7655a91, picfwversion 6.8, bootcount 13 -> 15, MQTT connected, health 200, crashlog summary unchanged (old latched 2026-11-10 panic only), settings diff 0 of 480 keys, freeheap 54124. Push origin/dev bf3c581ce..7655a91de; annotated tag v2.0.0-beta.203 -> 7655a91de. Draft with 7 assets, sizes equal to build/; zip holds flash_otgw.bat/.sh, capture-mqtt-debug.bat/.sh/-macos.sh, otgw_capture.py. Published: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.203 isDraft false, isPrerelease true; releases/latest still v1.7.9; downloaded zip sha256 c1188b1a763286ee6f9bfe62b0bf731b949df54f33c6163c8d875c9373a19e68 = SHA256SUMS. #alpha-testing zip post REFUSED by the one-tag-per-build guard (beta.203 already posted as alpha zip +61b69e0), as for v2.0.0-beta.174 (TASK-1809): 92 posts a note with the link instead.
<!-- SECTION:NOTES:END -->
