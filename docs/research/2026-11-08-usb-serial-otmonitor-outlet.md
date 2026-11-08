# USB serial as an OTmonitor outlet on PIC-less hardware

Date: 2026-11-08. Task: TASK-1721 (research for the proposal TASK-1720 by tester tjfs).
Scope: research only. No firmware change, no build, no bench run.
Code citations are against `dev` HEAD `4f7fd7742` (the commit after TASK-1718 landed; OTDirect.ino in the working tree equals this commit).

## Summary

**Question.** On PIC-less hardware (ESP32-S3 with OpenTherm handled by the ESP itself, "OT-Direct"), could the USB port behave like the serial port of a classic OTGW, so that Schelte Bron's OTmonitor connects over USB?

**Short answer.** It is technically possible with a small amount of code, because the OT-Direct bridge for TCP port 25238 already produces and parses the exact line protocol OTmonitor speaks. The cost is not in the code. It is in three places:

1. The USB port stops being the diagnostic console (boot log, `log_e`, IDF logs, panic backtraces), and that conflicts with ADR-168 and with the core rule "never write to `Serial` after init" as it is worded.
2. One open question cannot be answered without a bench: whether OTmonitor opening or closing the port on Windows, macOS or Linux resets the ESP32-S3 through the USB-Serial-JTAG DTR/RTS reset logic. A reset reboots the gateway in the middle of heating.
3. The user benefit is small. OTmonitor already connects over TCP natively, and on PIC-less hardware there is no PIC to flash, which is the classic reason to use the serial cable.

**Recommendation.** Keep TCP 25238 as the supported path and document it (option A) now. Build option B (USB carries the line protocol, opt-in setting, default off, OT-Direct mode only) only if Robert wants it, and only after a 30-minute bench test shows that OTmonitor connect and disconnect does not reset the chip on Windows. Do not pursue option D (TinyUSB composite).

## 1. How the build uses USB today (facts)

### 1.1 Build flags and what `Serial` is

- All three ESP32-S3 envs set `-DARDUINO_USB_MODE=1` and `-DARDUINO_USB_CDC_ON_BOOT=1`: `platformio.ini:169-170` (esp32), `:267-268` (esp32-classic), `:298-299` (esp32-combo). The comment at `platformio.ini:163-168` says this routes the Arduino console to the S3 native USB-Serial-JTAG port.
- Platform is pioarduino `55.03.35` (`platformio.ini:111`), which ships arduino-esp32 **3.3.5** (local `framework-arduinoespressif32/package.json`) on ESP-IDF **5.5.1** (`framework-arduinoespressif32-libs/esp32s3/sdkconfig:393`, `CONFIG_IDF_INIT_VERSION="5.5.1"`; libs package version `5.5.0+sha.9bb7aa84fe`).
- With `ARDUINO_USB_MODE=1`, `Serial` is `HWCDCSerial`, the driver for the fixed-function USB-Serial-JTAG controller, not TinyUSB: `cores/esp32/HardwareSerial.h:430-432` (`#define Serial HWCDCSerial`) and `cores/esp32/HWCDC.h:110-115`.
- `HWCDC::baudRate()` returns a fixed 115200 (`HWCDC.h:106-108`). The baud rate a host sets is ignored, so OTmonitor's `9600,n,8,1` is harmless.

### 1.2 The main firmware never opens `Serial`

- No `Serial.begin`, `Serial.print` or `Serial.write` exists in `src/OTGW-firmware/` or `src/libraries/Platform/` (grep, this session). The only hits are comments, e.g. `OTGW-firmware.ino:265-267`.
- The core's `main.cpp:99-100` calls `Serial.begin()` only when `ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE`, so not in our build. `chip-debug-report.cpp:292-294` begins it only when the chip debug report runs (`main.cpp:54-58`: `ARDUHAL_LOG_LEVEL >= DEBUG` or `shouldPrintChipDebugReport()`); our build sets neither (grep for `CORE_DEBUG_LEVEL`/`ChipDebugReport` in `platformio.ini` and `src/` finds nothing).
- So the HWCDC driver (ISR, 256-byte RX and TX ring buffers, `HWCDC.cpp:305-360`) is **not installed** at runtime today.

