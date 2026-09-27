---
id: TASK-1166
title: 'Fix: corrupted MQTT payload bytes on the wire (TSet published as 10.\xe0\x00)'
status: To Do
assignee: []
created_date: '2026-09-26 15:04'
updated_date: '2026-09-27 07:37'
labels:
  - bug
  - mqtt
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 235000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mrfox7688 (GH #682, 2026-09-26, on 1.7.6-beta.5) saw Home Assistant reject a payload on OTGW/value/<id>/TSet: b'10.à
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The code path that publishes TSet is traced from formatting to write, with file:line, and every buffer it passes through is shown to outlive the write (or the defect is identified)
- [ ] #2 The failure is reproduced on the bench or the mechanism is demonstrated in a host test, before any fix
- [ ] #3 Fix verified: after the fix, a forced short write/retry on the payload path cannot put bytes on the wire that differ from the formatted payload
- [ ] #4 Reporter informed on GH #682
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-26: asked mrfox7688 on GH #682 for device/info counters (desync_drops, sndbuf_skips, uptime, fwversion), recurrence and topics (HA log "Can't decode payload"), the Mosquitto log around 10:45:47, and a capture-otgw.sh transcript.

2026-09-26 19:56 UTC: mrfox7688 answered on GH #682 (1.7.6-beta.5+58d490d, uptime 1d 00:36):
- mqtt_sndbuf_skips 5548 (~3.8/min, up from ~1.6/min reported earlier), mqtt_desync_drops 13.
- 3 more "Can't decode payload" hits 18:09-18:42, all b'Oà
<!-- SECTION:NOTES:END -->
