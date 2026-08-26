---
id: TASK-1092
title: >-
  Fix: DHW flow rate entity rejected by HA - unit l/min invalid for device_class
  volume_flow_rate
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-26 19:48'
updated_date: '2026-08-26 19:55'
labels:
  - bug
dependencies: []
priority: high
ordinal: 191000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Regression introduced in v1.7.5-beta.2. The DHWFlowRate discovery config gained device_class volume_flow_rate (GH #675) but kept the unit string 'l/min'. Home Assistant validates unit against device class and rejects the config: "The unit of measurement l/min is not valid together with device class volume_flow_rate". HA's UnitOfVolumeFlowRate.LITERS_PER_MINUTE is 'L/min' with a capital L. Net effect: a sensor that worked in v1.7.4 is unavailable in beta.2. Reported by Jeroenll on GH #675 with the full HA error log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 haUnitStr(HaUnit::l_min) returns 'L/min'
- [ ] #2 Home Assistant accepts the DHWFlowRate discovery config with no unit/device_class error in the log, and the sensor shows a value again
- [x] #3 No other entity changes unit: HaUnit::l_min is used only by the two MsgID 19 entries
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Change the PROGMEM string for HaUnit::l_min from 'l/min' to 'L/min' in mqtt_configuratie.cpp haUnitStr().
2. Leave OTGW-Core.h:373 ('l/min') untouched: that is the OT log / REST display label, not an HA discovery field.
3. Verify HaUnit::l_min has no other users (grep: only the two MsgID 19 sensor entries).
4. Build firmware + evaluator, commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Root cause: HA validates unit_of_measurement against device_class. UnitOfVolumeFlowRate.LITERS_PER_MINUTE is 'L/min'; the discovery config emitted 'l/min', so HA discarded the whole config and the sensor went unavailable. Introduced when device_class volume_flow_rate was added in v1.7.5-beta.2 (GH #675); in v1.7.4 the entity carried no device_class, so the same unit string was accepted.
- Fix: one PROGMEM string in haUnitStr(). HaUnit::l_min has exactly two users, the two MsgID 19 sensor entries (mqtt_configuratie.cpp:714-715), so no other entity changes unit.
- OTGW-Core.h:373 keeps lowercase l/min on purpose: that is the OT log and REST display label, not an HA discovery field.
- Build: build.bat green, 'Build completed successfully!', sketch 761208 bytes (72%); firmware + littlefs binaries fresh. evaluate.py --quick: 37 checks, 35 passed, 0 failed, health 100%.
- AC #2 needs a real HA instance: reporter confirmation on GH #675 after the next beta.
<!-- SECTION:NOTES:END -->
