# ADR-168 PIC Serial Link Binds Native UART0 on GPIO43/44; IDF Console Muted on the PIC Path

## Status

Proposed. Date: 2026-07-09.

This ADR documents a diagnosis-driven decision already implemented and
verified on-device (commits ad7334846 / alpha.336 and 8118d29c /
alpha.337, TASK-972). It does NOT flip to Accepted here: acceptance is
the maintainer's own determination, made later, per project rule.

## Status History

status_history:
  - date: 2026-07-09
    status: Proposed
    changed_by: Agent (adr-generator, diagnosis-driven)
    reason: Documents the UART0 rebind and IDF console mute that restored the ESP-to-PIC command channel on esp32-classic, verified on the bench device 2026-07-08/09 (TASK-972). Maintainer acceptance pending.
    changed_via: adr-kit

## Context

On the OTGW Classic carrier board, the physical socket wires the PIC's
UART lines to GPIO43 (ESP TX to PIC RX) and GPIO44 (ESP RX from PIC TX)
of the ESP32-S3 (`src/libraries/Platform/src/boards.h:166-167`,
`PIN_PIC_TX=43` / `PIN_PIC_RX=44`). These two pins are the ESP32-S3's
native UART0 IO_MUX pins.

Before this decision, the vendored `OTGWSerial` driver bound **UART1**
on those pins, routed through the GPIO matrix. On the esp32-classic
bench device this produced an asymmetric failure, verified on-device
2026-07-08/09 (TASK-972):

- PIC to ESP RX worked perfectly: OT frames and reset banners arrived
  without loss.
- ESP to PIC TX was structurally broken: every `PR=` / `GW=` command
  went silently unanswered and was retried forever, and the PIC-flash
  path could not even reset the PIC via `GW=R`.

The asymmetry went unnoticed for a while because a banner-matching
callback (`fwreportinfo`) fires on RX-only reset banners, which looked
like command responses in the logs.

A second, independent problem surfaced during the same diagnosis: the
Arduino-ESP32 framework's prebuilt sdkconfig sets the **primary** IDF
console to UART0 (`CONFIG_ESP_CONSOLE_UART_NUM=0`, 115200 baud;
`CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=1`, so USB-Serial-JTAG is
only the SECONDARY console). `ARDUINO_USB_CDC_ON_BOOT=1` (TASK-850)
only redirects Arduino's `Serial` object; `esp_log` / `ets_printf` /
WiFi-driver output still transmits physically out of UART0, straight
into the PIC. The PIC answered stray log lines with `SE` (syntax
error) responses, and any log byte landing inside the PIC-flash
bootloader's STX window fatally aborts that handshake.

Terminology: IO_MUX is the ESP32-S3's direct pin-to-peripheral mux
(native routing); the GPIO matrix is the flexible any-signal-to-any-pin
crossbar that UART1 must use to reach GPIO43/44. IDF is Espressif's
ESP-IDF framework underlying the Arduino core; STX/ETX are the
start/end-of-text control bytes framing the PIC bootloader protocol.

## Decision

On the PIC path (esp32-classic and combo-with-PIC), the PIC serial link
binds the ESP32-S3's **native UART0** on GPIO43/44, and the IDF console
output on UART0 is **muted at runtime** before the PIC link starts.

Concretely:

1. `OTGWSerial`'s ESP32 constructor branch derives from
   `HardwareSerial(0)` instead of `HardwareSerial(1)`
   (`src/libraries/OTGWSerial/OTGWSerial.cpp:855`; commit ad7334846,
   alpha.336). Verified on-device: `PR=A` answers in under 50 ms and
   `GW=R` resets the PIC (reset banner observed +57 ms).
2. A new `platformMuteUart0Console()` shim
   (`src/libraries/Platform/src/platform_esp32.h:327`) installs a null
   vprintf via `esp_log_set_vprintf()` and a no-op putc via
   `ets_install_putc1()`. It is called in `setup()` on the PIC path
   (`src/OTGW-firmware/OTGW-firmware.ino:359`) before
   `OTGWSerial.begin()` (commit 8118d29c, alpha.337).

Scope: this decision covers the runtime command channel only. It does
NOT claim the PIC-flash bootloader handshake works; see the open issue
in Consequences.

## Alternatives Considered

### Alternative A: Keep UART1 via the GPIO matrix (status quo)

Leave `OTGWSerial` on `HardwareSerial(1)` routed through the GPIO
matrix onto GPIO43/44.

Rejected empirically: on this hardware ESP-to-PIC TX was demonstrably
broken in that configuration (every command unanswered, PIC reset via
`GW=R` impossible), while binding the native UART0 restored TX
immediately with no other change.

### Alternative B: UART1 on different GPIOs

Move the PIC link to pins where UART1 routing is unproblematic.

Impossible: the Classic carrier's physical socket fixes the PIC lines
to GPIO43/44 (board pinout; `boards.h` `PIN_PIC_RX=44` /
`PIN_PIC_TX=43`). There is no other pin choice without a hardware
revision.

### Alternative C: Make USB-JTAG the primary IDF console via sdkconfig

