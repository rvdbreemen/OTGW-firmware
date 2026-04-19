---
id: TASK-334
title: 'ADR-079 audit: move NtpStatus_t enum from networkStuff.h to NTPtypes.h'
status: To Do
assignee: []
created_date: '2026-04-19 18:34'
labels:
  - architecture
  - adr-079
  - cleanup
  - review-2026-04-18-followup
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
networkStuff.h contains a NtpStatus_t enum which is NTP-domain and belongs in NTPtypes.h per ADR-079. Small single-enum move; networkStuff.h becomes pure network transport machinery without NTP-specific type leakage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 NtpStatus_t enum moves to NTPtypes.h with the NTPSection struct
- [ ] #2 networkStuff.h contains no NTP-specific type declarations
- [ ] #3 Both platforms build clean; all consumers of NtpStatus_t still find it via the OTGW-firmware.h include chain
<!-- AC:END -->
