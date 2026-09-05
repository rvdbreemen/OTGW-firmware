---
id: TASK-1127
title: 'Diagnose screen: show and drive the diagnose PIC from the web interface'
status: Done
assignee:
  - '@claude'
created_date: '2026-09-05 07:30'
updated_date: '2026-09-05 10:35'
labels:
  - feature
dependencies: []
priority: high
ordinal: 215000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request, 2026-09-05: when the PIC runs diagnose firmware, the web interface must offer a dedicated screen that shows the diagnose output and lets you interact with it, so no extra tool (OTmonitor, telnet) is needed. The screen appears automatically once diagnose firmware is detected.\n\nDetection already exists and needs nothing new: OTGWSerial::matchBanner() sets the typed firmware enum plus version and processor from the PIC banner, for all three firmware types. Verified live on the bench unit, which reports picfwtype diagnose, picfwversion 2.2, picdeviceid pic16f1847. Use OTGWSerial.firmwareType(), not the state.pic.sType string projection.\n\nByte transparency on port 25238 already ships (TASK-1109, v1.7.5), so the menu and the terminator-less prompt reach a connected client today. What is missing is the browser path: raw PIC output into the page, and single keystrokes back out.\n\nDesign basis: docs/plan/DIAGNOSE_FIRMWARE_SUPPORT_PLAN.md stages 1 to 3. Key constraints from that analysis: the command queue cannot carry a keystroke because sendOTGW() appends CR+LF below every validator, so keystrokes need their own direct-to-UART writer; and the passthrough must keep reading through OTGWSerial::read() so matchBanner() still runs, otherwise the ESP never notices when the user reflashes gateway.hex.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A dedicated diagnose screen exists in the web interface as its own page section, and does not disturb the existing sections
- [x] #2 The screen appears automatically when the PIC is identified as diagnose firmware, and is absent for gateway and interface firmware
- [x] #3 Raw PIC output, including a prompt that carries no line terminator, is visible in the browser without any external tool
- [x] #4 Single keystrokes can be sent from the browser to the PIC without a trailing CR or LF being appended, and Enter can be sent as its own explicit key
- [x] #5 The ESP still detects a firmware type change, so reflashing gateway.hex from the same screen returns the device to normal operation without a reboot
- [x] #6 python build.py exits 0 with a per-target success line, python evaluate.py --quick shows no new failures, and the screen is verified on the bench unit running diagnose 2.2
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-05: measured the real diagnose menu on the bench unit (192.168.88.68, diagnose 2.2) over port 25238, which is byte-transparent since v1.7.5. Five of the design plan's open questions are now answered from hardware rather than from a single account, and one answer contradicts the brief.

1. THE MENU IS LINE-BASED, NOT PER-KEYSTROKE. Sending 1, then 4, then 6 without CR produced only the echoes "1", "4", "6" and no action; the following CR returned "Invalid test", because the PIC had buffered "146" and evaluated it as one number. The prompt "Enter test number: " reads a LINE. This is the opposite of what the brief assumed, and it means menu navigation does not need a single-keystroke channel at all.

2. INSIDE A RUNNING TEST, KEYS ARE CONSUMED SILENTLY. After "1" + CR the LED test starts and emits nothing. Subsequent "4" and "6" produced ZERO bytes back: no echo, no output. So the single-keystroke channel IS needed, but only while a test is running, and the user gets no feedback from the PIC for those keys.

3. THE PIC ECHOES AT THE MENU. "0" came back as "0". The browser must NOT add local echo at the menu or every character doubles. Inside a test there is no echo, so the UI has to represent those keypresses itself.

4. ENTER IS A BARE CR (0x0D). It both submits a menu choice and exits a running test. No LF needed.

5. THE BANNER IS REPRINTED ON EVERY MENU REDRAW, and that costs. Verified against the telnet debug log: each redraw re-fires fwreportinfo(), which calls sendMQTTversioninfo(). Two redraws in one 22 s window produced two full callback sequences. Every Enter in the diagnose screen would therefore publish MQTT version info. This needs throttling and it is firmware work beyond the screen itself.

6. Invalid input yields 0x04 (ETX) followed by "Invalid test" and a full banner+menu+prompt redraw. The 0x04 is the same byte detectPIC() probes for, which is consistent with the probe succeeding against diagnose.hex.

