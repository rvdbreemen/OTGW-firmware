---
id: TASK-1217
title: >-
  Decide: boot-time console output reaches the PIC RX line, and the UART0 mute
  also silences the USB console
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-11 22:25'
updated_date: '2026-10-17 20:34'
labels:
  - esp32
  - pic
  - console
  - needs-decision
dependencies: []
priority: medium
ordinal: 337000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the Classic + LOLIN S3 Mini bench, 2026-10-12 (esp32-combo alpha.418), traced and adversarially verified (workflow wf_337ef6e6-edd).
1) OTGWSerial is HardwareSerial(0) = UART0, begun at 9600 8N1 on RX 44 / TX 43 by its global constructor, and the IDF primary console is UART0 (CONFIG_ESP_CONSOLE_UART_NUM=0). Arduino log_e -> ets_printf -> putc1 writes to UART0, so every log line before platformMuteUart0Console() (OTGW-firmware.ino:359/:380 on combo, :456 on classic) goes into the PIC's RX at the PIC's own baud rate: about 530 bytes per boot on this board (the IMU probe and 0x26 disarm errors). Evidence: the console line timing matches 9600 baud to the millisecond for two different line lengths (124 chars -> 129 ms, 81 chars -> 84-85 ms). None of the lines contains '=', so the PIC should answer with syntax errors rather than act on them (inferred, not bench-checked).
2) The same mute also silences the USB-Serial-JTAG console: after the mute nothing appears on COM8, including the log_e('[combo] detect ...') at OTGW-firmware.ino:439 that exists to reach the USB console. That contradicts the comment at :379 ('The USB-Serial-JTAG console remains available') and means TASK-850's detection line can never be seen.
Options for the maintainer: (a) mute as the first statement of setup() on PIC-capable builds (no early boot text into the PIC; lose all early console diagnostics); (b) keep the order and make the mute UART0-only so USB keeps logging (needs a different mechanism than replacing putc1); (c) leave as is and fix only the misleading comments.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Maintainer decision recorded (a, b or c)
- [ ] #2 If a or b: bench evidence on the Classic + S3 Mini that no console text reaches the PIC after the change (e.g. no SE/syntax-error replies from the PIC at boot in the telnet/OT log), and for b that the [combo] detect line appears on the USB console
- [ ] #3 Comments at OTGW-firmware.ino:379 and :454-455 match the real behaviour
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-17: maintainer decision (b): keep the order, mute only the UART0 channel so the USB-Serial-JTAG console keeps logging. Since TASK-1216 one early line (~80 B, 0x26 disarm) still reaches the PIC.
<!-- SECTION:NOTES:END -->
