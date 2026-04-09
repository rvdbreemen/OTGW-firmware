---
id: TASK-146
title: 'OTGW32-Audit-2D: Audit PR= query registers (all 17 sub-registers)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:18'
updated_date: '2026-04-08 22:34'
labels:
  - audit
  - otgw32
  - phase-2
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify all 17 PR= query sub-registers against PIC firmware specification. Each register must return a correctly formatted response via processOT(). Check: A (banner), M (mode), O (setpoint override), S (setback temp), W (DHW override), G (GPIO), I (input flags), L (LED functions), T (transitions/override HB), D (temp sensor), P (power level), R (detection), B (build date), C (clock speed), Q (reset reason), N (message interval), V (voltage reference).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PR=A returns banner containing 'OpenTherm Gateway'
- [x] #2 PR=M returns correct mode character (G/M/P/S/L)
- [x] #3 PR=O returns T/C/N prefix with active override value
- [x] #4 PR=S returns current setback temperature
- [x] #5 PR=W returns A/1/0 for DHW override state
- [x] #6 PR=G returns 2-char GPIO function string
- [x] #7 PR=I returns input flags (00 stub is acceptable)
- [x] #8 PR=L returns 6-char LED function string
- [x] #9 PR=T returns IT/OH values in correct format
- [x] #10 PR=D returns temperature sensor function char
- [x] #11 PR=P returns power level (M stub acceptable)
- [x] #12 PR=R returns detection method (I stub acceptable)
- [x] #13 PR=B returns build date string
- [x] #14 PR=C returns clock speed (240 for ESP32-S3)
- [x] #15 PR=Q returns correct reset reason char (P/C/W/B/E)
- [x] #16 PR=N returns message interval in centiseconds
- [x] #17 PR=V returns voltage reference digit
- [x] #18 Any deviation from PIC spec results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit of all 17 PR= sub-registers complete.

PR=A: PASS - synthesizes "PR: A=OpenTherm Gateway OTGW32" via processOT(). Tools that check for "OpenTherm Gateway" string will match.
PR=M: PASS - returns G/M/P/S/L (G=gateway, M=monitor, P=bypass/passthru, S=master/standalone, L=loopback). Correct mode chars.
PR=O: PASS - checks otOverrides for MsgID 1 (CS) and MsgID 16 (TT). Returns T<val> for TT, C<val> for CS, N for none.
PR=S: PASS - returns otSetbackTemp as f8.8 string.
PR=W: PASS - returns A (0xFF), 1, or 0 from otDHWOverride.
PR=G: PASS - returns otGpioFunctions[2] (2-char string).
PR=I: PASS - stub "00", acceptable per spec.
PR=L: PASS - returns otLedFunctions[7] (6-char string).
PR=T: PASS - returns IT/OH values formatted as "%d/%d". Correct.
PR=D: PASS - returns otTempSensor char.
PR=P: PASS - stub "M" (medium power), acceptable.
PR=R: PASS - stub "I" (internal detection), acceptable.
PR=B: PASS - returns __DATE__ (build date string).
PR=C: PASS - "240" for ESP32-S3 at 240MHz.
PR=Q: PASS - maps ESP32 reset reasons to P/C/W/B/E via esp_reset_reason().
PR=N: PASS - returns otMinIntervalMs/10 (centiseconds). 100ms -> 10, 1000ms -> 100.
PR=V: PASS - returns otVoltageRef digit.

All 17 registers verified.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All 17 PR= sub-registers (A,M,O,S,W,G,I,L,T,D,P,R,B,C,Q,N,V) verified correct.

Highlights:
- PR=A includes "OpenTherm Gateway" in banner string (tool-compatible).
- PR=M returns correct mode char for all 5 modes (G/M/P/S/L).
- PR=O correctly distinguishes TT (temporary) vs CS (constant) overrides.
- PR=Q uses esp_reset_reason() for PIC-compatible reset codes.
- PR=C returns 240 (ESP32-S3 clock speed).
- Stubs for PR=I, PR=P, PR=R are acceptable per spec.

No deviations.
<!-- SECTION:FINAL_SUMMARY:END -->