### 1.3 What actually reaches the USB port at runtime

`platformMuteUart0Console()` (`src/libraries/Platform/src/platform_esp32.h:695-746`, called at `OTGW-firmware.ino:277` on every PIC-capable and combo build, TASK-1217, ADR-168) writes the console straight into the USB-Serial-JTAG TX FIFO with low-level calls:

- ROM `ets_printf`, Arduino `log_e` and `ESP_LOGx` through `ets_install_putc2(platformUsbJtagPutc)`;
- IDF component logs (WiFi driver and so on) through `esp_log_set_vprintf(platformUsbJtagLogVprintf)`;
- `printf` goes to `/dev/null`.

`platformUsbJtagPutc` (`platform_esp32.h:724-736`) is non-blocking: it waits at most 2 ms for the first character when the FIFO is full, then drops characters until it drains.

Content on the USB port today:

- The ROM boot banner and bootloader output: the sdkconfig has `CONFIG_ESP_ROM_CONSOLE_OUTPUT_SECONDARY=y` (`sdkconfig:538`) and `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y` (`sdkconfig:2309`), with the primary console on UART0 (`sdkconfig:2311-2312`).
- Two `log_e` calls in the firmware: the combo detection line `OTGW-firmware.ino:471` and `OLED.ino:559`.
- IDF component logs, and panic output via the secondary console.
- Nothing from the `Debug*` macros: they go to telnet only (`debugStuff.h:25-27, 70-82`).

Our own tooling reads this port: `scripts/capture-serial.py` (default `--port COM4`, 115200) and `scripts/otgw-test.py:9-13, 98, 149`. ADR-168 describes it as the place where boot diagnostics stay visible.

### 1.4 The combo image and the PIC UART

- The PIC link is `OTGWSerial`, a `HardwareSerial(0)` (UART0) opened at 9600 8N1 on the PIC pins by a global constructor: `src/libraries/OTGWSerial/OTGWSerial.cpp:880-890`, instantiated at `OTGW-firmware.h:85`.
- The project rule "never write to `Serial` after init (it is the PIC link)" dates from the ESP8266, where `Serial` *was* UART0. On the 2.x line the PIC link is UART0 and `Serial` is USB. The rule as worded would still forbid option B, so Robert would have to reword it or waive it for this case.
- The combo decides PIC or OT-Direct at boot (`OTGW-firmware.ino:471` logs the verdict). Everything before that verdict, including the ROM banner and early `log_e`, goes to USB whatever option is chosen.

### 1.5 Safeboot and USB flashing

