---
id: TASK-1131
title: >-
  ESP-IDF log output leaks onto the PIC serial line and the PIC answers it as
  menu input
status: Done
assignee:
  - '@claude'
created_date: '2026-09-05 20:39'
updated_date: '2026-09-06 19:19'
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
- [x] #1 No ESP-IDF log output can reach the PIC UART on any board where the PIC shares UART0
- [x] #2 The esp_task_wdt_reset call from an unsubscribed task is found and either subscribed or removed
- [x] #3 The diagnose console shows only PIC output, with no interleaved firmware log lines
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Root cause is a compliance gap, not a missing capability. platformMuteUart0Console() (platform_esp32.h:354) already does the right thing; both its call sites sat inside #if HAS_RUNTIME_HW_DETECT, which is 1 only on combo (boards.h:241, :303-304), so esp32-classic never compiled it in. ADR-168 claims scope over esp32-classic; the implementation never reached it.
- ARDUINO_USB_CDC_ON_BOOT does NOT move the IDF console. It remaps the Arduino Serial object only (HardwareSerial.h:426-440). The comment at platformio.ini:196-201 says otherwise and is the reason this stood.
- Emitter identified: picSerialDrainOnce() called feedWatchDog() at OTGW-Core.ino:762 and :793 from the picSerial task, which ADR-135 Decision 5 deliberately leaves unsubscribed. s_twdtReady reports that init ran, not who is asking, so the calls sailed through and the IDF answered "task not found".
- Corrected one reader claim by hand: async_tcp IS TWDT-subscribed (CONFIG_ASYNC_TCP_USE_WDT defaults to 1 at AsyncTCP.h:41-43; the undef to 0 sits in the LIBRETINY branch; _async_service_task calls esp_task_wdt_add at AsyncTCP.cpp:320-324). The FSexplorer feedWatchDog calls are therefore legitimate and are not part of this defect.
- Gates: build.bat per target. esp32-classic [SUCCESS] 346.71s with a fresh binary, esp32-combo and esp32 both [SUCCESS], zero error lines. evaluate.py --quick 68 passed / 0 failed / 98.7%; the one warning is a pre-existing boards.h path lookup in the STATUS_BURST_COOLDOWN_MS check.
- Committed 02a3d90d, pushed to origin/dev, version bumped alpha.363 to alpha.364.
- AC 3 is on-device and NOT yet done. COM8 is attached and is the esp32-classic board with a PIC, but the leak lives on GPIO43/44 and is invisible over USB: the check needs the diagnose screen, which needs WiFi, and that board is recorded as not provisioned.

- On-device before/after on the real rig: the classic-S3 bench board (MAC AC:27:6E:CE:45:D8) at 192.168.88.63, hardware_type otgw-classic, picfwtype diagnose 2.2 -- the exact configuration in the report.
- BEFORE (alpha.362+f05d559, as shipped): the defect reproduced verbatim in the diagnose console: "Enter test number: E (80774049) task_wdt: esp_task_wdt_reset(707): task not found". 1 occurrence over 60s / 14 menu round-trips.
- AFTER (alpha.364+02a3d90): 0 occurrences over 180s / 38 menu round-trips, at the same request cadence. Screenshot kept as task1131-diagnose-clean-alpha364.png.
- Honest weight of that measurement: 3x the duration and ~2.7x the round-trips with zero hits is consistent with the fix but is not overwhelming on its own. The deterministic argument carries it: the two feedWatchDog() calls in picSerialDrainOnce() are deleted, so the picSerial task can no longer call esp_task_wdt_reset() at all, and the guard now tests subscription. The test confirms the observable outcome; it cannot say which of the two barriers (emitter removal or console mute) is doing the work, because both landed together and the mute would hide the log either way.
- Flashed over USB, not OTA. App OTA is refused by the firmware itself on this board: partitions_otgw_esp32.csv has a single app slot, so esp_ota_get_next_update_partition() returns the RUNNING partition and the write would overwrite the executing app (TASK-959). hasSpareAppOtaSlot() blocks it. flash_otgw.bat --update --app writes only 0x10000 and the board rejoined WiFi by itself.
- Incidental find, not fixed here: POST /api/v2/otgw/diagnose with an unexpected field name forwarded the RAW JSON body to the PIC, which echoed {"input":"\r"} into the console. Same shape as the raw-body fallback recorded in TASK-1083.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The ESP was writing its own error log into the PIC, and the PIC was answering it as menu input. Both halves are now closed, and both turned out to be compliance with decisions already Accepted, so no new ADR is owed.

