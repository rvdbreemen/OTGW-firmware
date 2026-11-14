---
id: TASK-1848
title: 'OT-Direct: PR= reports that are not the PIC''s text'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T00:02:00Z'
updated_date: '2026-11-14T15:56:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 927000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N14), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

| Letter | PIC | OT-Direct |
|---|---|---|
| P | Low power / Medium power / High power (G:2261-2268, :5499-5502) | fixed P=M (O:3508-3511); library knows the level (OpenTherm.cpp:540-544) |
| R | D by default; S, R, C, I (G:2270-2280) | fixed R=I (= iSense) (O:3512-3515); FT= ignored |
| O | lowercase t/c until thermostat accepted; n while clearing (G:2201-2226) | always uppercase (O:3446-3475) |
| C | C=4 MHz (G:5506-5507) | C=240 (O:3520-3523) |
| B | HH:MM DD-MM-YYYY (tools/build.tcl:83) | __DATE__ (O:3516-3519) |
| L | default FXOMPC (G:5462-5468) | default XXXXXX (O:537) |
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 host test per letter, byte-exact; red on current code
- [ ] #2 PR=P follows the real power level
- [ ] #3 decision for C, B and L recorded (PIC text or agreed OT-Direct value)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): which clients parse P, R, C, B; pyotgw only stores them.

Part of the PIC 6.8 parity audit, parent TASK-1851.

2026-11-14 (95) decisions, Robert by DM via the coordinator: Q1 A (DM 1571191256833331375): PR=C '240 MHz' (PIC format SpeedString gateway.asm 6.8:5506-5507 with the S3 clock), PR=B the build time as 'HH:MM DD-MM-YYYY' (PIC build stamp format, otgw-6.8 tools/build.tcl:83), PR=L default 'FXOMPC' (PIC default :5462-5468; the old XXXXXX meant six transmit LEDs). Q2 A (DM 1571191595498340544): PR=O follows PrintOverride (:2201-2226) and covers only TT=/TC=; a CS= override alone reports N (CS= never sets the PIC override state, SetSetPoint :4095). Consumers named in the question: MQTT otgw-pic/settings/setpoint_override, HA sensor PIC_Setpoint_Override, REST /api/v2/pic/settings setpoint_override, web UI PIC panel (fmtPicSetting v2.js:6300-6304, which shows lowercase t/c and n raw; TASK-1939 filed for 09). SAT does not read it. CHANGELOG line added.

Findings: (1) OpenTherm library getSmartPowerState() (OpenTherm.cpp:540-544) returns LOW or HIGH, never MEDIUM; not patched (no library changes): OTDirect.ino derives OTDSlaveLine from OpenTherm and reads the protected txIdleLevel/rxIdleLevel, mapped as PrintPowerlevel :2262-2268 (tx=TransmitMode, rx=ReceiveModeT). (2) FT= is stored as the model letter PR=R prints (C, I, S by hand, else D; SetTStatModel :4705-4727); OT-Direct has no Remeha detection so R never appears. AC2 'follows the real power level' is host-tested on the mapping only; a Smart Power thermostat on the bench would be needed for the hardware path.
<!-- SECTION:NOTES:END -->
