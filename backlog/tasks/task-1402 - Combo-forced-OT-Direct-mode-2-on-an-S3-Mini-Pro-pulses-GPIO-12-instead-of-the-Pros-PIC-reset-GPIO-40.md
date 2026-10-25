---
id: TASK-1402
title: >-
  Combo: forced OT-Direct (mode 2) on an S3 Mini Pro pulses GPIO 12 instead of
  the Pro's PIC reset GPIO 40
status: To Do
assignee: []
created_date: '2026-10-25T20:37:00Z'
labels:
  - combo
  - pic
milestone: 2.0.0
dependencies: []
ordinal: 541000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-201 Open Question 1, Robert 2026-10-25: yes, fix. comboActivePinMap() returns the OTGW32 map for mode 2 (OTGW-firmware.h:768), so activePicRst() gives GPIO 12 (boards.h:264); on a Pro GPIO 12 is I2C SDA (boards.h:290) and the PIC reset is GPIO 40 (boards.h:288). The TASK-1031 recovery probe still works today only because resetPic() also sends GW=R over the UART (OTGWSerial.cpp:931-946). Fix: the recovery probe in forced mode 2 uses the Classic reset pin of the detected module (IMU probe says Pro or not).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Forced mode 2 recovery probe on a Pro pulses GPIO 40, not GPIO 12 (code + host or bench evidence)
- [ ] #2 Plain S3 Mini and OTGW32 behaviour unchanged
<!-- AC:END -->
