---
id: TASK-3019
title: >-
  SAT zones: web UI zone status, per-zone MQTT diagnostics from 1 zone, and a
  retained zone_count state
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-21T17:16:00Z'
updated_date: '2026-11-27T20:05:00Z'
labels:
  - sat
  - mqtt
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1134000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573748508622721095 (2026-11-21), three requests: (1) the web UI shows no zone status; add temperature, target and active per zone. (2) sat/zone/<n>/active|output|error are only published when zone_count > 1, so a single zone cannot be checked; publish them from 1 zone. Verified at origin/dev: satPublishZoneDiagnostics() is called only under 'if (settings.sat.iZoneCount > 1)' (SATcontrol.ino:8330; also :6602). (3) zone_count has a set command (MQTTstuff.ino:775, SATzonecount) but no retained state topic, so clients cannot read it back. Tester request: needs Robert's OK. Fits the TASK-3014 naming work (areas vs zones).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides which of the three go in
- [ ] #2 Per the decision: zone diagnostics publish from zone_count >= 1; zone_count state topic (retained or heartbeat per ADR-111); web UI zone status rows; host test on the real publisher red before / green after; screenshots for the UI
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN (otgw-webui, 2026-11-26; nothing built). Decided by Robert 2026-11-21 (msg 1573770202410123305) and entry 459: zone_count counts EXTRA rooms only, default 0, zone diagnostics from 1 zone, zone_count state topic, web UI zone status. Reading the code first changes the migration question:
FINDINGS (origin/dev 9f3b8af60): the zone loops already count extra rooms. satControlLoop SATcontrol.ino:8214-8225 runs for z < iZoneCount over satZones[0..]; satPublishZoneDiagnostics :3726-3735 and satShouldDiscoverZone :3510-3514 likewise; a zone without usable inputs counts as no zone (comment :8209-8210). So stored zone_count=1 behaves today as 'one extra room, silent if it gets no data'. Only the comment SATtypes.h:613 ('1 = single-zone'), the default 1, the clamp 1..4 (settingStuff.ino:1251) and the diagnostics gate '> 1' (:8330, deliberate TASK-1440) carry the old 'main room only' meaning. No code path treats 1 as main-room-only, so NO value mapping is needed for behaviour: stored 0 stays 0 once the clamp allows it, stored 1 keeps meaning one extra room.
Proposed migration rule (for otgw-dev's sub-task): (a) default 0, clamp 0..4 in settingStuff + REST range + settings help; comment SATtypes.h fixed; (b) stored values untouched; (c) to keep untouched installs (stored default 1, no zone data) free of new retained traffic (the TASK-1440 concern), publish zone/<n>/active|output|error only for zones that have ever received data (satZones[i].iLastUpdateMs != 0) or are explicitly counted AND have data; gruijter's single zone with pushed data then publishes from 1 zone; (d) new retained sat/zone_count state through publishIfChangedI (ADR-111) with the shadow in the republish reset. Open point for otgw-dev to confirm: an install that set 1 deliberately and pushes nothing stays silent under (c), same as today.
SPLIT: firmware sub-task (otgw-dev): a-d above plus a 'zones' array in /api/v2/sat/status (index, room_temp, setpoint, active, per zone up to iZoneCount; satSendStatusJSON SATcontrol.ino:4921 region) with a host test on the real code, red old / green fix. Web UI (me, after the status field lands): zone status rows per zone (temperature, target, active) in the SAT status view, hidden at 0 zones, EN/NL/DE/EL labels (nl 'Zone 1', no 'area' wording per TASK-3014), a webui test red/green, screenshots before/after light+dark, help text if a row needs one. Docs: manual ch05 5.15 / h05, MQTT.md zone_count state row: otgw-docs or me with a docs slot.
Not verified yet: everything above is plan only.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 gruijter follow-up (msg 1573750283282940076): the docs say every zone, zone 1 included, is an extra room next to the main room, yet iZoneCount defaults to 1 (SATtypes.h:614 comment: '1-4, default 1 = single-zone') and diagnostics need > 1, which looks like a leftover of '1 = main room only'. Suggests default zone_count 0 (no extra rooms) and zone topics from 1 zone. Verified: the code comment and the docs disagree on what zone_count = 1 means; the design must settle the meaning first (and migrate stored 1s accordingly).

2026-11-21: released by Robert in #alpha-testing (msg 1573770202410123305, reply to our zone_count note): fix as suggested, i.e. gruijter's suggestion: zone_count counts extra rooms only (default 0 = no extra rooms, matching the docs), zone diagnostics publish from 1 zone, plus the zone_count state topic and web UI zone status of the first note. Migration of stored zone_count = 1 (old meaning 'main room only') is for the plan to state.

2026-11-21: Robert confirmed the zone_count meaning (DM check answered A) and timing after Wednesday 25 Nov; recorded as entry 459 in the private SAT decision register. Open per entry 459: exact migration mapping of stored values (check how the code counts the main room first).

2026-11-26 otgw-webui: AC1 already decided: Robert 2026-11-21 (msg 1573770202410123305) took all three requests, zone_count meaning confirmed by DM (entry 459). Open for the plan: migration mapping of stored zone_count (1 = main room only under the old meaning) after reading how the code counts the main room; timing after 25 Nov.

otgw-architect verification 2026-11-27 21:xx (D-class triage): kept. Firmware side is in TASK-3102 (b56c4a143: zone topics from one extra room, retained sat/zone_count). OPEN in AC2: web UI zone status rows and the screenshots (owner otgw-webui), and a host test red/green named for this task.
<!-- SECTION:NOTES:END -->
