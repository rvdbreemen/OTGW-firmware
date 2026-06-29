---
id: TASK-925
title: >-
  feat(webui): solve all v2 audit findings + gas/heat-pump source control
  (TASK-908 P9)
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-24 18:50'
updated_date: '2026-06-28 21:49'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 141000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement approved plan cozy-juggling-fern: Phase1 firmware (satsource whitelist, PATCH/DELETE auth, comment scrubs); Phase2 v2 UI (REST seed, writable source toggle, steppers->sat/target, stats sort, log download, conn detail, support hover, graph chips+CSV+PNG, enum selects); Phase3 classic satsystem label fix + heating_source select; Phase4 docs (MQTT retained line, MEMORY ADR-124->140) + SAT golden regen.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Firmware: satsource settable + PATCH/DELETE auth + comment scrubs
- [x] #2 v2 UI: all unwired controls wired + REST seed + source toggle
- [x] #3 Classic satsystem label bug fixed + heating_source select
- [ ] #4 Docs/oracle updated; build+eval green; verified on device
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Loop assessment 2026-06-26: Phase2 (v2 UI: REST seed, source toggle, steppers, stats sort, log download, conn detail, support hover, graph chips+CSV+PNG, enum selects) = DONE per TASK-933 audit + Playwright validation. Phase3 (classic satsource 'Heat Source' select) present at index.js:3630. REMAINING/unverified: Phase1 firmware (satsource whitelist in dispatch, PATCH/DELETE auth, comment scrubs), Phase4 (docs MQTT-retained line + MEMORY ADR-124->140 + SAT json-golden regen), and AC#4 on-device verification. Recommend as next focused iteration — left In Progress.

Phase4 partial (2026-06-27): ADR-124->140 comment scrub done for MQTTstuff.ino deviceForOTId block (header + cases 243/245/246/249 now cite ADR-140, matching MQTTHaDiscovery.cpp). MQTT-retained doc line already correct (MQTT.md:80 states not-retained). BLOCKED ON OFFLINE DEVICE: (a) SAT json-golden regen — scripts/json-golden/v2_sat_status.json + v2_health.json (now stale: alpha.276 added thermostat_age_s/boiler_age_s) need a live capture; (b) AC#4 on-device verify. OTGW32 .88.39 is OFFLINE (ping 100% loss, curl HTTP 000). .88.16 runs 1.7.0-beta (1.x line, not 2.0.0). Scattered historical-origin ADR-124 refs (MQTTstuff.ino:2051/2756, OTGW-firmware.h:835, MQTTstuff.h:380/394, MQTTHaDiscovery.cpp:1271/1721/1729) left as-is (accurate about the split's origin).

Phase4 golden-regen finding (2026-06-27, live OTGW32 @1.143/alpha.278): the json-golden set is alpha.210-stale (captured 2026-06-18, githash f2252e5) — 68 alpha versions behind. 'json_golden.py compare' FAILs broadly, mostly NOT from this work: build version/githash, network (88.39/KeepOut2 -> 1.143/Koekie), accumulated otd* settings + ADR-155 health fields. MY contribution = +thermostat_age_s/+boiler_age_s in /health (idle -1). A blind full 'capture' now would bake in this idle + fw(278)/fs(275)-mismatched + device-specific network state (huge diff, beyond 925 scope). The 925 'ovp_*/heating_source' SAT-golden item is subsumed by this broader staleness. RECOMMEND: a dedicated golden-refresh task captured against a CLEAN alpha.278 device with matching fs+config, not a tail-end partial. Phase4 status: MQTT-retained doc line=already correct; ADR-124->140 deviceForOTId scrub=DONE (b8523a07); golden-regen=deferred to golden-refresh task. AC#4 firmware behaviour now hardware-verified (alpha.278, /health age fields emit).

VERIFIED CODE-COMPLETE 2026-06-28 (loop): AC#1 Phase1 firmware already landed — satsource settable (settingStuff.ino:956 SATsource write + restAPI.ino:3385 GET emit, via TASK-891.8/943), PATCH/DELETE auth centralised (restAPI.ino:2408-2413, comment explicitly tags TASK-925), comment scrubs (b8523a07). AC#2 v2 UI done (Phase2). AC#3 classic selects present with correct labels/enums (index.js:3628 satsystem 'Heating System', :3630 satsource 'Heat Source', meta 7279/7280). AC#4 firmware behaviour hardware-verified alpha.278; the json-golden regen is split to TASK-944 (goldens ~70 alpha stale, beyond 925 scope, per this task's own recommendation). Moved In Progress -> In Review; closes when TASK-944 lands the refreshed goldens.
<!-- SECTION:NOTES:END -->
