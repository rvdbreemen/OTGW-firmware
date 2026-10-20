---
id: TASK-1265
title: sat/control_mode off switches SAT off instead of silently falling back to auto
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-20 02:19'
updated_date: '2026-10-20 02:25'
labels:
  - bug
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 395000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1256: satHandleControlMode() parses the payload with atoi, so 'off' gives 0 and maps to auto/continuous (SATcontrol.ino ~2002). Maintainer decision 2026-10-20: 'off' on sat/control_mode disables SAT, same effect as sat/enabled 0. Update docs (MANUAL.md, ch04/h04, ch09/h09) that now say to use sat/enabled.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT sat/control_mode off disables SAT (state enabled=false, published), other mode values unchanged (host test or bench MQTT transcript, red on old code)
- [ ] #2 Docs describe off as a valid control_mode value
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation: satHandleControlMode() now maps 'off' (case-insensitive, named form only) to satHandleEnabled("0"), the same path as sat/enabled 0 (persists SATenabled via updateSetting). Numeric '0' keeps meaning auto (existing contract; note the status JSON reports control_mode 0 = off, a separate enum mismatch left as is). Covers MQTT sat/control_mode, REST /api/v2/sat/mode and the REST sat settings route, all of which call this handler. The auto-discovered HA climate entity has no mode_cmd_t, so default HA behaviour is unchanged; the manual YAML example in ch04/h04 that maps HA modes to sat/control_mode now works for 'off'. Docs: MANUAL.md, ch04/h04, ch09/h09, docs/api/MQTT.md, openapi.yaml.
<!-- SECTION:NOTES:END -->