7. On connect the PIC sends NOTHING. It sits silently at its prompt. The screen therefore cannot rely on passively waiting for output; it needs to send a CR to provoke a redraw when it opens, or accept a blank pane until the user acts.

2026-09-05: implemented and verified on the bench unit (192.168.88.68, diagnose 2.2), firmware 1.7.6-beta.1+e353cd4.

What shipped:
- New page section displayDiagnosePage, nav entry class diagnose-only, hidden by default and unhidden by applyDiagnoseAvailability() only when picfwtype is exactly "diagnose". Auto-navigates once per page load on first detection.
- Output: forwardDiagnoseChunk() taps the passthrough buffer that already exists for port 25238 and mirrors it to the OT-log WebSocket with an STX prefix. No second server, no extra static RAM. Non-printable bytes are dropped so the frame stays valid UTF-8; the 0x04 the PIC sends before a redraw is protocol noise on a text screen.
- Input: POST /api/v2/otgw/diagnose writes bytes to the UART verbatim, gated positively on firmwareType() == FIRMWARE_DIAG with 409 otherwise. It cannot use the command queue: sendOTGW() appends CR+LF below every validator, and an appended Enter would end a running test.
- MQTT throttle in fwreportinfo(), publishing only on a real change.

Browser verification with Playwright against the live device:
- Loading http://192.168.88.68/ lands directly on the diagnose screen, no click needed.
- "Redraw menu" produced the full menu in the pane.
- Typing 5 and pressing Enter ran the Voltage levels test and returned real readings: Power supply 3.302, Reference 1.473, Thermostat 2.921, Boiler 0.000, then the sub-prompt "Reference voltage setting (0..9) [5]:".
- The typed 5 appears ONCE, confirming the decision not to add local echo is right.
- The Enter key on the keypad left the sub-prompt and returned to the menu.
- Only console error is a 429 on the rate-limited device/time poll, unrelated to this feature.

End-to-end transport check outside the browser: one CR posted to the endpoint came back as 46 STX-prefixed WebSocket frames reassembling into the complete menu including the terminator-less prompt.

Throttle check: three menu redraws produced three fwreportinfo callbacks and three "MQTT version publish skipped" lines, so zero MQTT traffic where there would previously have been three full version publishes.

Build trap worth recording: the first build exited 0 while having failed, leaving build/ with no .ino.bin at all. The real cause was that OTGW_PASSTHRU_CHUNK was defined INSIDE handleOTGW(), below the new helper. Hoisted to file scope; the literal ADR-095 requires is still in the file.

AC #5 IS NOT CHECKED. It requires reflashing gateway.hex to prove the ESP notices the firmware type changing back, and flashing the PIC needs explicit per-instance authorisation. The mechanism is in place and source-verified: the passthrough still reads through OTGWSerial::read(), so matchBanner() keeps running and a gateway banner would re-type the PIC. It is the proof that is missing, not the code.

AC #2 second half is code-verified rather than hardware-verified: the nav entry carries "hidden" in the markup and is only unhidden for picfwtype "diagnose", but no gateway PIC was available to observe its absence.

2026-09-05: full round trip verified on hardware with maintainer authorisation to flash the PIC. gateway 6.8 -> diagnose 2.2 -> gateway 6.8, driven entirely from the web interface.

Evidence:
- On gateway 6.8: main page active, diagnoseAvailable false, all six diagnose nav entries hidden, diagnose section not rendered.
- Flash to diagnose 2.2: the diagnose screen comes up by itself, and test 5 returns live readings (Power supply 3.302, Reference 1.473, Thermostat 2.921, Boiler 0.000) with the typed digit echoed exactly once.
- Flash back to gateway 6.8 from that same screen: after the ten second timer the UI lands on the main page, diagnoseAvailable false, nav entries hidden again, OpenTherm traffic flowing in the log.

AC #5 is now checked: the ESP re-detected the firmware type in both directions and the UI followed, so a user who flashes diagnose can always get back to gateway from the same screen.

Two real defects were found by doing this rather than by reasoning about it, both now fixed:

1. TWO COMPLETION PATHS. handleFlashCompletion() is the HTTP-polling failsafe, but handleFlashMessage() carried an inline copy of the same logic for the WebSocket "end" state. The WebSocket wins the race, so the copy was the path that actually ran and it never learned about the post-flash switch. The copy is gone; that branch delegates to handleFlashCompletion().

