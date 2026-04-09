---
id: TASK-233
title: Multi-zone PID heating control via MQTT room sensors
status: To Do
assignee: []
created_date: '2026-04-09 05:42'
labels:
  - sat
  - future-sprint
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python SAT supports multiple heating zones each with independent thermostat input and PID controller. OT is single-master/single-slave with one room temp (MsgID 24) and one setpoint (MsgID 16). Solution: implement N virtual PIDs via MQTT zone inputs. Each zone pushes room_temp to set/<nodeId>/sat/zone/<n>/room_temp and setpoint to set/<nodeId>/sat/zone/<n>/setpoint. Firmware runs N PID instances and selects the most-demanding CH setpoint (highest demand wins). The boiler still receives one CH setpoint. This gives zone-aware control without multi-thermostat OT.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Add sat_zone_count setting (1-4, default 1)
- [ ] #2 Add MQTT subscriptions for set/<nodeId>/sat/zone/<n>/room_temp and set/<nodeId>/sat/zone/<n>/setpoint for n=1..sat_zone_count
- [ ] #3 Allocate N PID state structs (static array, max 4 zones) in SATcontrol.ino
- [ ] #4 Zone selector: most-demanding setpoint wins (highest CH setpoint among active zones)
- [ ] #5 Publish per-zone diagnostics: sat/zone/<n>/output, sat/zone/<n>/error, sat/zone/<n>/active
- [ ] #6 Zone times out after sat_zone_timeout_s without update (default 300s)
<!-- AC:END -->
