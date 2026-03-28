v1.3.1 is a stability release fixing command queue reliability, CS override interference, and serial coordination issues reported after v1.3.0.

[Full release notes](https://github.com/rvdbreemen/OTGW-firmware/blob/main/RELEASE_NOTES_1.3.1.md) | [README](https://github.com/rvdbreemen/OTGW-firmware/blob/main/README.md)

## Bug fixes

- **CS override interference:** Setpoint commands (CS=) from Home Assistant / MQTT are no longer intermittently overridden by the thermostat's lower setpoint. PIC settings readout triggers are now whitelisted to specific commands only.
- **PR command queue matching:** Responses like `PR: S=16.00` now correctly remove `PR=S` from the queue instead of the first PR= entry found. Fixes unnecessary retries and queue slot waste.
- **PR=A banner dequeue:** The PIC banner response is now properly detected and removes `PR=A` from the command queue, preventing 5 unnecessary retries.
- **`strstr_P` crash:** Inverted PROGMEM/RAM arguments in the PIC settings trigger check caused an Exception (2) crash. Fixed.
- **SC= time-sync bypassed queue:** The time synchronization command now goes through the command queue like all other PIC commands, preventing serial bus collisions.
- **UI footer overlap:** The log window no longer overlaps the status bar in Firefox/LibreWolf when stretched to the bottom of the screen.

## Improvements

- **Ser2net awareness:** The command queue now detects traffic from port 25238 (OTmonitor) and pauses queue processing for 2 seconds to avoid conflicting commands. Conflicting queue entries are automatically removed.
- **Non-blocking PIC queries:** PR= settings and gateway mode queries are now fully asynchronous, no longer blocking the HTTP server.
- **Queue code cleanup:** Deduplicated queue removal logic into a single `removeFromCmdQueue()` helper. Consistent char matching throughout.

## Upgrade notes

- No breaking changes vs v1.3.0.
- Flash both firmware and filesystem (CSS fix requires filesystem update).
- Hard-refresh the browser after flashing (Ctrl+F5).

## Thank you

Thanks to the community members who reported the CS override issue and provided detailed logs that helped pinpoint the root cause. Join us on [Discord](https://discord.gg/zjW3ju7vGQ) for support and discussion.