2. THE SWITCH FETCHED /device/info AND THE REQUEST STALLED. Right after a PIC flash the ESP is re-detecting the banner and rate limiting the UI polls. A pending fetch has no timeout and reaches neither .then nor .catch, so the timer expired and nothing happened, with no trace in the console. It now decides from fwInfo.type, which is synchronous and cannot hang, and a wrong guess self-heals because refreshFirmware() and the regular device-info poll both correct applyDiagnoseAvailability() shortly after.

A third thing cost real time and is worth recording as a development gotcha rather than a defect: index.js is cache-busted with ?v=<githash>, taken from data/version.hash. Iterating WITHOUT committing leaves that key unchanged, so the browser keeps serving its cached copy even though the device has the new file. Two apparent test failures were this. Verified by reading handleFlashCompletion.toString() in the page and finding the old body while curl showed the new one on the device. Commit before browser-testing a JS change, or the browser lies to you.

One known rough edge, not a regression and not fixed: deep-linking to #tabPICflash on a diagnose PIC loses a race against the once-per-load auto-show and lands on the diagnose screen instead. The PIC firmware page stays reachable through Advanced, which is how the flash back to gateway was performed, so nothing is unreachable.

2026-09-05: Reworked the on-screen explanation after Schelte Bron reported it was only true for test 1.
- Replaced the single paragraph with three short ones plus a link to https://otgw.tclcode.com/diagnose.html. No table, per maintainer instruction to keep the web app simple.
- Deliberately states no ending for tests 6 and 7: the source page does not, and Schelte's summary sentence is contradicted by the page for test 5.
- Dropped the old "press single keys during a test" instruction; every send path here appends CR and the digit buttons were removed, so that advice was unusable.
- Verified on 192.168.88.68 (1.7.6-beta.1+d3a4952): copy renders, link has target=_blank rel=noopener.
- Round trip re-run: PIC was found on diagnose 2.2 (menu answered live over the WS bridge), flashed back to gateway.hex 6.8, flash-status reported success, UI returned to the normal home screen with diagnoseAvailable=false, 0 of 6 diagnose tabs visible, and live OT traffic.
- Copy checked against the REAL menu: 1 LED test, 2 Bit timing thermostat, 3 Bit timing boiler, 4 Delay symmetry, 5 Voltage levels, 6 Idle times, 7 Temperature sensor.
- Cosmetic, pre-existing: "Redraw menu" sends a bare CR, so the PIC answers "Invalid test" before redrawing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The web interface can now show and drive a PIC running diagnose firmware, so no external tool is needed.

On a diagnose PIC the diagnose screen IS the home screen: the regular home page is built around OpenTherm telemetry that a diagnose PIC does not produce, so it would sit there empty. The redirect lives inside showMainPage() rather than at its call sites, so the Home button, the post-flash switch and the error paths all agree. On a gateway PIC nothing changes and every diagnose element stays hidden.

The design follows a measurement of the real menu, and one finding contradicted the brief: the menu is LINE based, not per keystroke. Sending 1, 4, 6 without Enter produced only echoes and then "Invalid test", because the PIC had buffered "146". Inside a running test the opposite holds, keys are consumed silently with no echo. Hence a text box for the menu and an Enter key that sends a bare CR, which both submits a choice and leaves a test. The digit keypad was dropped at the maintainer's request once the line-based behaviour made it redundant.

Output mirrors the existing port 25238 passthrough onto the OT-log WebSocket with an STX prefix, so there is no second server and no extra static RAM. Input is POST /api/v2/otgw/diagnose, which writes bytes verbatim and is gated positively on FIRMWARE_DIAG with 409 otherwise; it cannot use the command queue, because sendOTGW() appends CR+LF below every validator and an appended Enter would end the test just started.

One firmware fix was required rather than optional: the diagnose PIC reprints its banner on every menu redraw, so every keypress re-entered fwreportinfo() and published MQTT version info. It now publishes only on a real change.

Verified end to end on hardware across a full gateway -> diagnose -> gateway round trip driven from the browser, including live test readings and the ten second post-flash screen switch in both directions.
<!-- SECTION:FINAL_SUMMARY:END -->
