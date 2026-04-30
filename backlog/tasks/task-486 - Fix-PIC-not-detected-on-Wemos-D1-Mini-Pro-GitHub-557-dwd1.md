---
id: TASK-486
title: 'Fix: PIC not detected on Wemos D1 Mini Pro (GitHub #557, dwd1)'
status: To Do
assignee: []
created_date: '2026-04-29 23:49'
updated_date: '2026-04-30 00:37'
labels:
  - bug
  - needs-info
  - hardware
  - pic
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/557'
priority: medium
ordinal: 14000
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
- [ ] #1 Confirm the pin mapping difference between Wemos D1 (classic) and Wemos D1 Mini Pro on the NodoShop PCB
- [ ] #2 Document either how to make the Mini Pro work (rewire / pin-define change) or that it is unsupported
- [ ] #3 Update issue #557 with diagnostic steps for reporter (boot-time serial log at 74880/115200 baud)
<!-- AC:END -->
