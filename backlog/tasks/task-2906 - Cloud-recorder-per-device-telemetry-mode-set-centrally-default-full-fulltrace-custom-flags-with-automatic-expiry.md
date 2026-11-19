---
id: TASK-2906
title: >-
  Cloud recorder: per-device telemetry mode set centrally (default, full,
  full+trace, custom flags) with automatic expiry
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T15:52:00Z'
updated_date: '2026-11-15T16:04:00Z'
labels:
  - cloud-recording
dependencies: []
priority: low
ordinal: 1008000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15: after enrollment a device always has the default telemetry setting; the maintainer can change it per device in its record; the gateway gets the mode at login. Purpose: turn on more logging centrally for a while to debug a problem. Choices: 1 modes default/full/custom + flags (advice), 2 mode_until with automatic return to default: yes, 3 refresh every 15 min: yes, 4 the extra lines also go to telnet (the debug flags drive both), 5 sattrace separate as full+trace. Simulation flags (sensorsim, otgwsim) are never set remotely. Research branch only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Backend: devices has mode (default|full|full+trace|custom), flags (multi-select of the ten log flags), mode_until, mode_note; a gateway can set none of them on enroll or update; tested with a mutant
- [x] #2 Firmware: mode parsed from the login/refresh record; effective flags = none for default or after mode_until, all log flags but sattrace for full, all for full+trace, the chosen ones for custom; never the simulation flags; host-tested on the real header
- [ ] #3 Firmware: token refresh every 15 min while logging; a changed mode applies within 15 min, an expired mode within a minute; flags it set are cleared again, flags the user set locally stay
- [x] #4 esp32-combo-tlsdyn build compiles; evaluate --quick no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 evidence. Backend: devices gets mode (select default|full|full+trace|custom; empty = default), flags (multi-select of the ten log flags), mode_until (date), mode_note (text 200); create and update rules refuse all four from a gateway (OTGW_MODE_FIELDS_UNSET). test_client.py check 12 (enroll with mode refused, the four gateway updates refused, admin sets custom/sat+mqtt/2030, login and auth-refresh both return it) PASS, 1-10 PASS; mutant without the rule part: check 12 FAILs (enroll 200, gateway updates 200). Firmware: crec::parseRemote/remoteEffective/remoteParseDate in CloudRecPolicy.h; tests/test_cloudrec_policy.py 7/7 on a real captured login reply (incl. a mode_note that contains the text "mode":"full"), full = all but sattrace, full+trace = all, custom = chosen, simulation names never map, expiry exact to the second; a mutant that ends a day late fails. CloudRecDevice.h: cloudRecApplyRemote() through debugFlagByName (entries with onChange skipped), owned-flag bookkeeping so user-set flags stay; mode read at every login, auth-refresh every 15 min while logging (blocked -> opt-out, auth -> relogin, down -> backoff), expiry checked every loop pass (500 ms); blocked clears the mode. Login reply buffer 1024 -> 1536. Build esp32-combo-tlsdyn 2.0.0-poc.6 exit 0: RAM 156,356 B, app 2,253,003 B (98.2 %); ELF still has the dynamic mbedTLS and session tickets; no CloudRec warnings. evaluate --quick 0 FAIL 0 WARN. Bench check 14 added. AC 3 needs the bench.
<!-- SECTION:NOTES:END -->