Rebuild with `CONFIG_ESP_CONSOLE` pointed at USB-Serial-JTAG so no log
output targets UART0 in the first place.

Not available: the Arduino-ESP32 core ships a prebuilt sdkconfig; the
only way to change it is a custom framework build, which this project
does not maintain. Runtime hooks (`esp_log_set_vprintf`,
`ets_install_putc1`) are the only available knob and achieve the same
silence for all post-boot output.

### Alternative D: Do nothing

Rejected: the ESP-to-PIC command channel was dead on esp32-classic,
which makes gateway-mode control, PIC configuration, and the ser2net
bridge write path unusable. Unacceptable for a product whose core job
is talking to the PIC.

## Consequences

**Benefits**

- All ESP-to-PIC traffic works: commands (`PR=A` answered in under
  50 ms), gateway-mode control (`GW=R` PIC reset, banner +57 ms), and
  ser2net bridge writes.
- No IDF/WiFi log bytes reach the PIC anymore. Previously these caused
  spurious `SE` responses and fatally abort the PIC-flash bootloader's
  STX window if they land inside it.

**Trade-offs**

- The UART0 console is no longer usable for ESP-side debugging on PIC
  boards. USB-CDC (`Serial`) and telnet debug (port 23) remain the
  debugging channels, consistent with the long-standing project rule
  that the PIC serial link is never written to for debug output.

**Risks and mitigations**

- *Risk (accepted residual)*: the ROM first-stage boot log at ESP reset
  still reaches the PIC at 115200 baud. This is eFuse-level behaviour
  that runtime hooks cannot suppress. *Mitigation*: harmless in
  practice, because `detectPIC()` resets the PIC after ESP boot, so any
  PIC-side confusion from ROM boot bytes is cleared before the link is
  used.
- *Risk / known open issue (disclosed honestly)*: the PIC-flash
  bootloader handshake STILL fails after both fixes. A deterministic
  non-STX byte appears on the PIC RX line less than 130 ms after the
  bootloader ETX, even with a verified-silent TX line; a hardware
  measurement is pending. *Mitigation*: tracked under TASK-972; this
  ADR fixes the command channel and does not claim the flash path
  works.
- *Risk*: a future contributor reverts `OTGWSerial` to
  `HardwareSerial(1)` (for example during an upstream sync of the
  vendored driver), silently re-breaking TX. *Mitigation*: the
  Enforcement block below forbids that pattern in the file.

## Related Decisions

- **ADR-130 (PIC-UART Dedicated FreeRTOS Task as Sole OTGWSerial
  Owner)**: complements. This ADR changes WHICH UART peripheral the
  sole owner drives; the ownership/park contract of ADR-130 is
  untouched.
- **ADR-138 (PIC-UART Control-Method Park Handshake, TX
  Requeue-to-Front Ordering, and Progress-Path Heap-Gate Decision)**:
  complements. The park handshake and TX ordering invariants apply
  unchanged on UART0.
- **ADR-158 (Combo board: LOLIN S3 Mini Pro as a third boot-detected
  Classic variant)**: depends on. The combo pin map fixes the PIC lines
  to GPIO43/44, which is the physical constraint that rules out
  Alternative B.

## References

- TASK-972 (PIC flash over REST; the diagnosis that surfaced both
  findings) and TASK-850 (`ARDUINO_USB_CDC_ON_BOOT=1`, which moves only
  Arduino's `Serial`, not the IDF console).
- Commit ad7334846 (alpha.336): UART0 rebind,
  `src/libraries/OTGWSerial/OTGWSerial.cpp:855`.
- Commit 8118d29c (alpha.337): console mute,
  `src/libraries/Platform/src/platform_esp32.h:327` and
  `src/OTGW-firmware/OTGW-firmware.ino:359`.
- Pin map: `src/libraries/Platform/src/boards.h:166-167` (Classic) and
  `:262-263` (combo variant).
- Framework sdkconfig facts: `CONFIG_ESP_CONSOLE_UART_NUM=0` (primary,
  115200), `CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=1` (USB is
  secondary only), from the prebuilt Arduino-ESP32 core sdkconfig.
- On-device verification 2026-07-08/09, esp32-classic bench device.

## Enforcement

Declarative rule for the mechanically checkable half (the UART binding
regression is a one-line revert that a regex catches deterministically),
plus `llm_judge` for the semantic half (the console-mute call must stay
on the PIC path before `OTGWSerial.begin()`, an ordering/call-graph
property a regex cannot express). Rule for the judge: the PIC serial
link must use native UART0, and `platformMuteUart0Console()` must be
called on the PIC path before the PIC serial link starts.

```json
{
  "forbid_pattern": [
    {"pattern": "HardwareSerial\\(1\\)", "path_glob": "src/libraries/OTGWSerial/OTGWSerial.cpp",
     "message": "PIC link must bind native UART0 (HardwareSerial(0)) on GPIO43/44; UART1 via the GPIO matrix breaks ESP->PIC TX on this hardware (ADR-168)."}
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": true
}
```
