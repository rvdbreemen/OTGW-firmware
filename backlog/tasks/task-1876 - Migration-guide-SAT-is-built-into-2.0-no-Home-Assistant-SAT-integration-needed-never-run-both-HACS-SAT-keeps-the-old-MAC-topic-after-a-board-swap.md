---
id: TASK-1876
title: >-
  Migration guide: SAT is built into 2.0, no Home Assistant SAT integration
  needed; never run both; HACS SAT keeps the old MAC topic after a board swap
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T21:13:00Z'
updated_date: '2026-11-12T21:17:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 950000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's order, #nederlandse-ondersteuning https://discord.com/channels/812969634638725140/815561033036333076/1570545059907244074 ('Update the migratie gids') and 1570545246725480489 ('In de 2.0 firmware zit standaard de SAT implementatie in de firmware zelf, geen apart component in HA meer nodig'). Follow-up of TASK-1825 (Done). Trigger: indigo_light's case (https://discord.com/channels/812969634638725140/815561033036333076/1570535626447782012): after the Wemos-to-S3 swap the HACS SAT integration kept sending to OTGW/set/otgw-<old MAC>/command. Scope: docs/manuals/nl/h02-hardware-installatie.md and docs/manuals/en/ch02-hardware-setup.md, section on upgrading from v1.x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The v1.x upgrade section (NL and EN) says SAT is built into the 2.0 firmware, off by default, so the Home Assistant SAT integration is no longer needed
- [x] #2 It says not to run both: while the firmware SAT is on it refuses CS=, CH=, MM=, C2=, H2=, TT= and TC= from MQTT and REST (TASK-1695), so an HA SAT integration cannot control; keep the integration only with the firmware SAT off
- [x] #3 It explains that an HA SAT integration set up on the Wemos keeps sending to the old otgw-<MAC> command topic after the swap, and gives the fixes (carry the unique id over, or remove and re-add the integration with the new device)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edited docs/manuals/nl/h02-hardware-installatie.md and docs/manuals/en/ch02-hardware-setup.md (v1.x upgrade section): two bullets before the unique-id bullet. AC2 refusal checked in code: OTGW-Core.ino:3883 and OTDirect.ino:3259 (TASK-1695) refuse external CS= CH= MM= C2= H2= TT= TC= while SAT is enabled. AC3 topic fact from indigo_light's report (msg 1570535626447782012) plus getUniqueId() = otgw-<MAC> (TASK-1825 notes). Not verified: a HACS SAT integration live against the firmware SAT. Side note: the same section still says 'MQTT topics and the REST API are unchanged from v1.x', which conflicts with the README and BREAKING_CHANGES; left for a separate check.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC3: NL and EN v1.x upgrade sections now say SAT is built in (off by default), not to run the HA SAT integration with the firmware SAT (refusal per OTGW-Core.ino:3883, OTDirect.ino:3259), and how the old otgw-<MAC> command topic breaks a kept integration after the swap, with both fixes. Docs-only; no em dashes; not tested live with HACS SAT.
<!-- SECTION:FINAL_SUMMARY:END -->
