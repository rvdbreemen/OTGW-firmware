---
id: TASK-704
title: >-
  Fix: discovery last-verify outcome always shows Unknown
  (disc_last_verify_epoch stays 0)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-25 20:34'
updated_date: '2026-05-25 22:26'
labels:
  - bug
  - mqtt
  - discovery
dependencies: []
references:
  - 'Discord #beta-testing'
  - user crashevans
  - '2026-05-25'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans on #beta-testing (2026-05-25) running beta.21. The disc_last_verify_epoch MQTT stat always publishes 0 and disc_verify_runs stays 0, even after 3+ hours of runtime. The 'Discovery last verify outcome' in the WebUI always shows 'Unknown'. The verify routine appears to never trigger or never write back its result.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 disc_verify_runs increments correctly after a verify cycle completes
- [ ] #2 disc_last_verify_epoch publishes a non-zero value (Unix timestamp) after the first verify run
- [ ] #3 WebUI 'Discovery last verify outcome' shows a meaningful value (not Unknown) after firmware has been running for >30 minutes with MQTT connected
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Voeg first-run trigger toe in hourly block (OTGW-firmware.ino): als bDiscoveryAutoVerify && iLastVerifyEpoch==0, roep startDiscoveryVerification() aan\n2. Verifieer dat state.discovery.iLastVerifyEpoch direct toegankelijk is in OTGW-firmware.ino\n3. Build groen\n4. Evaluator groen\n5. Commit + push
<!-- SECTION:PLAN:END -->
