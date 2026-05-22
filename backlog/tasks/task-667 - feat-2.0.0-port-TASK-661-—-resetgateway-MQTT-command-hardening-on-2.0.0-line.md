---
id: TASK-667
title: 'feat-2.0.0: port TASK-661 — resetgateway MQTT command hardening on 2.0.0 line'
status: To Do
assignee: []
created_date: '2026-05-22 06:24'
labels:
  - port
  - 2.0.0
  - security
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Mirror of TASK-661 on 2.0.0 worktree (claude/2.0.0-port-from-review-xOxi4). The resetgateway dispatch exists on 2.0.0 at src/OTGW-firmware/MQTTstuff.ino:453 (s_cmd_resetgateway), :545 (dispatch table), :945 (handler). Pseudo-ID is 244 on 2.0.0 (vs 251 on dev) — different line numbers but same semantics. Apply identical payload-validation + rate-limit hardening.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 2.0.0 src/OTGW-firmware/MQTTstuff.ino resetgateway dispatch requires exact payload match (same value as picked in TASK-661 — keep dev/2.0.0 contracts identical)
- [ ] #2 2.0.0 rate-limit: subsequent resetgateway calls within N seconds dropped + logged; N = same const value as TASK-661
- [ ] #3 2.0.0 HA discovery payload_press matches the new required payload (no drift)
- [ ] #4 2.0.0 version.h prerelease bumped via bin/bump-prerelease.sh
- [ ] #5 python build.py --firmware passes on 2.0.0
- [ ] #6 python evaluate.py --quick passes on 2.0.0
<!-- AC:END -->
