---
id: TASK-1470
title: >-
  Local 24 h diagnostic recording with web UI download and host replay (step 1
  of TASK-1469, no cloud)
status: To Do
assignee: []
created_date: '2026-10-29T09:03:00Z'
updated_date: '2026-10-29T12:46:00Z'
labels:
  - sat
  - diagnostics
  - replay
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 630000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer decision 2026-10-29 on TASK-1469 (gruijter's proposal): first a LOCAL recording, cloud later. Record the same event stream as the proposal (header with version/board/redacted settings, OT frames T/R/B/A with time delta, external inputs: MQTT/REST commands, weather, BLE readings, setting changes; PIC commands and responses; MQTT publishes at sendMQTTData; reboot/crash), compact binary with sequence numbers, and offer it as a download in the web UI (no cloud, no account, no outbound traffic). Budget per board (PSRAM boards larger buffer, no-PSRAM boards small ring with visible gaps); flash headroom ~196 KB. Replay: extend the OT replay (TASK-1294) with timestamp pacing, input injection and host streaming, plus a host diff tool on PIC commands and MQTT publishes. Before building: measure RAM headroom on a no-PSRAM board with the web UI open, record rate and size over 24 h, flash cost; respect the 2-concurrent-API rule for the download. Needs a Proposed ADR (new data path + replay format).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Proposed ADR (adr-kit) for the recording format, buffer budget per board and the download path
- [ ] #2 Measurements before building: RAM headroom (no-PSRAM board, web UI open), record rate and size over 24 h, flash cost; recorded in the notes
- [ ] #3 A recorded day replays on the host and the diff tool shows PIC-command and MQTT-publish differences between two firmware builds (demonstrated old vs new)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Constraint from martreides (#alpha-testing msg 1565312598595993792, 2026-10-29): units may run in a restricted VLAN with all outbound internet blocked. Recording must work fully local with no cloud dependency; a later cloud upload must fail quietly (buffer/drop, no stall, no reboot). See TASK-1471 for the existing outbound paths.

Robert 2026-10-29: local first; cloud upload parked as a later feature (task-1473). Captures and recordings stay on the user's own network.
<!-- SECTION:NOTES:END -->
