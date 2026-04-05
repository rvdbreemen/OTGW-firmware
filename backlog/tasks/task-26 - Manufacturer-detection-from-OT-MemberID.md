---
id: TASK-26
title: Manufacturer detection from OT MemberID
status: To Do
assignee: []
created_date: '2026-04-05 20:46'
updated_date: '2026-04-05 20:55'
labels:
  - sat
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Identify boiler brand from OpenTherm MemberID (MsgID 3 slave). SAT Python (manufacturer.py) maps MemberIDs to 18 brands and applies brand-specific quirks (e.g. Geminox: min modulation 10%). Note: vendor MemberID assignment has been inconsistent over time, so detection is informational rather than reliable.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/manufacturer.py
Known quirk: Geminox needs min MM=10 (heating_control.py:522)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Read OT MemberID from MsgID 3 slave response
- [ ] #2 Map MemberID to known manufacturer names (best-effort)
- [ ] #3 Apply brand-specific quirks where needed (e.g. Geminox min MM=10)
- [ ] #4 MQTT publish: detected manufacturer name
- [ ] #5 WebUI: show detected manufacturer in device info
- [ ] #6 Geminox quirk: enforce minimum modulation of 10% (never send MM=0)
- [ ] #7 Immergas quirk: send TP=11:12=<value> alongside MM=, cap at 80%
- [ ] #8 Ideal/Intergas/Geminox/Nefit: supports_relative_modulation=False, skip modulation-based decisions (coordinator/__init__.py:310)
- [ ] #9 Ideal/Intergas/Nefit: send MI=500 on boot for faster OT polling (opentherm.py:171)
- [ ] #10 All manufacturers: boot sequence sends PM=3, PM=15, PM=48 priority messages (opentherm.py:167-169)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Vendor quirks found in SAT Python thermo-nova:
- Geminox: min modulation 10% (heating_control.py:522)
- Immergas: extra TP=11:12 command + cap at 80% (opentherm.py:216-222)

Additional vendor quirks found (2026-04-05):
- Ideal/Intergas/Geminox/Nefit: no modulation reading support (coordinator/__init__.py:310)
- Ideal/Intergas/Nefit: MI=500 boot command (opentherm.py:171)
- All: PM=3,15,48 boot sequence (opentherm.py:167-169)

File references in other-projects/SAT-releases-thermo-nova:
- Manufacturer base class: custom_components/sat/manufacturer.py
- Manufacturer implementations: custom_components/sat/manufacturers/ (18 brands)
- Geminox min MM=10: custom_components/sat/heating_control.py:522
- Immergas TP=11:12: custom_components/sat/coordinator/mqtt/opentherm.py:216-222
- No modulation support: custom_components/sat/coordinator/__init__.py:310
- MI=500 boot: custom_components/sat/coordinator/mqtt/opentherm.py:171
<!-- SECTION:NOTES:END -->
