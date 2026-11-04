---
id: TASK-1402
title: >-
  Combo: forced OT-Direct (mode 2) on an S3 Mini Pro pulses GPIO 12 instead of
  the Pro's PIC reset GPIO 40
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T20:37:00Z'
updated_date: '2026-11-04T07:08:00Z'
labels:
  - combo
  - pic
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 541000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-201 Open Question 1, Robert 2026-10-25: yes, fix. comboActivePinMap() returns the OTGW32 map for mode 2 (OTGW-firmware.h:768), so activePicRst() gives GPIO 12 (boards.h:264); on a Pro GPIO 12 is I2C SDA (boards.h:290) and the PIC reset is GPIO 40 (boards.h:288). The TASK-1031 recovery probe still works today only because resetPic() also sends GW=R over the UART (OTGWSerial.cpp:931-946). Fix: the recovery probe in forced mode 2 uses the Classic reset pin of the detected module (IMU probe says Pro or not).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Forced mode 2 recovery probe on a Pro pulses GPIO 40, not GPIO 12 (code + host or bench evidence)
- [x] #2 Plain S3 Mini and OTGW32 behaviour unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: mode 2 resolves the OTGW32 map, activePicRst() = GPIO 12 (OTGW-firmware.h:781/845); TASK-1552 boardPinReserved only guards user pins. Code doable, bench needs a Mini Pro.

2026-11-04 (sonnet agent, lead reran on the integration tree): picRstPinFor() in OTGW-firmware.h; activePicRst() returns 40 for mode 2 + Pro (IMU), 12 otherwise; comboActivePinMap untouched. tests/test_pic_rst_pin.py 3 OK (9 cases), mutant without the mode-2 clause red; old header lacks the helper. board_detect_persist_gate 7 OK, board_pin_reserved 5 OK, evaluate 0 failed. Tag beta.63. Bench needs an S3 Mini Pro on a Classic carrier with boardmode=2 (neither bench board is one).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Forced OT-Direct (mode 2) on an S3 Mini Pro resets the PIC on GPIO 40 (picRstPinFor). Evidence: tests/test_pic_rst_pin.py 9 cases incl. plain Mini and OTGW32 unchanged (12), mutant red; beta.63+ boots normally on the plain S3 Mini Classic unit (PIC detected, boot +2 per flash) and the OTGW32 unit. Not bench-tested on an S3 Mini Pro (none on the bench).
<!-- SECTION:FINAL_SUMMARY:END -->
