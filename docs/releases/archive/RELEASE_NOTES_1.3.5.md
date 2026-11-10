# OTGW-firmware v1.3.5 Release Notes
**Release date:** 2026-04-06
**Branch:** main (from dev)
**Compare:** [v1.3.4...v1.3.5](https://github.com/rvdbreemen/OTGW-firmware/compare/v1.3.4...v1.3.5)

## Overview

v1.3.5 fixes a WiFi reconnection regression introduced in v1.3.0 and adds MQTT uptime and firmware version publishing. The WiFi state machine timeout was too aggressive (5 seconds), causing the ESP8266 to repeatedly cancel and restart WiFi association before it could complete. This led to devices going offline and requiring a reboot.

## Bug fixes

- **WiFi reconnection regression (#530):** The `loopWifi()` state machine introduced in v1.3.0 used a 5-second per-attempt timeout. On ESP8266, WiFi association (scan + auth + DHCP) takes 5-10+ seconds, so connections were repeatedly cancelled before completing. The timeout is now 30 seconds (matching v1.2.0 behavior), max retries reduced from 15 to 10 (300s total before reboot). SDK auto-reconnect remains enabled to handle brief glitches transparently.

## New features

- **MQTT uptime and version publishing:** The firmware now publishes uptime and firmware version info to MQTT on connect, providing better visibility into device status for Home Assistant and other MQTT consumers.

## Internal improvements

- **ADR-061 (WiFi Reconnect Timeout Tuning):** New architecture decision record documenting the timeout and retry parameter changes. ADR-047 status updated to Superseded.
- **Non-blocking WiFi reconnect refinements:** Cleaned up the WiFi state machine logic for clarity and maintainability.

## Breaking changes

No breaking changes vs v1.3.4.

## Upgrade notes

- Flash firmware only (no filesystem changes required).
- If your OTGW was experiencing WiFi disconnects or going offline periodically since v1.3.0, this release should resolve the issue.
- Users on v1.2.0 or earlier who experienced no WiFi issues may still benefit from this update for the MQTT improvements.
