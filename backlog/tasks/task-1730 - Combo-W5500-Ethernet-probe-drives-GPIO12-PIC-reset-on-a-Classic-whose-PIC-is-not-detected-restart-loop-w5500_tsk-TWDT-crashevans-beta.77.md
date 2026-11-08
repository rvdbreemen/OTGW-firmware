---
id: TASK-1730
title: >-
  Combo: W5500 Ethernet probe drives GPIO12 (PIC reset) on a Classic whose PIC
  is not detected, restart loop (w5500_tsk TWDT, crashevans beta.77)
status: Done
assignee:
  - system32-78
created_date: '2026-11-08T11:58:00Z'
updated_date: '2026-11-08T14:14:00Z'
labels:
  - bug
  - field-report
  - combo
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 838000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
crashevans 2026-11-07 18:02 #beta-testing (msg 1568687176538001601): on an OTGW Classic with an S3, when the PIC was not detected (board fault) beta.77 went into a restart loop; the stored core dump is a task watchdog in w5500_tsk (elf 9849b031). His reading: the W5500 probe drives GPIO12, which is the PIC reset line on the Classic (boards.h:221 combo pin table: GPIO12 = PIC reset on Classic, SPI SCK for the W5500 on the other board). Core dump is in both capture zips attached to that message. To check: does the current combo still run the W5500 probe when no PIC is found (Ethernet.ino ~155), and can a Classic be told apart before touching GPIO12/16; decode the core dump with the beta.77 elf.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause confirmed from the core dump and the code (file:line), on current dev
- [x] #2 If still present: a Classic without a detected PIC does not drive GPIO12/16 for a W5500 probe and boots without a restart loop (host test or bench with the PIC held in reset/removed)
- [x] #3 Gate: initEthernet() never runs with iBoardMode 1/3, nor in HW_MODE_PIC or HW_MODE_DEGRADED; only on confirmed OTGW32 (host test red/green on the real gate)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-08 (requirement): the W5500 may only be touched on confirmed OTGW32 hardware, and never in PIC mode. Code check on dev (6c7d1ca/b5ed03e23): the only gate is OTGW-firmware.ino:526 'if (!isPICEnabled()) initEthernet();' and isPICEnabled() is state.pic.bAvailable (OTGW-firmware.h:643-649). With iBoardMode 1 or 3 (Classic) and a dead/undetected PIC, detectPIC() sets HW_MODE_DEGRADED (OTGW-firmware.ino:421) and bAvailable stays false, so the W5500 probe runs and clocks GPIO12 = PIC reset on the Classic (boards.h:221): crashevans's restart loop is still possible. Fix direction: run initEthernet() only when the board is positively an OTGW32 (define 'confirmed': e.g. state.hw.eMode == HW_MODE_OT_DIRECT with a working OT-Direct bus AND iBoardMode not 1/3), never when iBoardMode is 1 or 3 and never in HW_MODE_DEGRADED or HW_MODE_PIC. Host test red on the old gate (Classic + dead PIC -> probe) and green on the fix.

Implementation 2026-11-08 (system32-78, wt-1730 lane-1730 on origin/dev 5fe2363c9): Robert decided in session system32-78 to ADD a PSRAM term after crashevans's own transcript (beta.118 zips from msg 1568687176538001601, downloaded with Robert's OK) showed hardwaremode OT-Direct, hardware_type otgw32, psram_found 1, chip_model_est ESP32-S3FH4R2, status_pixel_gpio 47 = Classic with a plain S3 Mini in auto mode whose PIC is not detected; without the PSRAM term that exact unit stays unprotected. isOtgw32HardwarePossible() (OTGW-firmware.h): combo = iBoardMode not 1/3 && !isPICEnabled() && !bClassicPro && !platformHasPsram(); fixed boards = HAS_DIRECT_OT && !isPICEnabled(). isConfirmedOtgw32() = that && eMode == HW_MODE_OT_DIRECT. Gates: OTGW-firmware.ino initEthernet() now 'if (isConfirmedOtgw32())'; the auto no-PIC branch runs initOTDirect() only if isOtgw32HardwarePossible() (else eMode stays DEGRADED, log line). Forced mode 2 still runs initOTDirect by user choice, but never probes the W5500 on a LOLIN. Found on the way: initOTDirect() sets HW_MODE_OT_DIRECT whenever no PIC is enabled, regardless of probeOTBus(), so eMode alone never proved OTGW32 hardware. Tests: tests/test_w5500_otdirect_only_on_otgw32.py compiles the real detection block + eth gate + helpers: old code 4 of 6 FAIL (Classic mode 1/3 dead PIC probes W5500; auto Pro dead PIC; auto plain S3 Mini dead PIC = crashevans; forced mode 2 on LOLIN probes W5500), fix 6/6 OK. ADR-201 gate test_board_detect_persist_gate.py now injects the real helper (no-PSRAM stub) and stays 7/7 OK incl. its mutant; test_pic_rst_pin OK; evaluate.py --quick 0 FAIL 0 WARN. AC1 core dump decode still open: needs the beta.77 elf (sha256 9849b031c), not local.

