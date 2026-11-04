---
id: TASK-1069
title: >-
  feat-2.0.0: port TASK-1066 — four vh_* status bits never republish (StatusVH
  slot collision)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-08 15:43'
updated_date: '2026-10-03 10:15'
labels:
  - bug
  - mqtt
dependencies: []
priority: medium
ordinal: 256000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x TASK-1066, verified present on this branch: publishStatusVHBitMQTT is called with slots 0,1,2,3 for the master (HB) bits AND slots 0,1,2,3,4,6 for the slave (LB) bits, so slots 0-3 are used twice. mqttlastsentstatusvhbit[] documents its own contract as 'slots 0-7=master, 8-15=slave' and the OT_Statusflags fan-out honours that, using 8-15 for slave bits. The master fan-out stamps slots 0-3 microseconds before the slave fan-out reads them, so elapsedTrackedSeconds is ~0 and the 60s heartbeat never elapses. Consequence: vh_fault, vh_ventilation_mode, vh_bypass_status and vh_bypass_automatic_status publish only on first-seen or on a force, never on their interval. vh_free_ventliation_status (slot 4) and vh_diagnostic_indicator (slot 6) are unaffected because they have no master counterpart, which is the signature that identifies the bug. Fix mirrors 1.x commit ef12138cf: slave bits move to 8,9,10,11,12,14.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 StatusVH slave bits use slots 8-14, matching the declared contract and the OT_Statusflags fan-out
- [x] #2 No duplicate slot index remains across the master and slave StatusVH fan-outs
- [x] #3 Build green for the relevant esp32 targets, verified on artifact freshness and the per-env SUCCESS line
- [x] #4 python evaluate.py --quick shows no new failures
- [x] #5 Behaviour matches the otgw-1.x.x implementation
- [x] #6 On-device verification that all four topics republish on their heartbeat (blocked: needs ESP32 hardware)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-25 backlog sweep: code is implemented and committed on this branch; verified by git rather than by the task file. TASK-1068 and TASK-1069 both landed in a7e06f8df; TASK-1052's shim is at platform_esp32.h:234 with the call at networkStuff.ino:92. Every AC except the on-device one is met.

Left In Progress deliberately. The remaining AC needs ESP32 hardware in the loop, which no amount of code reading can substitute for, and flipping the task to Done would claim a verification that never happened.

AC#6 ON-DEVICE, 2026-10-03, OTGW32 192.168.88.61 running 2.0.0-alpha.404+1d9ae71 (contains the slot fix ca55863bb: slave bits on slots 8-14, OTGW-Core.ino:2571-2576). MQTT to the Docker test-rig mosquitto (192.168.88.32), legacy topic names on.
Method: a 2-line /otgw_simulation.log fixture, looped by the replay every 1.5 s: T80460F00 (READ_DATA MsgID 70, master bits HB 0x0F) and B40460F5F (READ_ACK MsgID 70, slave bits LB 0x5F). The original fixture was restored afterwards (sha256 6f88a78ea743fd57 verified).
Captured with mosquitto_sub on OTGW/value/otgw-1020BA21B4F8/# for 215 s, 40 vh_* messages. Times are relative to the first message:
- master: vh_ventilation_enabled, vh_bypass_position, vh_bypass_mode and vh_free_ventilation_mode at t=2, 62, 123, 183 s.
- slave: vh_fault, vh_ventilation_mode, vh_bypass_status, vh_bypass_automatic_status (the four that used to share slots 0-3 with the master), plus vh_free_ventliation_status and vh_diagnostic_indicator, at t=3, 63, 124, 184 s.
So every topic republishes on the hardcoded 60 s heartbeat (gaps 60/61/60 s), the four slave topics on their own cadence. All payloads ON, as injected.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported 1.x TASK-1066. The StatusVH slave bits (MsgID 70 LB) used tracker slots 0-6, which collided with the master bits on slots 0-3, so four slave topics never republished on their heartbeat. They now use slots 8-14, as mqttlastsentstatusvhbit[] documents (commit ca55863bb).
Evidence:
- AC#1-#5: slot indices checked, build and evaluate green at the time; behaviour matches otgw-1.x.x.
- AC#6 (2026-10-03, OTGW32 bench, alpha.404, MQTT to the Docker test-rig): a looped MsgID 70 replay (T80460F00 / B40460F5F) was run for 215 s. All ten vh_* topics, the four formerly colliding slave topics included (vh_fault, vh_ventilation_mode, vh_bypass_status, vh_bypass_automatic_status), published at the start and then every 60 s (gaps 60/61/60 s). The fixture was restored (sha256 verified).
<!-- SECTION:FINAL_SUMMARY:END -->
