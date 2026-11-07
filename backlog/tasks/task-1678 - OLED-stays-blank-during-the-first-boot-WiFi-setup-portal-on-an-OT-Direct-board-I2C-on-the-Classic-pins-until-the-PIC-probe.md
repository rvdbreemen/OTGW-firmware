---
id: TASK-1678
title: >-
  OLED stays blank during the first-boot WiFi setup portal on an OT-Direct board
  (I2C on the Classic pins until the PIC probe)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-06T21:06:00Z'
updated_date: '2026-11-07T04:02:00Z'
labels:
  - oled
  - wifi
  - bug
dependencies: []
priority: medium
ordinal: 801000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs (#alpha-testing, 2026-11-06 21:04Z, https://discord.com/channels/812969634638725140/1514720723980259460/1568370545224056873): latest beta, full flash erase, first boot: nothing on the OLED, setup AP present. Code reading (dev, not yet reproduced on the bench): on the combo image in auto board mode the pin map defaults to Classic until the PIC probe settles eMode (OTGW-firmware.h comboActivePinMap). initOLED() at OTGW-firmware.ino:355 runs before startWiFi() (:366) on the Classic I2C pins, so on an OT-Direct board the 0x3C probe NACKs and oledPresent stays false; the blocking WiFiManager portal then opens with a dark display, and applyResolvedComboPins() (:466) re-inits the OLED on the right pins only after startWiFi() returns. ADR-201 keeps a no-PIC verdict unpersisted, so this happens on every boot of such a board, visible only while the portal blocks. Fix must not persist the no-PIC verdict (ADR-201).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced on an OT-Direct bench unit after a WiFi wipe: OLED dark while the setup portal is open (old code), telnet/console shows 'OLED: No display at 0x3C'
- [ ] #2 Fix shows the boot splash and the WiFi Setup screen during the portal on an OT-Direct board, and still on a Classic S3 board; ADR-201 gate test stays green
- [ ] #3 tjfs confirms on the next alpha build
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Fix (wt-1678, lane-1678): setup() calls initOLEDBeforeDetect() instead of initOLED() before startWiFi(). It runs initOLED(); on the combo board in auto mode (iBoardMode 0) with the Classic map live, if no display answers it looks once on the OT-Direct I2C pins (PIN_I2C_SDA/SCL 18/17). If found: oledStart(false), no button pinMode (the Classic button alias GPIO 18 is SDA there; loopOLED skips the button until oledButtonAttached), log_e line on the USB console. If not found: Wire goes back to the live map. applyResolvedComboPins() re-inits with the button after detection. Nothing persisted (ADR-201). Chosen over moving the PIC probe before the portal, which would touch the UART-storm order of ADR-125. Verification: host test red/green/mutant; bench .61 with Robert watching the OLED (old beta.116 vs fix, WiFi wipe + re-provision, Robert's OK 2026-11-06).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 22:2x host test test/host/test_oled_before_detect.py (slices the real probeOLED/oledStart/initOLED/initOLEDBeforeDetect from OLED.ino and the real boot call from setup()): origin/dev dc727000e = FAIL 2 (scenario A: display not found, I2C not on 18/17); fix = PASS 10/10; mutant (fallback attaches the button) = FAIL 1 (pinMode on live SDA 18). evaluate.py --quick in wt-1678: Failed 0, health 100%. Build slot and .61 come after 6b's beta.116 build and A/B; number beta.117+ after 6b's push.

2026-11-06 22:3x independent check of the diagnosis: tjfs answered 'yes' (21:17Z, https://discord.com/channels/812969634638725140/1514720723980259460/1568373801845133495) to 'once the gateway joins your WiFi, does the OLED come on?'. That matches the cause: applyResolvedComboPins() re-inits the OLED on the right pins only after startWiFi(). ADR-201 gate tests/test_board_detect_persist_gate.py: Ran 7, OK. GPIO 17 has no function on the Classic S3 Mini map in boards.h (only PIN_I2C_SCL of the OT-Direct map), and GPIO 18 there is the D3 config button (pull-up), so the extra look is a short I2C probe on a free pin and the button pin. Classic leg of AC2: .63 is busy until 08:00 (overnight heap sampler), so not tonight. AC1: the telnet line 'OLED: No display at 0x3C' cannot be captured before WiFi (telnet is not up yet), so the evidence for the old code is Robert's photo of the OLED while the portal is open.

2026-11-07 Robert: .61 has no OLED mounted, so the bench cannot show the screen. Decision (Robert, interactive): prove it in the field (tjfs). No WiFi wipe on .61. The bench does a regression check only: OTA the fix on .61 (OT-Direct, boardmode 0, no OLED). That takes the new path (no display on the Classic pins, none on 18/17 either, Wire back to the live map): the boot must still end in OT-Direct mode with no crash or reboot loop, and oledpresent false. AC1 (bench reproduction) and the OLED part of AC2 are replaced by tjfs's field confirmation (AC3); state that in the final summary.
<!-- SECTION:NOTES:END -->
