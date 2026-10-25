---
id: TASK-1400
title: >-
  ADR: combo board detection persists only a positive PIC verdict and re-probes
  every boot (supersedes ADR-164)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T20:20:00Z'
updated_date: '2026-10-25T20:32:00Z'
labels:
  - adr
  - combo
milestone: 2.0.0
dependencies: []
ordinal: 539000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-25 endorsed the asymmetric design: a found PIC is persisted (iBoardMode 1/3), a miss is never persisted (stays auto, re-probes next boot), forced modes still probe each boot (forced 1/3: detectPIC; forced 2: TASK-1031 recovery probe that corrects to Classic when a PIC answers). This is what the code does since TASK-1031 (2a820ac11; OTGW-firmware.ino:369-451) but Accepted ADR-164 still says 'persist the verdict once, 2 included, no re-detection'. Write a superseding ADR via adr-kit (Proposed), Robert accepts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Proposed ADR via adr-kit that supersedes ADR-164, every claim cited to OTGW-firmware.ino at HEAD, lint passes
- [ ] #2 Robert's explicit OK recorded, then accepted and ADR-164 marked superseded (adr supersede)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 ADR-201 drafted by the adr-generator (Proposed): adr-lint --strict exit 0 (advisory: 2 Open Questions), adr-quality 0.92 (A); guideline-level per ADR-080 (no gate). Code vs brief: only AUTO retries (3x, :425-429); forced 1/3 and forced 2 probe once per boot; a second positive-only persist site at OTGW-Core.ino:5647-5650; stale comments OTGW-firmware.ino:369-374 ('NEVER persists'), :418 ('line 348'), OTGW-firmware.h:750; S3 Mini Pro at mode 2 pulses GPIO 12 (OTGW32 map) instead of its PIC reset GPIO 40 (recovery still works via GW=R); an OTGW32 in auto pays ~3.4 s probing per boot (estimate) and starts pre-detection peripherals on the Classic pin map (OLED portal screen may not show; code reading only). Open Questions for Robert: fix the Pro reset pin (separate task?), add a gate?
<!-- SECTION:NOTES:END -->
