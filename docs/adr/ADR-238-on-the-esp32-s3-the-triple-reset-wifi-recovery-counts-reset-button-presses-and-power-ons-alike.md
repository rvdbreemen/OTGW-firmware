---
id: "ADR-238"
title: "On the ESP32-S3 the triple-reset WiFi recovery counts reset-button presses and power-ons alike"
status: "Accepted"
date: "2026-11-14"
binding: false
gate: null
documents_shipped: false
verified_in:
  - "src/libraries/Platform/src/platform_esp32.h:platformIsResetButtonOrPowerOn"
  - "src/OTGW-firmware/OTGW-firmware.ino:isPortalResetGesture"
  - "test/host/test_wifi_portal_reset.py"
supersedes:
  - "ADR-043"
superseded_by: null
topics:
  - "WiFi recovery"
  - "triple reset"
  - "config portal"
  - "reset reason"
aliases:
  - "reset button WiFi portal"
  - "pull the plug three times"
components:
  - "WiFi recovery trigger"
symbols:
  - "platformIsResetButtonOrPowerOn"
  - "isPortalResetGesture"
  - "shouldForceWifiConfigPortal"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-238 On the ESP32-S3 the triple-reset WiFi recovery counts reset-button presses and power-ons alike

## Status

Accepted, 2026-11-14.

## Status History

```yaml
status_history:
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-14
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: "Superseded 2026-11-14 by ADR-238 (Robert in session: 'All ADR proposed 233, 238 and 239 should be accepted. So yes.')"
    changed_via: adr-kit lifecycle
  - date: 2026-11-14
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Accepted 2026-11-14 by Robert in session: 'All ADR proposed 233, 238 and 239 should be accepted. So yes.'"
    changed_via: adr-kit lifecycle
```

## Context

ADR-043 (Accepted, written for the ESP8266) recovers WiFi with a triple reset: three external resets within a
10-second window clear the saved WiFi credentials and force the WiFiManager config portal. The count lives in
persistent storage (`platformRtcWrite`, NVS (non-volatile storage) on the ESP32-S3), slot 96, with a magic value
(`OTGW-firmware.ino`, `WIFI_PORTAL_RESET_*`).

On the ESP32-S3 the gesture never fires. `shouldForceWifiConfigPortal()` counted a boot only when
`esp_reset_reason() == ESP_RST_EXT` (`platform_esp32.h:platformIsExternalReset`), and the S3 never reports that
value. The S3 hardware gives the EN pin (the reset button), a power-on, a chip brown-out and the super watchdog one
and the same raw reset code, 0x01 (pinned libs `soc/esp32s3/include/soc/reset_reasons.h:38-40`:
`RESET_REASON_CHIP_POWER_ON`, `_CHIP_BROWN_OUT`, `_CHIP_SUPER_WDT`, all 0x01), which ESP-IDF reports as
`ESP_RST_POWERON`. Software cannot tell a press of the reset button from a power-on. The bench confirmed the code
table on two units (TASK-1904: a task watchdog gives raw 0x0c, an RTC (real-time clock) watchdog 0x09). A USB (Universal Serial Bus) reset from a host
reports `ESP_RST_USB` (TASK-1930 notes, otgw-firmware-09), not a power-on.

A forced portal is destructive: it calls `manageWiFi.resetSettings()` and wipes the stored WiFi credentials
(`networkStuff.ino`, the `forcePortal` branch of `startWiFi()`, ADR-043 / TASK-760). Any reset kind that counts can
therefore erase the WiFi.

## Decision

On the ESP32-S3 a boot counts towards the triple-reset WiFi recovery when it is a reset-button press or a power-on
(`ESP_RST_POWERON`, raw 0x01) or an external reset (`ESP_RST_EXT`); everything else of ADR-043 stays: 3 counted
boots, each within 10 s of the boot before, clear the WiFi credentials and force the config portal.