Root cause, the console half. On esp32-classic the IDF console and the PIC link are not merely colliding, they are the same peripheral on the same pins: the prebuilt Arduino-ESP32 sdkconfig pins the primary console to UART0, OTGWSerial derives from HardwareSerial(0), and PIN_PIC_RX/TX are GPIO44/43, UART0's native IO_MUX pins. ARDUINO_USB_CDC_ON_BOOT does not move the console; it remaps the Arduino `Serial` object only. The comment in platformio.ini claiming otherwise is what let this stand for so long.

The project already had the remedy. `platformMuteUart0Console()` nulls the esp_log vprintf, nulls the ROM putc1 and reopens stdout/stderr on /dev/null, and ADR-168 decided it must run on the PIC path. Both of its call sites sat inside `#if HAS_RUNTIME_HW_DETECT`, which is 1 only on combo, so the fixed Classic target never compiled it in. This was a compliance gap between an ADR and its implementation, not a missing capability.

Root cause, the emitter. `esp_task_wdt_reset()` lives in exactly one application function, `feedWatchDog()`, guarded only by `s_twdtReady`, which reports that initWatchDog ran and says nothing about which task is asking. `picSerialDrainOnce()` called `feedWatchDog()` twice, and its only caller is the picSerial task, which ADR-135 Decision 5 deliberately leaves unsubscribed. Both calls were no-ops by construction and additionally dragged an I2C transaction and a GPIO write into task context, which the function's own contract already excludes.

Changes:
- `platformMuteUart0Console()` is called on the fixed-board path, gated on HAS_PIC so OTGW32 keeps its console, where UART0 carries no PIC. Deliberately not hoisted above the `#if`: the combo boot order is known fragile and that path already calls the mute.
- The two `feedWatchDog()` calls in `picSerialDrainOnce()` are removed, and the reason is recorded in that function's own header comment beside the LED and ser2net entries that moved loop-side before it.
- Both watchdog guards now test `esp_task_wdt_status(NULL) == ESP_OK` rather than the boot flag, so a future off-loop caller is a silent no-op instead of a log storm aimed at the PIC.

Measured on the real rig, the classic-S3 bench board at 192.168.88.63 with a diagnose PIC 2.2:
- BEFORE (alpha.362, as shipped): reproduced verbatim, "Enter test number: E (80774049) task_wdt: esp_task_wdt_reset(707): task not found", 1 occurrence over 60s and 14 menu round-trips.
- AFTER (alpha.364): 0 occurrences over 180s and 38 round-trips at the same cadence.

Stated honestly, that measurement alone is suggestive rather than conclusive: three times the duration with zero hits is consistent with the fix but not overwhelming for an event that fired once in a minute. The deterministic argument is what carries it: the calls are deleted, so the picSerial task can no longer reach `esp_task_wdt_reset()` at all. The test also cannot say which of the two barriers is doing the work, because both landed together and the mute would hide the log either way.

Flashed over USB rather than OTA, and not by choice: app OTA is refused by the firmware itself on this board. `partitions_otgw_esp32.csv` has a single app slot, so `esp_ota_get_next_update_partition()` returns the RUNNING partition and the write would overwrite the executing app, confirmed under TASK-959. `flash_otgw.bat --update --app` writes only 0x10000 and the board rejoined WiFi unaided.

Not done, on purpose: the picSerial task is not subscribed to the TWDT. ADR-135 rejects that explicitly, so it needs a superseding ADR rather than a patch.

Known limit: the mute cannot cover bytes emitted before setup() reaches it. The ROM boot banner and the OTGWSerial global constructor run first. This narrows the window to boot; it does not close it.

One reader claim was wrong and is corrected in the record: async_tcp IS TWDT-subscribed (CONFIG_ASYNC_TCP_USE_WDT defaults to 1, the undef sits in the LIBRETINY branch, and _async_service_task calls esp_task_wdt_add itself), so the FSexplorer feedWatchDog calls on that task are legitimate and were never part of this defect.

Gates: build.bat per target, esp32-classic, esp32-combo and esp32 all [SUCCESS] with zero error lines. evaluate.py --quick 68 passed, 0 failed, 98.7%; the single warning is a pre-existing boards.h path lookup in the STATUS_BURST_COOLDOWN_MS check. Committed 02a3d90d and pushed to origin/dev, alpha.363 to alpha.364.

Filed separately: TASK-1133, where POST /api/v2/otgw/diagnose forwarded the raw JSON body to the PIC when the field name was unexpected. Found while gathering evidence here; same shape as TASK-1083.
<!-- SECTION:FINAL_SUMMARY:END -->
