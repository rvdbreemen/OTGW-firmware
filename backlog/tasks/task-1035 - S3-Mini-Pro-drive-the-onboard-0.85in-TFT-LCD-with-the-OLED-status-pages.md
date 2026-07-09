---
id: TASK-1035
title: 'S3 Mini Pro: drive the onboard 0.85in TFT-LCD with the OLED status pages'
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-09 18:37'
updated_date: '2026-07-09 19:35'
labels: []
dependencies: []
ordinal: 244000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer goal (/goal working oled for s3 mini pro, 2026-07-09): the LOLIN S3 Mini Pro's onboard display should show the OTGW status pages. Finding: the onboard display is NOT an I2C SSD1306 OLED but an 0.85-inch 128x128 TFT LCD on SPI (GC9107-class controller); OLED.ino (SSD1306Ascii, I2C 0x3C probe) can never drive it - device correctly reports oledpresent:false (boot: pro=1, IMU alive on I2C 12/11, nothing at 0x3C). Scope: add a display driver for the Pro's TFT, render the existing OLED page content (splash, status pages, portal hint) on it, gated on the Pro board detection (state.hw.bClassicPro). Requires: exact Pro TFT pinout verification (incl. conflict check with Classic-carrier functions - candidate overlap with PIC_RST=GPIO40 must be ruled out), library selection (new dependency -> ADR per project rules, PROGMEM/RAM budget check), and a decision whether the external-I2C-OLED path stays supported alongside.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 TFT pinout verified against LOLIN sources and no conflict with Classic-carrier pin map (or conflicts documented + mitigated)
- [ ] #2 Display library chosen via ADR (alternatives + RAM/flash budget documented)
- [ ] #3 Boot splash + status pages render on the Pro's onboard LCD, gated on Pro detection; non-Pro boards unaffected
- [ ] #4 Field validation on the maintainer's S3 Mini Pro
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 GPIO40 soak test EXECUTED on the maintainer's S3 Mini Pro in the Classic carrier (telnet-toggled 40MHz SPI byte-bursts on the TFT pins SCK=40/MOSI=38/CS=35/DC=36, diagnostic build, since reverted). RESULT: HARD CONFLICT CONFIRMED. With SPI stress ON, ALL PIC traffic stopped (0 poll lines in 25s vs normal 1/s) - and it did NOT recover when the stress stopped: SPI mode-0 leaves SCK (=the PIC MCLR net) idle-LOW, holding the PIC in permanent reset until detectPIC() reclaimed the pin (recovery verified, PIC healthy after). Conclusion: driving the onboard TFT while a PIC operates on the Classic carrier is NOT viable without separating the D5/GPIO40 net - every redraw clocks the MCLR line. Remediation options: (A) hardware mod - isolate the module's D5 pin from the carrier (lift pin/kapton); PIC reset then falls back to GW=R-only, which this week's TASK-972 work proved functional (pinless reset works; detectPIC finds ETX), firmware would set resetPin=-1 when the TFT is active on a Pro; (B) TFT only in OT-Direct/no-PIC modes (defeats the maintainer's use case); (C) abandon. AC#1 satisfied: pinout verified (3 sources) and the conflict is now empirically documented.

2026-07-09 user decision: PARK. The onboard-TFT path requires a physical D5 (GPIO40/SCK, left column 4th-from-top on the S3 Mini Pro header) isolation from the carrier's PIC-reset net — a hardware mod the user is not doing now. GPIO40 soak proved the conflict is fatal (TFT SPI holds the PIC in permanent reset). No firmware work until the hardware path is chosen. External I2C-OLED on the Pro bus (11/12) remains supported today as the alternative.
<!-- SECTION:NOTES:END -->
