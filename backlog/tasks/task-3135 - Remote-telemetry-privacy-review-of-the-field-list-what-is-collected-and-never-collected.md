---
id: TASK-3135
title: >-
  Remote telemetry: privacy review of the field list (what is collected and
  never collected)
status: To Do
assignee:
  - '@otgw-poc-dev'
created_date: '2026-11-27T19:01:00Z'
updated_date: '2026-11-27T19:54:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1186000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order (relayed by the coordinator 2026-11-27 19:5x): remote telemetry in the next beta (target Sat 28 Nov 20:00), opt-in, off by default, PSRAM boards only, experimental. Governing: ADR-218 (outbound TLS, PSRAM only), ADR-263 (PSRAM buffers), ADR-268 (one recorder, diagRec retired), ADR-299 (opt-in ask), and the new remote telemetry data-contract ADR (Proposed, otgw-architect, tonight). Existing work on lane-poc-merge: TASK-2937, 2984, 3001, 3010, 3026, 3092. Owners: otgw-poc-dev writes the field list, otgw-architect reviews it against the new ADR. Lane task TASK-3010 has the strict-privacy rules.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 One field list file: every recorded field or event, its source, its redaction; reviewed by otgw-architect with findings resolved
- [ ] #2 Never collected: IP, MAC, SSID, BSSID, host names, user names, e-mail, passwords, tokens, API keys, MQTT credentials, location beyond what the ADR allows; a host test greps a recorded sample for each and fails on a mutant that leaks one
- [ ] #3 The pseudonymous ID cannot be derived back to the MAC by a reader of the stored data
<!-- AC:END -->
