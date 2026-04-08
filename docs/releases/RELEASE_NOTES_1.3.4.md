# OTGW-firmware v1.3.4 Release Notes
**Release date:** 2026-04-01
**Branch:** main (from dev)
**Compare:** [v1.3.3...v1.3.4](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.3...v1.3.4)

## Overview

v1.3.4 is a bugfix and minor improvement release. It fixes a bug where MQTT throttle slots could permanently suppress stable sensor values after a transient publish failure, adds missing tooltips to the Debug Information page, renames "OTGW Connected" to "OpenTherm Active" for clarity, and adds thermostat-only MQTT support so the OTGW stays online without a boiler connected.

## Bug fixes

- **MQTT throttle slot fix:** Stable values like Room Temperature could become permanently suppressed after a transient MQTT publish failure. The throttle slot is now only updated after a successful publish, preventing values from being silently dropped.
- **Debug Information page tooltips:** Tooltips were defined in the firmware but never wired up to the device info labels on the Debug Information page. They are now visible on hover.

## Improvements

- **Renamed "OTGW Connected" to "OpenTherm Active":** The label on the Debug Information page now accurately describes what the field indicates (whether OpenTherm communication is active), with an updated tooltip.
- **Thermostat-only MQTT support:** The OTGW now stays online via MQTT when only a thermostat is connected. A boiler is no longer required for MQTT to remain active. This enables use cases like monitoring thermostat data without a boiler attached.

## Breaking changes

No breaking changes vs v1.3.3.

## Upgrade notes

- Flash both firmware and filesystem (frontend changes require filesystem update).
- Hard-refresh the browser after flashing (Ctrl+F5).
- If you rely on the "OTGW Connected" label in automations or scripts, update references to "OpenTherm Active".
