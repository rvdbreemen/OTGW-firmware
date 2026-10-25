---
id: TASK-1217
title: >-
  Decide: boot-time console output reaches the PIC RX line, and the UART0 mute
  also silences the USB console
status: Done
assignee:
  - '@claude'
created_date: 2026-10-11 22:25
updated_date: '2026-10-25T10:37:00Z'
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
- [x] #2 If a or b: bench evidence on the Classic + S3 Mini that no console text reaches the PIC after the change (e.g. no SE/syntax-error replies from the PIC at boot in the telnet/OT log), and for b that the [combo] detect line appears on the USB console
- [x] #3 Comments at OTGW-firmware.ino:379 and :454-455 match the real behaviour
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Option (b) mechanism, plus one consequence: once the mute keeps USB alive, it can move to the top of setup() at no cost, so the PIC gets no console text at all (AC2) and USB keeps every diagnostic.
1. Measure first (bench, no code change kept): small experiment build that logs a marker via each path (log_e/ets_printf, ESP_LOGE, printf) before and after each of the three current mute steps, captured on COM8 (USB) and on the PIC RX side (telnet OT log / port 25238 echo). Goal: which step silences USB.
2. Implement in platformMuteUart0Console() (platform_esp32.h only): ROM putc1 -> null as now; leave the USB-Serial-JTAG ROM channel alone; point stdout/stderr at the USB-Serial-JTAG VFS device (/dev/usbserjtag, if registered in this build) instead of /dev/null; esp_log vprintf -> default (now ends on USB) instead of null. Fallback if the VFS device is absent: a vprintf that writes to the USB-Serial-JTAG driver directly.
3. Move the mute call to the first statement of setup() on PIC-capable builds (combo :375/:396, classic :472), so nothing reaches UART0 before OTGWSerial owns it.
4. Fix the comments at :379 and :454-455 (AC3).
5. Bench old-vs-fix on the Classic + S3 Mini (combo): USB capture shows the [combo] detect line and the IMU/0x26 lines after the change; PIC side shows no SE/syntax-error replies at boot and port 25238 sees no console bytes. Also check boot with no USB host attached (USB-JTAG writes must not block). Build classic + combo, evaluate --quick.
Risk: USB-JTAG VFS writes can stall when a host is attached but not reading; IDF drops after a timeout, and pre-mute boot already writes there today.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-17: maintainer decision (b): keep the order, mute only the UART0 channel so the USB-Serial-JTAG console keeps logging. Since TASK-1216 one early line (~80 B, 0x26 disarm) still reaches the PIC.

2026-10-18 measurement (experiment build, never committed; captures in .tmp/bench-1217/exp2-usb.txt). Markers via ets_printf, Arduino ESP_LOGE, printf, log_e and raw esp_log_write at each step on combo:
- top of setup(): all paths reach USB.
- right after OTGWSerial.end() (:327), before any mute step: ets/log_e/ESP_LOGE are gone; printf and esp_log survive. Cause: uartEnd(0) -> uartSetDebug(NULL) -> uart_install_putc() -> ets_install_putc2(NULL) (esp32-hal-uart.c:1056, :1381). So the USB silence was not caused by the mute.
- esp_log_set_vprintf(null) kills esp_log; freopen(/dev/null) kills printf on USB too (the VFS console writes UART0+USB together).
- /dev/usbserjtag is not registered in this build (fopen returned NULL); a freopen to it closed stdout and boot-looped exp1. Never freopen to an unchecked path.
Deviation from plan step 2 (fallback taken): putc2 and the esp_log vprintf write the USB-Serial-JTAG TX FIFO directly (non-blocking, one 2 ms wait max then drop while the FIFO stays full). uartSetDebug(NULL) first, so a later OTGWSerial.end() no longer resets putc2. stdout/stderr stay /dev/null: plain printf no longer reaches USB (it did not reach USB after the mute before either). Mute moved to the first statement of setup() on HAS_PIC || HAS_RUNTIME_HW_DETECT; the three old call sites removed.
ADR-168 (Accepted) stays valid: the decision is unchanged, only the mechanism and call sites moved; no ADR edit. Open-source check: trivial glue over the IDF LL API, no library search needed.

2026-10-18: committed and shipped in esp32-combo alpha.436 (posted to #alpha-testing). evaluate.py --quick: 72 passed, 0 warn, 0 fail. AC2 bench evidence (no SE at boot on the Classic + S3 Mini, [combo] detect line on USB) still open.

2026-10-22 cycle: skipped, AC2 needs the Classic + S3 Mini bench board, which is not connected (no COM8, .88.63 silent).

2026-10-22 23:15 cycle: skipped, needs the Classic + S3 Mini bench board (not connected; only OTGW32 COM4 and the 1.x ESP8266 COM3).

2026-10-25 evidence recovered from the 2026-10-18 bench session (Classic + LOLIN S3 Mini, captures in the local untracked folder .tmp/bench-1217/): OLD build probe-old-r1/r2-usb.txt '[probe1217] rx=73 SE=1' and 'rx=84 SE=1' (the PIC answered SE to console text); FIX build probe-fix-r1/r2-usb.txt 'rx=21 SE=0' in both runs, and '[combo] detect: eMode=1 picEnabled=1 boardMode=1 pro=0 ...' on the USB console in both runs. Caveat: probe build = the fix (87cb037c2, fixsave matches apart from the version banner) plus one marker log line, not the shipped alpha.436 binary; boot without a USB host was not captured (not an AC). AC3: comments rewritten in 87cb037c2; HEAD OTGW-firmware.ino:267, :457-459, :468 say UART0 is muted at the top of setup() and the console stays on USB-Serial-JTAG, matching platformMuteUart0Console().
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decision b: platformMuteUart0Console() mutes only the UART0 console channel at the top of setup(), so boot text no longer reaches the PIC RX and the IDF console keeps logging on USB-Serial-JTAG (87cb037c2, alpha.436). Evidence: AC2 bench old-vs-fix on the Classic + S3 Mini 2026-10-18 (.tmp/bench-1217: old SE=1 in 2 runs, fix SE=0 in 2 runs plus the [combo] detect line on USB; probe build = fix + marker line). AC3: comment rewrite in 87cb037c2, checked at HEAD OTGW-firmware.ino:267/457-459/468.
<!-- SECTION:FINAL_SUMMARY:END -->
