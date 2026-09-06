---
id: TASK-1131
title: >-
  ESP-IDF log output leaks onto the PIC serial line and the PIC answers it as
  menu input
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-05 20:39'
updated_date: '2026-09-06 17:51'
labels:
  - bug
  - pic
  - serial
dependencies: []
priority: high
ordinal: 279000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Made visible by the new diagnose screen (TASK-1128) on the bench ESP32-S3 at alpha.362/363, esp32-classic, against a pic16f1847 running diagnostic firmware 2.2. The diagnose console repeatedly shows lines like: 'Enter test number: E (89325) task_wdt: esp_task_wdt_reset(707): task not found' immediately followed by 'Invalid test' and a menu redraw. Read that sequence carefully: the PIC is ECHOING an ESP-IDF error log back at us, which means the ESP wrote that log text INTO the PIC UART. The PIC then treats it as a menu choice, rejects it, and redraws. On the classic-on-S3 pin map the PIC UART is UART0 (GPIO43/44), which is also where the IDF console logs by default, so this is the known UART0/console overlap. Two defects are stacked here: the console leak itself, and whatever calls esp_task_wdt_reset() from a task that is not subscribed to the task watchdog. The leak is the more serious of the two, because the ESP is injecting bytes into the PIC on any board whose PIC UART shares UART0, which on a gateway PIC would mean unsolicited command text rather than a harmless rejected menu choice.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No ESP-IDF log output can reach the PIC UART on any board where the PIC shares UART0
- [ ] #2 The esp_task_wdt_reset call from an unsubscribed task is found and either subscribed or removed
- [ ] #3 The diagnose console shows only PIC output, with no interleaved firmware log lines
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Both halves are compliance with decisions already Accepted, so neither needs a new ADR.

Established by reading, not inferred:
- The IDF console and the PIC UART are the SAME peripheral on esp32-classic. The prebuilt Arduino-ESP32 sdkconfig pins the primary console to UART0 (CONFIG_ESP_CONSOLE_UART_NUM=0), OTGWSerial derives from HardwareSerial(0) (OTGWSerial.cpp:877), and PIN_PIC_RX/TX are 44/43 (boards.h:165-166), which are UART0's native IO_MUX pins. ARDUINO_USB_CDC_ON_BOOT does NOT move the IDF console: it only remaps the Arduino `Serial` object (HardwareSerial.h:426-440). The comment in platformio.ini:196-201 claims otherwise and is wrong.
- The project already has the fix: platformMuteUart0Console() (platform_esp32.h:354) nulls the esp_log vprintf, nulls the ROM putc1, and reopens stdout/stderr on /dev/null. Both of its call sites (OTGW-firmware.ino:358, :379) sit inside `#if HAS_RUNTIME_HW_DETECT`, which is 1 only on combo and 0 on esp32-classic (boards.h:241, :303-304). On the target that hit this defect the mute is never compiled in. ADR-168 claims scope over esp32-classic; the implementation never covered it.
- The watchdog emitter is the picSerial task. esp_task_wdt_reset() appears in application code in exactly one function, feedWatchDog(), guarded only by s_twdtReady, which is a plain global that says "init ran", not "this task is subscribed". picSerialDrainOnce() calls feedWatchDog() at OTGW-Core.ino:762 and :793, and its only caller is picSerialTaskBody. ADR-135 Decision 5 deliberately subscribes the loop task only.
- Contradiction resolved by hand, because two readers disagreed: async_tcp IS subscribed. CONFIG_ASYNC_TCP_USE_WDT defaults to 1 (AsyncTCP.h:41-43), the `#undef` to 0 sits inside `#if defined(LIBRETINY)` which does not apply, and _async_service_task calls esp_task_wdt_add(NULL) (AsyncTCP.cpp:320-324). So the FSexplorer feedWatchDog calls on that task are legitimate and are NOT part of this defect.

Changes:
1. Console: call platformMuteUart0Console() in the `#else` arm at OTGW-firmware.ino:444-449, before detectPIC(), gated on `#if HAS_PIC`. Not hoisted above the `#if`, deliberately: the combo boot order is known fragile (moving LittleFS/readSettings to the top of setup() hung the S3 once), and the combo path already calls the mute. Gating on HAS_PIC keeps OTGW32 unmuted, where UART0 carries no PIC and the console is worth having.
2. Emitter: remove the two feedWatchDog() calls inside picSerialDrainOnce() (OTGW-Core.ino:762, :793). They cannot do their job: the TWDT reset can never succeed from an unsubscribed task, and the 0x26 feed is the loop task's work. Removing them also takes an I2C Wire transaction and a GPIO LED write out of task context, which picSerialDrainOnce's own contract at :677-689 already says belong loop-side.
3. Class-level guard: replace the s_twdtReady test at OTGW-Core.ino:1460 and :1517 with a subscription test, `esp_task_wdt_status(NULL) == ESP_OK`. The primitive is already used in this file at :1394 and :1504. A future off-loop caller then becomes a silent no-op instead of a UART-visible log storm.

Not doing, and why:
- Subscribing the picSerial task to the TWDT. ADR-135 Decision 5 rejects it explicitly and the comment at OTGW-Core.ino:1355-1358 records that. Changing it is an ADR supersession, not a patch.
- Touching src/libraries/OTGWSerial. Vendored, and excluded from the abstraction gate precisely because it is upstream.
- Moving the IDF console off UART0 at build time. The console selection is baked into the prebuilt Arduino-ESP32 static libraries; a -D flag cannot reach it.

Known limit to state honestly: the mute cannot cover bytes emitted before setup() reaches it. The ROM boot banner and the OTGWSerial global constructor both run first. This narrows the window to boot; it does not close it.

Gates: build.bat (never python build.py directly), targets esp32-classic and esp32-combo since both compile the changed code; python evaluate.py --quick with the abstraction gate at BASELINE=0 (`#if HAS_PIC` does not match its regex, so it is safe); bin/bump-prerelease.sh for the version sweep, which stages what it touches. Then on-device verification on the bench S3, which is what AC 3 actually asks for.
<!-- SECTION:PLAN:END -->
