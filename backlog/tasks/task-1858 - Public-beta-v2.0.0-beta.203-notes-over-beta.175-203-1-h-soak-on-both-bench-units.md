---
id: TASK-1858
title: >-
  Public beta v2.0.0-beta.203 (notes over beta.175-203, 1 h soak on both bench
  units)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T05:17:00Z'
updated_date: '2026-11-12T06:33:00Z'
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
- [ ] #3 Release build from the release commit: Build completed successfully, artifacts with the release githash; bench OTA check by 09
- [ ] #4 Tag v2.0.0-beta.203 on the release commit; prerelease published with --latest=false; releases/latest still the 1.x stable; downloaded zip sha256 equals SHA256SUMS
- [ ] #5 Zip posted in #alpha-testing; #beta-testing announcement sent by 92 and read back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12: Robert typed /beta-release-2x in session d3 (07:28). Bump question (bot DM msg 1570317346185412753): Robert answered A by DM at 06:28:40 UTC (msg 1570324436509266005) and explained at 06:29:52 UTC (msg 1570324735953473658): a bump only when something other than banner files and documentation changes; documentation can be updated on its own and needs no bump on dev. So: docs-only release commit on the candidate 61b69e0 (origin/dev bf3c581ce adds a test stub only; src identical), tag v2.0.0-beta.203, no bump. Soak (09): .63 Classic+PIC 6.8 06:09:58-07:11:55 bootcount 13 unchanged, MQTT on, no new crash (old latched 2026-11-10 panic), freeheap 55016->53884, maxblk 31732; .61 OT-Direct 06:10:20-07:11:56 bootcount 445 unchanged, MQTT on, crashlog none, freeheap 68832->68668, maxblk 31732.

Robert's words verbatim (DM): 06:28:40 UTC msg 1570324436509266005: "A". 06:29:52 UTC msg 1570324735953473658: "Alleen als er iets verandert, anders dan alleen banner bestanden en documentatie, dan is dat een bump waard, documentatie mag ook los bijgewerkt worden. Daar is op dev in elk geval geen bump voor nodig, documentatie zit namelijk niet in de firmware release."
<!-- SECTION:NOTES:END -->
