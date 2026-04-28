---
id: TASK-479
title: 'fix(mqtt): port ADR-066 master-topic + slave-echo gating from dev'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-28 20:54'
updated_date: '2026-04-28 21:01'
labels:
  - mqtt
  - ADR-066
  - port-from-dev
dependencies: []
references:
  - docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md
  - docs/api/MQTT-message-id-echo-audit.md
  - src/OTGW-firmware/OTGW-Core.h
  - src/OTGW-firmware/OTGW-Core.ino
  - src/OTGW-firmware/MQTTstuff.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the ADR-066 publish-gating decision from `dev` branch into `feature-dev-2.0.0-otgw32-esp32-sat-support`. The decision is platform-neutral; same structural fix applies to both branches.

## Context

ADR-066 was implemented on dev as TASK-478 (commit `8f87bfaa` plus `OT_WRITE_ACK` typo fix in `297c6eb1`) for v1.5.0-beta.3 release. Feature branch shares the MQTT publish architecture: `OTlookup_t` + `OTmap[]`, `is_value_valid()`, `publishToSourceTopic()`, eight call sites in `print_*()` functions.

Key difference: feature branch's `is_value_valid()` is the strict v1.3.5 form (no WRITE-ACK), so it never had the master-topic flapping regression that triggered the dev fix. Port still beneficial: enables echo-aware /boiler subtopic for source-separation users.

## Implementation

Already executed in commit `d71d8063`:

1. `OTGW-Core.h`: added `bSlaveEchoesValue` field to `OTlookup_t`, populated all 133 `OTmap[]` entries (default `true`; six MsgIDs `false`: 14, 16, 23, 24, 37, 98)
2. `OTGW-Core.ino`: broadened `is_value_valid()` to accept WRITE-ACK for OT_WRITE/OT_RW (matching dev post-1.4.1 state), added strict `is_value_valid_for_master_topic()` helper, wrapped 8 `sendMQTTData()` call sites with master-topic guard
3. `MQTTstuff.ino`: added early-return in `publishToSourceTopic()` for `OT_WRITE_ACK && !OTlookupitem.bSlaveEchoesValue`
4. `docs/adr/ADR-066-...md`: copied verbatim from dev
5. `docs/api/MQTT-message-id-echo-audit.md`: copied verbatim from dev
6. `docs/api/MQTT.md`: appended publish-gating paragraph under Source-Separated Topics section
7. `CHANGELOG.md`: `[Unreleased]` entry adapted to feature branch

## Acceptance Criteria
<!-- AC:BEGIN -->
- ESP8266 + ESP32 builds green
- Code matches dev's commit `8f87bfaa` semantics: same struct field, same helper signature, same call-site wraps, same publishToSourceTopic gate
- Feature-branch-specific consideration: `is_value_valid` was strict, now broadened (the upgrade enables source-separation observability that wasn't previously functional for echo MsgIDs like MaxTSet)
- Hardware verification (deferred to user): MQTT topic OTGW/value/{id}/Tr stable, /boiler subtopic for non-echo MsgIDs receives no Write-Ack publication, /boiler for echo MsgIDs (e.g. MaxTSet) shows slave-clamped value when bSeparateSources=true

## References

- Parent decision: `docs/adr/ADR-066-mqtt-publish-gating-by-source-and-slave-echo.md`
- Spec audit: `docs/api/MQTT-message-id-echo-audit.md`
- Dev-side task: TASK-478 (lives in dev's backlog/tasks/)
- Plan: `C:\Users\rvdbr\.claude\plans\the-design-package-still-elegant-globe.md`
- Implementation commit: `d71d8063`
<!-- SECTION:DESCRIPTION:END -->

- [x] #1 OTlookup_t has bSlaveEchoesValue field; all 133 OTmap entries populated correctly (six MsgIDs false: 14, 16, 23, 24, 37, 98)
- [x] #2 is_value_valid_for_master_topic helper exists in OTGW-Core.ino with strict v1.3.5 semantics (Read-Ack + Write-Data only)
- [x] #3 is_value_valid broadened to accept WRITE-ACK (matching dev post-1.4.1 state)
- [x] #4 Eight sendMQTTData call sites in print_*() functions wrapped with is_value_valid_for_master_topic guard
- [x] #5 publishToSourceTopic early-returns on OT_WRITE_ACK && !OTlookupitem.bSlaveEchoesValue
- [x] #6 Build green on ESP8266 and ESP32
- [x] #7 ADR-066 + audit doc verbatim from dev
- [x] #8 MQTT.md and CHANGELOG.md updated
- [x] #9 Hardware verification: master topic stable, /boiler subtopic correctly gated
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port van ADR-066 naar feature-dev-2.0.0-otgw32-esp32-sat-support succesvol afgerond. Twee commits (d71d8063 + aadb1869) gepusht naar origin. Hardware-verificatie door gebruiker bevestigd: master topic stabiel, source-separation /boiler subtopic correct gegate. Same decision (ADR-066) nu live op beide branches: dev (1.5.0-beta.3) en feature-dev-2.0.0-otgw32-esp32-sat-support.
<!-- SECTION:FINAL_SUMMARY:END -->
