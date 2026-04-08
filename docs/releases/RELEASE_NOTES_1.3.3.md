# OTGW-firmware v1.3.3 Release Notes

**Release date:** 2026-03-31
**Branch:** main (from dev)
**Compare:** [v1.3.2...v1.3.3](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.2...v1.3.3)

## Overview

v1.3.3 adds support for running the firmware without a PIC microcontroller and fixes the dashboard showing empty or zero values for unsupported OpenTherm message IDs.

## New features

- **PIC-less OTGW support** (PR #522): All PIC-dependent functions (serial commands, MQTT topics, HA discovery, REST API endpoints, firmware flash, UI elements) are now automatically disabled when no PIC is detected at boot. A central `isPICEnabled()` guard protects all code paths. If the PIC appears later (e.g., delayed startup), it is automatically re-detected via banner detection and all functions re-enable without a reboot. The REST API returns HTTP 503 for PIC endpoints when no PIC is present. The Web UI hides PIC-related elements (firmware menu, gateway mode indicator, command bar, boot command settings, PIC device info) using a CSS `pic-only` class driven by `applyPICAvailability()`.

## Bug fixes

- **Dashboard no longer shows unsupported OT values** (`ae4487a`): Previously, the dashboard displayed empty or zero values for OpenTherm message IDs that the boiler doesn't support or the thermostat never queries (e.g., DHW temperature, Max CH Water setpoint on some boilers). The firmware now only records message timestamps for valid responses (`is_value_valid()`), and the REST API only includes OT monitor entries that have actually received valid data. Reported by chielh on Discord.

- **Gateway mode detection fix** (PR #528): Gateway mode polling (`PR=M`) now checks `isGatewayFirmware()` in addition to PIC availability, so non-gateway PIC firmware correctly shows "N/A" instead of continuously polling an unsupported command.

- **"Home Assistant Integration" label renamed to "OTGW Connected"** (PR #528): The `otgwconnected` field in the device info page was misleadingly labeled "Home Assistant Integration". It actually indicates whether the OTGW is online (thermostat and/or boiler communication detected). Both the label and tooltip have been corrected.

## Architecture

- **ADR-060: PIC Availability Guard Pattern** — New ADR documenting the `isPICEnabled()` guard pattern, guarded code paths, auto-recovery mechanism, and design alternatives. Related sections of ADR-031 and ADR-012 updated.

## Breaking changes

There are **no breaking changes** in v1.3.3. All MQTT topics, REST API endpoints, settings format, and ser2net behavior remain identical to v1.3.2. The only behavioral difference is that unsupported OT values are no longer included in the dashboard API response — this is a correctness fix, not a contract change.

## Upgrade notes

- Flash both firmware (.ino.bin) and filesystem (.littlefs.bin).
- Hard-refresh the browser after flashing (Ctrl+F5) to pick up the updated JavaScript.
- If you have a standard OTGW with PIC, behavior is unchanged — the PIC guard is transparent.
- If you run without a PIC, PIC-related UI elements and MQTT topics will automatically disappear.
