---
id: TASK-1871
title: >-
  REST /api/v2/otgw/messages/<id>: tell never seen from unknown or rejected
  (status and last-seen)
status: To Do
assignee: []
created_date: '2026-11-12T18:20:00Z'
updated_date: '2026-11-12T21:04:00Z'
labels:
  - feature
  - rest-api
milestone: 2.0.0
dependencies: []
ordinal: 946000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter thaghostnl (Discord 285886063266889728), #alpha-testing https://discord.com/channels/812969634638725140/1514720723980259460/1570499415284781151 (2026-11-12 18:03:59 UTC). Setup: 2.0.0-beta.170+d260e69, PIC gateway 6.8, OTGW Classic Combo (ESP32-S3), Remeha Avanta 23C + iSense. Observation: GET /api/v2/otgw/messages/10 returns {"label":"TSP","value":0,"unit":""} both before ID 10 was ever requested and after the boiler answered BF00A0000 Unknown-Data-Id (after PM=10); only the live log tells them apart. Suggestion: add a status per ID (never_seen / ack / unknown_dataid / data_invalid) and a last-seen timestamp to the response. Existing: GET /api/v2/otgw/boiler-support (restAPI.ino:787) already lists unsupported_read / unsupported_write from the in-RAM bitmaps; never-seen vs seen and last-seen are missing. Tester feedback: Robert decides (needs-maintainer-ok). Evidence: log excerpt in the post; feature request, no capture needed.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 Robert approved option A (DM 1570541077226717255, relayed by the coordinator): per ID both a last-seen (0 = never seen) and a status (never_seen / ack / unknown_dataid / data_invalid), reusing what /otgw/boiler-support already tracks. needs-maintainer-ok removed. Queued after TASK-1872; needs a firmware build (behind 219 and 220).
<!-- SECTION:NOTES:END -->
