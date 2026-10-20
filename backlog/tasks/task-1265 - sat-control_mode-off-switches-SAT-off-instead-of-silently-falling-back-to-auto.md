---
id: TASK-1265
title: sat/control_mode off switches SAT off instead of silently falling back to auto
status: Done
assignee:
  - '@claude'
created_date: '2026-10-20 02:19'
updated_date: '2026-10-20 02:43'
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
- [x] #1 MQTT sat/control_mode off disables SAT (state enabled=false, published), other mode values unchanged (host test or bench MQTT transcript, red on old code)
- [x] #2 Docs describe off as a valid control_mode value
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation: satHandleControlMode() now maps 'off' (case-insensitive, named form only) to satHandleEnabled("0"), the same path as sat/enabled 0 (persists SATenabled via updateSetting). Numeric '0' keeps meaning auto (existing contract; note the status JSON reports control_mode 0 = off, a separate enum mismatch left as is). Covers MQTT sat/control_mode, REST /api/v2/sat/mode and the REST sat settings route, all of which call this handler. The auto-discovered HA climate entity has no mode_cmd_t, so default HA behaviour is unchanged; the manual YAML example in ch04/h04 that maps HA modes to sat/control_mode now works for 'off'. Docs: MANUAL.md, ch04/h04, ch09/h09, docs/api/MQTT.md, openapi.yaml.

2026-10-20 bench OTGW32 .88.61. RED alpha.439+9ac0c39: SAT enabled=true, POST /api/v2/sat/mode/off -> 200, status stayed enabled=true, control_mode 0->1 (off taken as auto), settings satenabled true. GREEN alpha.441+07de5ff: same REST call -> enabled=false, control_mode 0, satenabled false; continuous->1, pwm->2, auto->1 still as before. MQTT on alpha.441 via a temporary local broker at the device's configured 192.168.88.32 (device subscribed OTGW/set/otgw-1020BA21B4F8/#): publish sat/control_mode off -> enabled False control_mode 0; publish sat/enabled 1 -> enabled True (restored). MQTT red run on the old code was not done (REST red covers the same satHandleControlMode). Evidence: %TEMP%\bench-1265-1266\red_rest_off.txt, green_sat.txt, mqtt_green.txt.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
sat/control_mode "off" now switches SAT off (same path as sat/enabled 0) instead of falling back to auto. AC1: bench old-vs-fix on the same unit: REST off left SAT enabled on alpha.439, disabled it on alpha.441; MQTT off disabled it on alpha.441 (local broker transcript); other modes unchanged. AC2: MANUAL.md, ch04/h04, ch09/h09, docs/api/MQTT.md and openapi.yaml list off. Not done: an MQTT red run on the old firmware (REST red uses the same handler). Unchanged on purpose: numeric 0 still means auto, while the status JSON reports control_mode 0 for off.
<!-- SECTION:FINAL_SUMMARY:END -->