User Decision (Robert, Discord DM 1571125618777522260, 2026-11-14, answer A to TASK-1930's question): count every
power-on-type boot, accepting that three power interruptions inside the windows also wipe the WiFi. Pulling the
plug three times is therefore a supported route too, which matters because it is not known whether every Classic
PCB (printed circuit board) revision has a reset or config button fitted (Robert, DM 1571125777720672267: "Not sure").

## Decision Contract

### Must

- Count a boot when `esp_reset_reason()` is `ESP_RST_POWERON` or `ESP_RST_EXT`, through the platform shim
  `platformIsResetButtonOrPowerOn()`; the firmware calls it only from `isPortalResetGesture()`.
- Keep ADR-043's trigger count (3), window (10 s, cleared by `loop()` when it expires), magic value and the
  credential wipe before the portal.
- Name the gesture as "press reset, or pull the plug, three times" in the user documentation and the OLED (organic light-emitting diode display) hint.

### Must Not

- Count software resets (OTA, `/ReBoot`, the WiFi supervisor restart of ADR-211), panics, watchdog resets,
  `ESP_RST_BROWNOUT` (raw 0x0F, system brown-out) or `ESP_RST_USB`.
- Read the reset reason with a raw `#ifdef ESP32` outside the Platform library.

### Exceptions

- None.

### Verification

- `python test/host/test_wifi_portal_reset.py` compiles the real gate and counter: three reset-button boots force
  the portal (P1), three power-ons force it (P5), software resets (P2) and boots outside the window (P3) do not;
  red on the code before TASK-1930 (P1, P5).
- Bench: not possible on the bench units over USB (a USB reset reports `ESP_RST_USB`); a power cycle or button press
  by the maintainer or in the field is the hardware check (TASK-1930).

## Alternatives Considered

- **B. Count power-ons only when the boot before ran less than about 3 s.** Rejected by Robert's choice of A: it
  keeps the same false-trigger risk with a smaller window, and three presses within 3 s each are harder for users.
- **C. A new gesture: hold the board button for at least 5 s while the firmware runs.** Rejected: the button pin
  differs per board (GPIO0 on the OTGW32, which is a strapping pin, so it cannot be held during reset; GPIO18 or
  GPIO13 on the Classic boards), it is set up today only when an OLED is found, and it is not known whether every
  Classic revision has the button. The agent recommended C; Robert chose A.
- **D. Leave the triple reset dead on the S3 and point users at the web UI.** Rejected: a gateway with wrong WiFi
  credentials has no web UI to reach, so there is no reset-only recovery left.

## Consequences

**Positive:**

- The documented recovery gesture works again on every ESP32-S3 board, with or without a button: press reset or
  pull the plug three times, each within 10 s of the boot before.
- One small platform shim; ADR-043's counter, window and wipe stay unchanged.

**Negative:**

- Three power interruptions, each within 10 s of the boot before (a flickering supply, a plug that is wiggled, a
  power cut that restores and drops twice), wipe the stored WiFi credentials and leave the gateway in the portal
  until someone configures it. Robert accepted this risk. Mitigation: the 10-second window, so a single power cut or
  slow repeated cuts do not count; the portal times out after 240 s (`startWiFi(..., 240, ...)`).
- A chip brown-out (raw 0x01) and a super-watchdog reset also count, since they share the code; a system brown-out
  (0x0F) does not.
- Every power-on now writes the counter to NVS (count 1) and clears it again after 10 s: two NVS writes per power-on,
  which is rare compared with the flash endurance.
- The OTGW32 and Classic documentation and the OLED hint change from "triple reset" to "reset or power, three times".

## Open Questions

- None.

## Related Decisions

- **ADR-043 (Reset-Pattern WiFi Recovery Trigger)**: superseded on the 2.x line; this ADR keeps its mechanism and
  changes which resets count.
- **ADR-211 (Skip the WiFi config portal on the boot after a WiFi supervisor restart)**: a supervisor restart is a
  software reset and does not count here.

## References

- TASK-1930 (the dead gate, Robert's decision, the harness); TASK-1904 (the raw reset code shim, bench raw codes).
- `src/libraries/Platform/src/platform_esp32.h:platformIsResetButtonOrPowerOn`
- `src/OTGW-firmware/OTGW-firmware.ino:isPortalResetGesture`, `shouldForceWifiConfigPortal`
- `src/OTGW-firmware/networkStuff.ino`: the `forcePortal` branch of `startWiFi()` (credential wipe)
- Pinned libs `framework-arduinoespressif32-libs/esp32s3/include/soc/esp32s3/include/soc/reset_reasons.h:38-40`
- `test/host/test_wifi_portal_reset.py`
