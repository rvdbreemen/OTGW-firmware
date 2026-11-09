---
id: TASK-1749
title: >-
  OT-Direct: show an active TT=/TC= remote override in the overrides panel again
  (gateway mode)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T09:26:00Z'
updated_date: '2026-11-09T10:20:00Z'
labels:
  - otdirect
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 851000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). In gateway mode TT=/TC= no longer set a write override on MsgID 16/100, so /api/v2/otdirect/overrides (getOTDirectOverridesJSON, OTDirect.ino ~1131-1139) and state.otd.iOverrideCount / overrides_active (restAPI.ino ~3910) no longer list an active remote override; the v2 overrides panel and the aRoomSet badge (v2.js ~2092-2123) then show 'No active overrides' while TT= is active. Expose otRemoteOverride (mode, value, accepted, MsgID 9 answered) as its own entry and show it in the panel, with Clear = TT=0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 GET /api/v2/otdirect/overrides lists an active TT=/TC= remote override (mode, value, whether the thermostat read MsgID 9, accepted) in gateway mode; host test
- [x] #2 The v2 overrides panel shows it with a working Clear (TT=0); webui test red on the current UI, green after
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Part 1 (now, no conflict): OTDirect.ino getOTDirectOverridesJSON (~1131) gains a 'remote' object for the active TT=/TC= remote override (mode temporary|constant, value in C, msgid9_answered, accepted, mismatches; null/absent when none), and state.otd.iOverrideCount / overrides_active counts it. Update docs/api (openapi + README) for the new field; REST contract test if one covers this route; host test that slices the real JSON builder: red on 2fedae505 (no remote entry while TT active), green after. Part 2 (after 4c's TASK-1747 push, which changes fetchOverrides): v2.js overrides panel renders the remote entry with Clear = TT=0; webui test red/green. Then rebase, bump, build, OTA .61, bench (master mode on .61: TT=21.5 visible in the panel, TT=0 clears), soak, push, post. No ADR (no contract change beyond an added field).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 part 1 (otgw-firmware-80; implementation subagent, rerun in session): OTDirect.ino sendOTDirectOverridesJSON() adds overrides.remote (null, or {mode temporary|constant, value C, msgid9_answered, accepted, mismatches}) after sat_owns, existing fields unchanged; new otdActiveOverrideCount() counts write slots + 1 for an active remote override, feeding state.otd.iOverrideCount (overrides_active / otdoverrides / override_count: all 4 readers are informational totals). Docs: openapi.yaml, docs/api/README.md, MQTT.md overrides_active row. Test tests/test_otd_overrides_remote_json.py compiles the real sendOTDirectOverridesJSON + otdActiveOverrideCount + jsonEmit.h: fix Ran 4 OK; old code (2fedae505) FAILED 4. Honest note: the genuine red is the missing 'remote' key (key-order case: ['write','response','modify','unknown','sat_owns'] without 'remote'); the 3 count cases are red only because the old code has no otdActiveOverrideCount and the test substitutes a 255 stub. evaluate --quick 0 FAIL. Part 2 (v2.js panel, AC2) waits for 4c's TASK-1747 push (same fetchOverrides region).

2026-11-09 part 2 (otgw-firmware-80; implementation subagent, rerun in session): v2.js otdRemoteRow() renders overrides.remote as 'Room setpoint override (TT=|TC=, temporary|constant) · MsgID 9  <value> °C' with status 'waiting for the thermostat to read it' / 'offered to the thermostat' / 'accepted by the thermostat'; Clear sends TT=0 via otdOvrSendCmd; with sat_owns the SAT note instead of Clear; normOtdOvr pushes id 9 so the aRoomSet badge and the 'N injected' count include it; fetchOverrides (TASK-1747 503 handling) untouched. tests/webui/otd-remote-override-panel.test.mjs: new UI 5/5, old v2.js 1/5 (4 red; the null/absent case is a regression guard, green on both). otd-override-clear 5/5, otd-hidden-in-pic-mode 2/2, health-chain-one-in-flight 2/2. Review fix (master mode, found in session): .61 runs master mode, where TT= still writes MsgID 16, so the remote entry would show twice with a misleading 'waiting for the thermostat' text; otdRemoteOverrideListed() lists and counts the remote entry only outside OTD_MODE_MASTER. test_otd_overrides_remote_json.py now 5/5 incl. a master case, which fails on a variant without that rule (1 failure).
<!-- SECTION:NOTES:END -->