AC1 core dump decode 2026-11-08: elf = release v2.0.0-beta.77 asset OTGW-firmware-esp32-combo-2.0.0-beta.77+03339b6.elf (sha256 9849b031c78d..., matches the dump's elf=9849b031; downloaded with Robert's OK). crashevans transcript (beta.118 zips) crashlog: 'Task watchdog (6)', 6 core dumps all in task w5500_tsk. addr2line (toolchain-xtensa-esp-elf) of the main backtrace 0x42014455 0x4201a261 0x420140f5 0x4214f77f 0x420d61a1 0x420d6426 0x40383841: SPIClass::write16 <- EthernetESP32 W5500Driver.cpp:54 (read: write16 cmd, write addr, transferBytes; no timeout, always returns ESP_OK) <- EthDriver.cpp:59 <- esp_eth_mac_w5500.c:189 (w5500_read) <- :243 (second register read in w5500_get_rx_received_size, do { read RX_RSR twice } while (received0 != received1)) <- :674 (emac_w5500_alloc_recv_buf) <- :832 (emac_w5500_task receive loop) <- vPortTaskWrapper. The other dumps land in the same path (W5500Driver::read -> esp32-hal-spi.c spiTransferBytes, xQueueGenericSend in the SPI lock). Reading: the W5500 MAC task was running on a Classic without a W5500 (the probe only exists because !isPICEnabled() let initEthernet() run); with nothing on SPI the RX-size reads return floating data, the do-while never sees two equal reads and the task spins in SPI transfers until the task watchdog resets the chip, every boot (restart loop). Line numbers mapped against the local ESP-IDF copy (framework-espidf components/esp_eth/src/spi/w5500); the elf links the precompiled IDF of the Arduino core v5.5.1, so the exact line match is consistent but not proven. Still present on dev before this fix: the gate at OTGW-firmware.ino 'if (!isPICEnabled()) initEthernet();' is unchanged since beta.77 and crashevans's unit (auto mode, plain S3 Mini, psram 1, PIC not detected) still runs as OT-Direct on beta.118.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Combo: the W5500 probe and OT-Direct run only on a confirmed OTGW32. Root cause (AC1, beta.77 elf sha256 9849b031c): all 6 crashevans core dumps are a task watchdog in w5500_tsk, spinning in esp_eth_mac_w5500.c w5500_get_rx_received_size (read RX_RSR twice until equal) via EthernetESP32 W5500Driver::read with nothing on SPI; the probe ran because the only gate was !isPICEnabled() (OTGW-firmware.ino), and initOTDirect() sets HW_MODE_OT_DIRECT whenever no PIC is enabled. Fix (d63a128cf, beta.147): isOtgw32HardwarePossible() = board mode not 1/3 && !isPICEnabled() && !bClassicPro && !platformHasPsram() (PSRAM term added by Robert after crashevans's transcript showed psram_found 1 / ESP32-S3FH4R2 / status_pixel_gpio 47 in auto mode); isConfirmedOtgw32() adds eMode == HW_MODE_OT_DIRECT and gates initEthernet(); the auto no-PIC branch runs initOTDirect() only when the hardware is possible. AC2/AC3: tests/test_w5500_otdirect_only_on_otgw32.py compiles the real detection block, eth gate and helpers: old code 4 of 6 FAIL (Classic mode 1/3 dead PIC probes W5500; auto S3 Mini Pro dead PIC; auto plain S3 Mini dead PIC with PSRAM = crashevans; forced mode 2 on a LOLIN probes W5500), fix 6/6 OK; ADR-201 gate (real helper injected) 7/7 OK incl. mutant; evaluate.py --quick 0 FAIL 0 WARN. The dead-PIC case is proven by the host test only: a silent PIC cannot be simulated safely on the bench. Bench (OTA app+fs of beta.147+d63a128): .63 Classic S3 Mini PIC 6.8 stays PIC mode, ethernetpresent false, 0 settings changed, stable (bootcount 176 unchanged after 6 min, no restart loop); .61 OTGW32 stays OT-Direct (otdmode master), psram_found 0, board mode 0, i.e. exactly the inputs for which the gate runs the probe; this unit has no W5500 fitted (ethernetpresent false before and after), and the boot telnet lines of the probe are emitted before a telnet client can connect (two reboot captures), so the probe running on an OTGW32 is shown by the gate inputs plus the host test, not by a log line. Full suite: no regression (pre-existing: test_diag_decode fixture TASK-1723; test_web_file_scheduling needs a compiler; test_refresh_storm is a stub-only tool self-test that failed on timing under -j 4 load).
<!-- SECTION:FINAL_SUMMARY:END -->