- Safeboot is its own image with the same USB flags (`safeboot/platformio.ini:29-30`). It calls `Serial.begin(115200)` and prints `[safeboot] ...` status lines to USB (`safeboot/src/main.cpp:176-226`). None of the options below needs to change it.
- `flash_otgw.bat:188-198` finds the port by VID/PID: `303A:1001` (the built-in USB-Serial-JTAG) or `303A:4001` (the USB CDC of a fresh board's factory firmware, TASK-1394). All write paths use `--before default_reset --after hard_reset` (`flash_otgw.bat:226, 231, 233`).
- esptool 5.1.0 (`tool-esptoolpy/package.json`) resets a USB-Serial-JTAG device with `USBJTAGSerialReset` (`tool-esptoolpy/esptool/reset.py:125-143`): it sets DTR to select IO0, then sets RTS with DTR cleared to reset ("Reset. Calls inverted to go through (1,1) instead of (0,0)"), then clears both. The reset and download-mode entry are done by the controller hardware, independent of the running app.
- ESP-IDF 5.5.1 docs: "The USB Serial/JTAG Controller is able to put the ESP32-S3 into download mode automatically." If the application reconfigures the USB pins or disables the controller, the device disappears and you must hold GPIO0 low and reset by hand. The PIC-less board has a boot button on GPIO0 (`boards.h:58`, `docs/hardware/PINOUT.md:83`). Whether it is reachable with the case closed is not verified.

## 2. ESP32-S3 USB options (facts from vendor docs and the pinned core)

| | USB-Serial-JTAG (today) | USB OTG with TinyUSB |
|---|---|---|
| Implementation | Fixed-function hardware, one CDC-ACM port plus JTAG (IDF USJ console page) | Software USB device stack; arduino-esp32 selects it with `ARDUINO_USB_MODE=0` |
| Number of CDC ports | One | Up to 2 in the precompiled libs (`CONFIG_TINYUSB_CDC_MAX_PORTS=2`, `sdkconfig:669`) |
| Auto download / reset | In hardware, works while the app hangs or crashes | Only while the app's TinyUSB stack runs: the app reboots into the ROM loader on a 1200-baud touch (`USBCDC.cpp:282-283`) or DTR/RTS (`enableReboot`), and switches the PHY back to USJ first (`esp32-hal-tinyusb.c:523-602`) |
| App crash / boot loop | Port stays, panic output visible | IDF usb-otg-console page: "If the application crashes, panic handler output may not be sent over USB CDC in some cases"; a boot loop before the stack starts gives no USB device |
| PHY | Internal PHY, default owner | Arduino switches the internal PHY to OTG at runtime (`esp32-hal-tinyusb.c:132-143`, `USB_PHY_TARGET_INT`) |

## 3. What OTmonitor expects from a serial port (facts, otmonitor 6.6 source)

- Opening: on Windows, `open $device RDWR` then `fconfigure $dev -mode 9600,n,8,1` (`other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl:1522-1530`). On other systems it opens with `{RDWR NOCTTY NONBLOCK}` (`:1531-1535`). Then `-blocking 0 -buffering line -translation {crlf cr}` (`:1538`): input lines end in CR or CRLF, output lines get CRLF.
- OTmonitor never sets `-handshake` or `-ttycontrol` DTR/RTS on connect (grep of `otmonitor.vfs`; the only `-ttycontrol` use is `BREAK` for a PIC upgrade, `upgrade.tcl:314-318`). In Tcl 8.6 on Windows, `-mode` calls `BuildCommDCBW` and leaves `fDtrControl`/`fRtsControl` at whatever `GetCommState` returned (tcltk/tcl `core-8-6-branch`, `win/tclWinSerial.c`, `SerialSetOptionProc`). So the DTR/RTS state on open depends on the driver's earlier state, not on OTmonitor.
- Line handling: `receive` (`otmonitor.tcl:423-434`) feeds each line to `process` (`:467`). A 9-character `[ABRT]XXXXXXXX` line is an OpenTherm frame (`:478-489`). When `fwversion` is still unknown and a `B` frame arrives, OTmonitor sends `PR=A`.
- **Unknown lines are taken as command answers.** The final `else` branch (`:580-583`) prints any unrecognised line and calls `response $line`. For an unknown line `response` uses the pattern `*` (`:448-449`), and `lsearch -index 0 $sercmd $pat` (`:452`) then matches the first pending command. So a log line that arrives while OTmonitor waits for an answer is consumed as that answer. Under option B or C, any stray console text in protocol mode corrupts command handling. The logs must be fully off on that port, not just reduced.
- Gateway reset detection: a line containing `OpenTherm Gateway <ver>` signals "Gateway reset" (`:541-549`).

## 4. What exists to reuse (facts)

- **Output, one choke point.** Every OT-Direct bridge line leaves through `otDirectBridgeWriteLine()` (`OTDirect.ino:695`), which writes payload plus CRLF to `OTGWstream` (the port 25238 server, `OTGW-Core.h:33`) when `settings.mqtt.bLegacyPort25238Enabled` is set. Its callers are status lines (`otDirectBridgeProcessStatus`), frames (`bridgeFrameToParser`, `OTDirect.ino:775`), `XX: value` answers (`synthesizeResponse`, `:2278`) and `PR:` answers (`otDirectBridgeProcessPRResponse`, `:2291`). A second sink is one extra write in one function.
- **Input.** `handleOTDirectBridgeStream()` (`OTDirect.ino:714`) assembles CR-terminated lines (LF skipped, 128-byte limit, at most 4 lines per call) and passes each to `sendPICSerial()` (`:749`), which routes to `handleOTDirectCommand()` on PIC-less hardware (`OTGW-Core.ino:4385-4393`). The SAT guard against external control commands (ADR-179, `OTDirect.ino:2773`) sits in that handler, so a USB source would inherit it. ADR-217 is the Classic-only 25238 filter and does not apply here.
- **Not reusable as is.** The line buffer, overflow flag and counters are function `static`s (`sWrite`, `bytes_write`, `discardCurrentWriteLine`), so a second input source needs its own instance: a refactor to `(Stream&, BridgeLineState&)` or a small struct.
- **Must not be copied.** The input loop calls `otgwStreamRxNote()` (`OTDirect.ino:733`), the LAN witness for the WiFi dead-link check (TASK-1450). USB bytes must not feed it, or a dead WiFi link would look alive.
- **Execution context.** Bridge writes run in `loop()` context (`loopOTDirect()` from `doBackgroundTasks()`, `OTGW-firmware.ino:1075-1076`), on the same path that answers the thermostat in time. A USB write there must never block. `HWCDC` defaults to `tx_timeout_ms = 100` (`HWCDC.cpp:48`), so a host that opens the port and stops reading could stall each write by up to 100 ms. A USB sink needs either `setTxTimeoutMs(0)` or the existing non-blocking low-level writer.

## 5. Options

Code sizes below are **estimates** from reading the code, not measurements. Flash headroom is a **fact**: app slot `0x230000` = 2,293,760 bytes (`partitions_otgw_esp32.csv:21`), the beta.141 combo app image is 2,123,616 bytes (`OTGW-build-archive/2.0.0-beta.141+963b68d.zip`), so about 170 KB (7.4 %) is free.

### Option A: TCP 25238 only, documented

- **How:** no firmware change. Document in the manual and the OT-Direct notes that OTmonitor connects with connection type TCP, host `<gateway>`, port 25238 (OTmonitor supports it natively, `otmonitor.tcl:1501-1508`). Mention the `bLegacyPort25238Enabled` setting.
- **Cost:** a docs change only.
- **Flashing, safeboot, recovery, boot log:** unchanged.
- **Risks:** none new. Port 25238 is unauthenticated on the LAN, as today.
- **User benefit:** OTmonitor works today over WiFi or Ethernet. It does not cover "no network" or "WiFi is down" debugging. For those cases the web UI's recovery paths and the USB boot log stay available.

### Option B: the one USB CDC carries the line protocol in OT-Direct mode

- **How:** after board detection resolves OT-Direct, and only when a new setting (default off) asks for it, the console hooks stop writing to USB: `putc2` gets a null writer and `esp_log_set_vprintf` gets a null or telnet-bound writer. The bridge output then also goes to the USB FIFO, and a USB line assembler feeds `sendPICSerial()`.
- **Implementation choice:** use the same low-level FIFO calls that `platformUsbJtagPutc` already uses, plus a non-blocking RX poll in `loop()`. Do not call `HWCDC::begin()`: it adds an ISR and ring buffers next to our direct FIFO writer, and `HWCDC::end()` calls `setDebugOutput(false)`, which installs `putc1(NULL)` and `putc2(NULL)` (`HWCDC.cpp:387, 607-614`), silently undoing `platformMuteUart0Console()`. All of this goes into `platform_esp32.h` shims behind a `HAS_*` flag (firmware-coding rules).
- **Code (estimate):** 80 to 150 lines of firmware (two shims, bridge refactor, setting, mode hook), plus a settings UI toggle, docs and host tests. Flash under 2 KB, RAM about 200 bytes (a second 128-byte line buffer). Both are estimates.
- **USB flashing:** unchanged in principle, because download and reset stay in hardware (USJ). Bench check still needed: esptool while the app polls the RX FIFO.
- **Safeboot:** unchanged (separate image, still prints its status lines).
- **Recovery and boot log:** the ROM banner, bootloader and pre-detection `log_e` still appear on USB at every boot (`sdkconfig:538, 2309`). After detection the console is gone from USB. Our capture scripts and the "plug in USB and read the boot log" troubleshooting step then only work until detection, unless the setting is off. A panic backtrace still reaches USB through the secondary console, and OTmonitor shows it as noise (and may take a line of it as a command answer, section 3).
- **Telnet debug:** unaffected (`Debug*` already goes to telnet only). Moving `esp_log` output to telnet would be a bonus but must stay non-blocking and safe from any task.
- **Testability:** host tests can cover the shared line assembler and the "no `otgwStreamRxNote()` from USB" rule. The reset-on-open behaviour needs a bench with Windows, plus macOS or Linux if they are supported.
- **Risks:**
  - Reset on port open or close (section 7, first item). This is the deciding unknown.
  - Linux ModemManager may probe a new `/dev/ttyACM*` with `AT` commands. Our parser would answer them as unknown commands. Not verified for VID 303A.
  - A terminal user who opens the port to read logs now types into the OT-Direct command path. This needs physical access, so the risk is lower than with port 25238.
  - Governance: changes the ADR-168 console contract (new ADR) and conflicts with the wording of the core rule about `Serial`.
- **User benefit:** OTmonitor over a cable, no network needed. On PIC-less hardware this is mainly a comfort feature: there is no PIC to flash over the cable.

### Option C: runtime mode switch on the one CDC (debug vs protocol)

- **How:** like B, but switchable at runtime: a telnet key, a REST call, or an in-band escape on the USB line itself (for example a magic line that flips modes).
- **Code (estimate):** B plus 40 to 80 lines for the switch, its state and its UI. An in-band switch needs a parser that is safe against OTmonitor's own traffic.
- **Extra risks over B:** two protocols on one port with no handshake. OTmonitor cannot tell which mode it is in, and unknown lines are consumed as command answers (section 3). An automatic switch ("protocol when a host opens the port") cannot be built reliably: `HWCDC::isConnected()` only knows that the host polled the IN endpoint (`HWCDC.cpp:86-95, 161-180`), not which program is listening.
- **User benefit:** same as B, plus logs on demand. In practice B with a setting gives the same result with less state.

### Option D: TinyUSB composite device with a second CDC

- **How:** build with `ARDUINO_USB_MODE=0`, expose two CDC ports (console and OTGW protocol), and leave the USB-Serial-JTAG controller unused at runtime.
- **Code (estimate):** firmware glue of 100 to 200 lines, but the TinyUSB stack, USBCDC and descriptors come from the precompiled libs. My estimate is 20 to 40 KB flash and several KB of RAM. Not measured: this would need a build.
- **USB flashing:** auto-download then depends on the running app (1200-baud touch or `enableReboot`, section 2). An app that hangs or boot-loops before TinyUSB starts has no USB device, so recovery needs GPIO0 plus a reset by hand. The device enumerates with a different PID in the app than in safeboot and in the ROM loader. Windows assigns different COM numbers, and `flash_otgw.bat:188-198` only filters `1001` and `4001`. Whether our app's TinyUSB PID would be `4001` is not verified.
- **Safeboot:** stays on USJ (`safeboot/platformio.ini:29-30`), so the port changes identity when the unit drops into safeboot.
- **Boot log and panics:** a panic may not reach USB at all (IDF usb-otg-console limitations).
- **Risks:** highest of all options: recovery, a hardware-level change to the one cable users have, host driver differences for composite CDC devices, and every combo board (Classic-S3 too) is affected, because the USB mode is a compile-time flag of the one image.
- **User benefit:** both logs and protocol at the same time. That is the only gain over B, and it does not justify the recovery cost.

### Comparison

| | A | B | C | D |
|---|---|---|---|---|
| Firmware code (estimate) | 0 | 80-150 lines | 120-230 lines | 100-200 lines + TinyUSB |
| Flash (estimate) | 0 | < 2 KB | < 3 KB | 20-40 KB |
| USB flashing | unchanged | unchanged (bench check) | unchanged (bench check) | depends on the app |
| Safeboot | unchanged | unchanged | unchanged | port changes identity |
| Boot log on USB | full | until detection | per mode | on CDC 0 |
| Panic output on USB | yes | yes, mixed into the protocol | yes, mixed in | maybe not |
| New ADR | no | yes | yes | yes |
| Reset-on-open risk | none | open, bench needed | open, bench needed | different, open |

## 6. Recommendation

1. **Now:** option A. Write the OTmonitor-over-TCP steps into the manual and answer tjfs with them. This costs nothing and covers every user who has the gateway on the network.
2. **Only if Robert wants USB anyway:** option B as an opt-in setting, default off, active only in OT-Direct mode, implemented with the existing low-level FIFO path in `platform_esp32.h`, with its own ADR that supersedes the relevant part of ADR-168 and a reworded core rule. The gate before any code is written: a bench test of OTmonitor connect and disconnect, 10 times on Windows, watching the gateway uptime. If it resets even once, B is not viable as designed.
3. **Not:** option C (two protocols on one port; OTmonitor takes stray lines as answers) and option D (recovery depends on a healthy app, the port changes identity, and every combo board is affected).

On the Classic with an S3 module the USB port is not the PIC line either. A classic OTGW user who expects "USB = PIC serial" will not find it there under any option above. That is an observation, not a second question.

## 7. Not verified

- **Reset on port open or close.** The USJ controller resets the chip in the "RTS set, DTR clear" state (esptool `reset.py:125-143`). Tcl on Windows leaves DTR/RTS as the driver had them (`tclWinSerial.c`). An mpremote report says Windows clears DTR before RTS on close, which passes through the reset state (secondary source, a different tool). The comment in `scripts/capture-serial.py:100-104` says a plain DTR/RTS pulse does *not* reset the USJ device, and that script opens the port with pyserial without resetting the unit. That is one project-internal observation, not a test of OTmonitor. **Bench recipe:** OTmonitor 6.6 on Windows, connect and disconnect 10 times to the USB COM port, record `/api/v2/device/info` uptime before and after each cycle; repeat on Linux (`/dev/ttyACM0`) if Linux users are in scope.
- Whether OTmonitor on Windows opens a USJ COM port reliably (driver `usbser.sys`) and whether its `BREAK` (PIC upgrade) does anything on USJ.
- The ROM banner and bootloader lines on USB under option B. Read from sdkconfig, not captured.
- Linux ModemManager probing of VID 303A ports.
- The TinyUSB flash and RAM cost and the PID it would enumerate with (option D). No build was made.
- Whether the GPIO0 boot button on the PIC-less board is reachable without opening the case.
- The panic-output path under B (whether the panic handler writes to USJ through our muted hooks or directly). Read from config, not tested.
- tjfs's actual use case (no network, or habit from the classic OTGW). The Discord message was not re-read in this session; the task description was used.

## 8. Sources

Project code (dev `4f7fd7742`):
- `platformio.ini:111, 161-170, 267-268, 298-299`
- `partitions_otgw_esp32.csv:20-21`
- `src/libraries/Platform/src/platform_esp32.h:695-746`
- `src/libraries/Platform/src/boards.h:50-58`
- `src/libraries/OTGWSerial/OTGWSerial.cpp:872-895`
- `src/OTGW-firmware/OTGW-firmware.h:85`
- `src/OTGW-firmware/OTGW-firmware.ino:265-277, 471, 1075-1076`
- `src/OTGW-firmware/OTGW-Core.h:33`
- `src/OTGW-firmware/OTGW-Core.ino:4385-4393`
- `src/OTGW-firmware/OTDirect.ino`: 678, 695, 714-769 (733 `otgwStreamRxNote`, 749 `sendPICSerial`), 775, 2278, 2291, 2773
- `src/OTGW-firmware/debugStuff.h:25-27, 70-82`
- `safeboot/platformio.ini:29-30`, `safeboot/src/main.cpp:15-16, 176-226`
- `flash_otgw.bat:188-198, 226-233`
- `scripts/capture-serial.py:96-120`
- `docs/hardware/PINOUT.md:83`
- `docs/adr/ADR-168-pic-serial-native-uart0-and-idf-console-mute.md`
- Build archive `OTGW-build-archive/2.0.0-beta.141+963b68d.zip` (image sizes)

Pinned toolchain (local PlatformIO packages):
- arduino-esp32 3.3.5: `cores/esp32/HardwareSerial.h:430-436`, `HWCDC.h:95-115`, `HWCDC.cpp:48, 80-95, 161-180, 305-392, 607-614`, `main.cpp:52-67, 99-100`, `chip-debug-report.cpp:292-294`, `USBCDC.cpp:282-283, 324`, `esp32-hal-tinyusb.c:132-143, 523-602`
- ESP-IDF libs 5.5.1 `esp32s3/sdkconfig:538, 669, 2303-2314`
- esptool 5.1.0 `esptool/reset.py:125-143`

Reference implementation:
- `other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl:423-434, 436-460, 467-489, 541-583, 1501-1538, 1633`
- `other-projects/otmonitor-6.6/otmonitor.vfs/upgrade.tcl:303-325`

Vendor docs and upstream:
- ESP-IDF v5.5.1, USB Serial/JTAG Controller Console (ESP32-S3): https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/usb-serial-jtag-console.html
- ESP-IDF v5.5.1, USB OTG Console (ESP32-S3): https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/usb-otg-console.html
- esptool, Boot Mode Selection (ESP32-S3): https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html
- arduino-esp32, USB CDC API: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb_cdc.html
- Tcl 8.6 `win/tclWinSerial.c`: https://raw.githubusercontent.com/tcltk/tcl/core-8-6-branch/win/tclWinSerial.c
- Secondary (context only): esp32.com forum threads on DTR/RTS reset (https://esp32.com/viewtopic.php?p=151736), arduino-esp32 issue #9582 (https://github.com/espressif/arduino-esp32/issues/9582)

## 9. The question for Robert

**Should the USB port on PIC-less hardware carry the OTGW line protocol for OTmonitor?**

- **A. No, TCP 25238 only, documented (recommended).** Consequence: a docs change only; the USB port stays the diagnostic console; tjfs gets the TCP connection steps; TASK-1720 closes as "not planned" with this report as the reason.
- **B. Yes, as an opt-in setting (default off, OT-Direct mode only), gated on a bench test first.** Consequence: a 30-minute bench test of OTmonitor connect and disconnect on Windows before any code. If there is no reset: about 80 to 150 lines (estimate), a new ADR that amends ADR-168, the core rule "never write to `Serial` after init" reworded to "never write to the PIC UART", and a USB console that goes quiet after board detection whenever the setting is on. If the bench shows a reset, B stops there.
- **D. Yes, with a TinyUSB second CDC port.** Consequence: logs and protocol at the same time, but auto-flash and recovery depend on a running app, the port changes identity between app, safeboot and ROM loader, 20 to 40 KB flash (estimate), and every combo board changes its USB device. Not recommended.
