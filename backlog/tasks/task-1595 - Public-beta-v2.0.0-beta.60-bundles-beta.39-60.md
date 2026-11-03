---
id: TASK-1595
title: Public beta v2.0.0-beta.60 (bundles beta.39-60)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T19:46:00Z'
updated_date: '2026-11-03T22:37:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 740000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Evening public beta (20:00 slot missed; maintainer decision 2026-11-03 ~20:45 local, relayed by the coordinating 2.0.0 session: publish beta.60 after its 1 h MQTT bench soak via /beta-release-2x as beta.38 this morning).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes and CHANGELOG rollover committed; TASK-1565 change described accurately (form-encoded POST -> 400, raw JSON/text plain unchanged, verified on the device)
- [x] #2 esp32-combo build from the release commit, OTA on the Classic+S3 bench unit, 1 h soak with MQTT: bootcount = flash + 2, no crashlog, heap not trending down
- [x] #3 Tag v2.0.0-beta.60 on the release commit, GitHub prerelease --latest=false, assets and SHA256SUMS match build/, latest stays the 1.x stable
- [ ] #4 #alpha-testing zip post and #beta-testing announcement (text approved by Robert)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Go: Robert, 2026-11-03 ~20:45 local, interactive in the coordinating session (Geheugengebruik beperken), relayed to this session: publish beta.60 as public beta after the 1 h MQTT soak, notes since beta.38, prerelease --latest=false, one #beta-testing post, notes must flag TASK-1565. Device check of the TASK-1565 wording on .61: text/plain 'comfort' 200, text/plain 'value=comfort' 400, form 400.

2026-11-03 23:3x local: Robert 'Publish now' (in this session) = waiver of the remaining soak minutes. Soak evidence beta.60+1c34c39: zaphod .63 2h33m uptime, MQTT since 22:01 UTC (~33 min) plus ~27 min earlier before the broker kill, heap flat 56344-56400, boot 96 = flash+2, no crashlog; marvin .61 1h44m uptime, heap ~72960 flat, boot 24, cmd_dropped 0, no crashlog. Tag v2.0.0-beta.60 on 1c34c39f3 (pushed, fast-forward); GitHub prerelease published, latest stays v1.7.8; 7 assets sizes = build/, download sha256 of the flash zip = SHA256SUMS (c3315c3a...). Notes and announcement corrected to the shortened soak. #alpha-testing zip post REFUSED by discord-post-build.sh (prerelease already published guard); the same firmware was posted earlier as the beta.60 alpha zip (90e691d).
<!-- SECTION:NOTES:END -->
