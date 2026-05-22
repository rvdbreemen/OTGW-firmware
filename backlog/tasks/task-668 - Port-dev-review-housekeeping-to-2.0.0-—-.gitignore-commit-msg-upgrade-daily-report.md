---
id: TASK-668
title: >-
  Port dev review housekeeping to 2.0.0 — .gitignore + commit-msg upgrade +
  daily-report
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 06:28'
updated_date: '2026-05-22 06:55'
labels:
  - port
  - housekeeping
dependencies: []
priority: high
ordinal: 50000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Consolidated port task on 2.0.0 covering dev review TASK-653 findings that also affect this worktree. Covers: (a) repo-hygiene .gitignore + commit-msg hook upgrade + daily-issue-report ignore (port of dev TASK-657/659/663), (b) resetgateway MQTT hardening — payload validation + 5s rate-limit (port of dev TASK-661), (c) intentional first-OT-message bypass comment (port of dev TASK-665), (d) openHAB/Domoticz guide anchor fix (port of dev TASK-664), (e) HTML streaming verification — read-only audit confirming all .html and .js paths use streamFile() (port of dev TASK-662).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 2.0.0 /.gitignore extended with /OTGW_*.txt, /OTGW_*.log, /plan/, docs/daily-issue-report.md
- [ ] #2 git rm --cached docs/daily-issue-report.md (file kept on disk as local artefact)
- [ ] #3 .githooks/commit-msg upgraded with Pass 1 + OTGW_TASK_HOOK_DISABLE bypass
- [ ] #4 .githooks/README documents both hooks + bypass + activation
- [ ] #5 /.gitignore extended with /OTGW_*.txt, /OTGW_*.log, /plan/, docs/daily-issue-report.md
- [ ] #6 git rm --cached docs/daily-issue-report.md (file kept on disk as local artefact)
- [ ] #7 .githooks/commit-msg upgraded with Pass 1 + OTGW_TASK_HOOK_DISABLE bypass
- [ ] #8 .githooks/README documents both hooks + bypass + activation
- [ ] #9 src/OTGW-firmware/MQTTstuff.ino:943 resetgateway dispatch requires exact payload '1', rate-limited to 5s (matches dev TASK-661 contract)
- [ ] #10 src/OTGW-firmware/OTGW-Core.ino first-message branch carries the boot-time one-shot bypass comment (matches dev TASK-665)
- [ ] #11 docs/guides/OPENHAB.md + DOMOTICZ.md anchor points at #migration-note-200-topic-shape-transition (the 2.0.0 equivalent heading at MQTT.md:622)
- [ ] #12 L1 audit recorded: FSexplorer.ino serves index.html (15 KB) and FSexplorer.html (20+ KB) via streamFile() — no code change needed
- [ ] #13 CHANGELOG.md updated with resetgateway hardening entry under [Unreleased] -> Changed
- [ ] #14 version.h prerelease bumped via bin/bump-prerelease.sh: alpha.52 -> alpha.53
<!-- AC:END -->
