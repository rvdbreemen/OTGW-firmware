---
id: TASK-1432
title: 'Fix: two dead links in the manual (GH #689)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T20:31:00Z'
updated_date: '2026-10-28T22:02:00Z'
labels:
  - bug
  - docs
dependencies: []
references:
  - https://github.com/rvdbreemen/OTGW-firmware/issues/689
priority: low
ordinal: 594000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
keyral (GH #689, 2026-10-27): https://github.com/mvdwetering/hass-otgw and https://github.com/JAndrassy/EthernetESP32 return 404 (re-verified 2026-10-28 with curl). Occurrences: docs/MANUAL.md:3567, :3591 and docs/manuals/en/ch10-appendix.md:211, :243 (dev only; not present in otgw-1.x.x docs). Replace with the current repo URLs or remove.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No 404 link to hass-otgw or EthernetESP32 remains in docs/ (curl check of every replaced URL returns 200)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced the two dead links reported on GH #689 (keyral) in docs/MANUAL.md and docs/manuals/en/ch10-appendix.md.

- mvdwetering/hass-otgw (404, repository gone): replaced by the built-in Home Assistant OpenTherm Gateway integration, https://www.home-assistant.io/integrations/opentherm_gw/ (connects to port 25238). The "(HACS)" label is dropped.
- JAndrassy/EthernetESP32 (404): the library moved to https://github.com/Networking-for-Arduino/EthernetESP32 (contributor JAndrassy, latest release 1.0.2, the version the firmware uses).

Evidence: curl -L on both new URLs returns 200; grep for hass-otgw and JAndrassy/EthernetESP32 under docs/ returns 0 hits. Docs-only, no firmware change.
<!-- SECTION:FINAL_SUMMARY:END -->
