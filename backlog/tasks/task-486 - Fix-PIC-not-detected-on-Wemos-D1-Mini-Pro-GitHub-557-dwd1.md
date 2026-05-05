---
id: TASK-486
title: 'Fix: PIC not detected on Wemos D1 Mini Pro (GitHub #557, dwd1)'
status: Done
assignee:
  - '@copilot'
created_date: '2026-04-29 23:49'
updated_date: '2026-05-05 21:55'
labels:
  - bug
  - needs-info
  - hardware
  - pic
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/557'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter dwd1 owns NodoShop v2.11 OTGW with original PIC16LF1847 and original Wemos D1 board. Wants to switch to Wemos D1 Mini Pro for external antenna due to poor WiFi reception. Flashed identical v1.4.1 firmware (ino.bin + littlefs.bin via esptool at 0x0 / 0x200000) on the Mini Pro board:

- 'PIC available: false' shown in webUI
- Settings page lacks Run Boot Command checkbox and Boot Command input field
- Forcing firmwarePage() in browser dev console renders nothing useful
- Switching back to the original Wemos D1 board: PIC menu reappears

Hypothesis: the Wemos D1 Mini Pro has a different RX/TX pin mapping or a different USB-serial chip wiring than the classic D1 used by NodoShop. The OTGW PIC is connected via the ESP's hardware serial; if the pin assignments differ, the firmware will not see the PIC even though it's physically present.

The OTGW Discord and wiki should mention that NodoShop v2.x is wired specifically for the classic Wemos D1; using a Mini Pro is not a drop-in replacement.

GitHub: https://github.com/rvdbreemen/OTGW-firmware/issues/557
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Confirm the pin mapping difference between Wemos D1 (classic) and Wemos D1 Mini Pro on the NodoShop PCB
- [x] #2 Document either how to make the Mini Pro work (rewire / pin-define change) or that it is unsupported
- [x] #3 Update issue #557 with diagnostic steps for reporter (boot-time serial log at 74880/115200 baud)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the repo hardware and board-mapping docs/code to confirm whether NodoShop v2.x is wired for the classic Wemos D1, the D1 mini family, or both.
2. Trace how PIC availability is detected in firmware and whether any board-selection or pin-definition path could make a D1 Mini Pro work without rewiring.
3. If the board is fundamentally unsupported, document that clearly and prepare the exact diagnostic/response needed for issue #557; if it is supportable, identify the smallest safe code or config change.
4. Validate any code/doc change with the normal build/evaluator flow and update the backlog task with a concise issue-response summary.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Confirmed from the repo and current firmware code that this is not a missing board-profile setting: PIC comms are fixed to ESP8266 UART0 (TX/GPIO1, RX/GPIO3) and PIC reset is fixed to D5/GPIO14. There is no alternate pin-remap path in the firmware.
- README hardware support says NodoShop 2.3+ uses the Wemos D1 mini family. Based on that and the D1 mini Pro pinout, a Mini Pro should already match the expected D1 mini-family pins used by the firmware.
- Updated README hardware support text to make that explicit and to steer future triage toward boot-log / hardware continuity checks instead of speculative firmware pin remaps.
- Posted diagnostic guidance on GitHub issue #557 asking for 74880/115200 boot serial logs and hardware orientation/reset-line verification: https://github.com/rvdbreemen/OTGW-firmware/issues/557#issuecomment-4383443726
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed TASK-486 as a documentation-and-diagnostics task rather than a firmware fix.

Findings:
- The current firmware hardcodes PIC communication to ESP8266 UART0 (TX/GPIO1, RX/GPIO3) and PIC reset to D5/GPIO14. There is no alternate board profile or pin-remap setting for this path.
- The repo hardware-support table says NodoShop 2.3+ uses the Wemos D1 mini family, and the Wemos D1 mini Pro matches that footprint/pinout for the pins the firmware uses.
- That means the reported `picavailable=false` symptom is not explained by a missing firmware pin-definition change.

Changes:
- Updated `README.md` hardware support text to clarify the D1 mini-family assumption, the fixed PIC wiring expectations, and the correct next diagnostic step when a Mini Pro still fails.
- Updated GitHub issue #557 with boot-log and hardware-verification steps for the reporter.

Disposition:
- No firmware code change was made. The task is complete because the acceptance criteria were documentation/diagnostic focused and the issue now has the precise next steps needed to continue.
<!-- SECTION:FINAL_SUMMARY:END -->
